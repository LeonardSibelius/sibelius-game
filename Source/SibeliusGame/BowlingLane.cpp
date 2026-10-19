// BowlingLane.cpp — see the header for what this actor is for and what it must never do.

#include "BowlingLane.h"

#include "ProgressionSubsystem.h"   // his best game, for the agents to remember (ChatDeeds.h)
#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"
#include "TableSounds.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace BowlingLaneDetail
{
	const FBowlingLaneLayout& Layout()
	{
		static const FBowlingLaneLayout L = FBowlingLaneLayout::MakeRetroBowl();
		return L;
	}

	// Where the sim's foul line sits in SM_Pist's own space (BowlingSim.cpp, MakeRetroBowl).
	const FVector FoulLineInPist(77.0, -30.0, 0.0);

	constexpr double HookStep = 0.25;
	constexpr double AimLimitRadians = 0.07;   // about 4 degrees either way
	constexpr double AimLineLength = 450.0;    // to just past the arrows
	constexpr double ShapeSize = 100.0;        // the engine's basic shapes
	constexpr double GutterDrop = 5.0;         // cm a gutter ball rides below the lane
	constexpr double PinTipDegreesPerSecond = 420.0;
}

ABowlingLane::ABowlingLane()
{
	PrimaryActorTick.bCanEverTick = true;

	const FBowlingLaneLayout& L = BowlingLaneDetail::Layout();
	Sim = FBowlingSim(L);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PinMesh(TEXT("/Game/Bowling/SM_BowlingPin.SM_BowlingPin"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BallMesh(TEXT("/Game/IE_RetroBowl/Meshes/SM_BowlBall.SM_BowlBall"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GuideMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakeVisualOnly = [](UStaticMeshComponent* Component)
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetGenerateOverlapEvents(false);
	};

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	// SM_Pist runs toward its -Y; a yaw of -90 turns the frame's +X that way.
	Lane = CreateDefaultSubobject<USceneComponent>(TEXT("Lane"));
	Lane->SetupAttachment(SceneRoot);
	Lane->SetMobility(EComponentMobility::Movable);
	Lane->SetRelativeLocationAndRotation(BowlingLaneDetail::FoulLineInPist, FRotator(0.0, -90.0, 0.0));

	Approach = CreateDefaultSubobject<UBoxComponent>(TEXT("Approach"));
	Approach->SetupAttachment(Lane);
	Approach->SetMobility(EComponentMobility::Movable);
	Approach->SetBoxExtent(FVector(40.0, L.LaneHalfWidth + L.GutterWidth, 60.0));
	Approach->SetRelativeLocation(FVector(-45.0, 0.0, 60.0));
	Approach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Approach->SetCollisionResponseToAllChannels(ECR_Ignore);
	Approach->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Approach->SetGenerateOverlapEvents(false);

	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetupAttachment(Lane);
	MakeVisualOnly(Ball);
	if (BallMesh.Succeeded())
	{
		Ball->SetStaticMesh(BallMesh.Object);
	}

	PinMeshes.SetNum(FBowlingSim::PinCount);
	for (int32 I = 0; I < FBowlingSim::PinCount; ++I)
	{
		UStaticMeshComponent* Pin = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Pin%02d"), I + 1));
		Pin->SetupAttachment(Lane);
		MakeVisualOnly(Pin);
		if (PinMesh.Succeeded())
		{
			Pin->SetStaticMesh(PinMesh.Object);
		}
		PinMeshes[I] = Pin;
	}

	AimLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimLine"));
	AimLine->SetupAttachment(Lane);
	MakeVisualOnly(AimLine);
	AimLine->SetCastShadow(false);
	if (CubeMesh.Succeeded())
	{
		AimLine->SetStaticMesh(CubeMesh.Object);
	}
	if (GuideMaterial.Succeeded())
	{
		AimLine->SetMaterial(0, GuideMaterial.Object);
	}
	AimLine->SetVisibility(false);

	LaneCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("LaneCamera"));
	LaneCamera->SetupAttachment(Lane);
	LaneCamera->SetFieldOfView(55.0f);

	auto MakeLight = [this](const TCHAR* Name, const FVector& Where, float Lumens, float Radius)
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
		Light->SetupAttachment(Lane);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetRelativeLocation(Where);
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->SetIntensity(Lumens);
		Light->SetAttenuationRadius(Radius);
		Light->SetLightColor(FLinearColor(1.0f, 0.97f, 0.92f));
		Light->SetCastShadows(true);
		return Light;
	};
	// Above and a little in front of the rack, where the pinsetter's lamps are.
	DeckLight = MakeLight(TEXT("DeckLight"), FVector(L.HeadPinX + 20.0, 0.0, 130.0), 250.0f, 420.0f);   // 2500 whited the rack out
	ApproachLight = MakeLight(TEXT("ApproachLight"), FVector(-60.0, 0.0, 180.0), 120.0f, 320.0f);

	FitMeshes();
	SyncPins();
	SyncBall(0.0f);
	SyncCamera(0.0f, true);
}

FVector ABowlingLane::SimToLocal(FBowlingVec2 P, double Height)
{
	// The sim's +Y is the bowler's left; the Lane frame's +Y is his right.
	return FVector(P.X, -P.Y, Height);
}

// The pin mesh was made Y-up and may come in lying down, depending on the importer; the
// ball is the pack's, at whatever size it was modelled. Both are fitted from their bounds
// here, so neither needs fixing by hand in the editor.
void ABowlingLane::FitMeshes()
{
	const FBowlingLaneLayout& L = Sim.Layout();
	if (PinMeshes.Num() > 0 && PinMeshes[0] && PinMeshes[0]->GetStaticMesh())
	{
		const FBox Box = PinMeshes[0]->GetStaticMesh()->GetBoundingBox();
		const FVector Size = Box.GetSize();
		const int32 Axis = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
		// The pin's base is at the mesh origin, so its body lies along whichever way the
		// long axis runs from zero.
		const double Centre = Box.GetCenter()[Axis];
		FVector Along = FVector::ZeroVector;
		Along[Axis] = Centre >= 0.0 ? 1.0 : -1.0;
		PinUpright = FQuat::FindBetweenNormals(Along, FVector::UpVector);
	}
	if (Ball && Ball->GetStaticMesh())
	{
		/* THE PACK'S BALL IS NOT CENTRED ON ITS PIVOT. Spun about the pivot, it orbited it,
		   and Walt's first ball bobbed up and down "as if the lane was some kind of fabric"
		   (2026-09-27). So its true centre is read from the bounds, and every placement
		   puts that centre, turned by the ball's rotation, where the sim says it is. */
		const FBox Box = Ball->GetStaticMesh()->GetBoundingBox();
		const FVector Size = Box.GetSize();
		const double Across = FMath::Max3(Size.X, Size.Y, Size.Z);
		BallMeshScale = Across > 1.0 ? (2.0 * L.BallRadius) / Across : 1.0;
		BallMeshCentre = Box.GetCenter() * BallMeshScale;
		Ball->SetRelativeScale3D(FVector(BallMeshScale));
	}
}

void ABowlingLane::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FitMeshes();
	SyncPins();
	SyncBall(0.0f);
	SyncAimLine();
	SyncCamera(0.0f, true);
}

void ABowlingLane::BeginPlay()
{
	Super::BeginPlay();
	Sim = FBowlingSim(BowlingLaneDetail::Layout());
	FitMeshes();
	SyncPins();
	SyncBall(0.0f);
	SyncCamera(0.0f, true);
}

void ABowlingLane::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlaying();   // whatever tore this down, he gets his controls and his eyes back
	Super::EndPlay(EndPlayReason);
}

bool ABowlingLane::ShouldTickIfViewportsOnly() const
{
	return bDrawLaneInEditor;
}

void ABowlingLane::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!World->IsGameWorld())
	{
		if (bDrawLaneInEditor)
		{
			DrawLane();
		}
		return;
	}

	if (bPlaying)
	{
		UpdateControls(DeltaSeconds);
		if (bCharging)
		{
			ChargeSeconds += DeltaSeconds;
		}
		Sim.Advance(DeltaSeconds);
		HandleEvents();
		if (ASibeliusHUD* Hud = GetHud())
		{
			Hud->HoldCinematic(0.25f);
		}
	}

	SyncBall(DeltaSeconds);
	SyncPins();
	SyncAimLine();
	SyncCamera(DeltaSeconds, false);
	RefreshPanel();
}

void ABowlingLane::Interact_Implementation(AActor* Interactor)
{
	if (bPlaying)
	{
		return;
	}
	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC && GetWorld())
	{
		PC = GetWorld()->GetFirstPlayerController();
	}
	if (PC)
	{
		StartPlaying(PC);
	}
}

FText ABowlingLane::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Bowling", "Prompt", "[E] bowl");
}

void ABowlingLane::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	bCharging = false;
	bStanceLeft = bStanceRight = bAimLeft = bAimRight = bFine = false;
	PendingMouseAim = 0.0f;

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	SyncCamera(0.0f, true);
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);
	EnableInput(PC);
	BindLaneKeys();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Bowling] on the approach (%s)."), *GetActorNameOrLabel());
}

void ABowlingLane::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	bCharging = false;
	Sim.TakeEvents();
	if (AimLine)
	{
		AimLine->SetVisibility(false);
	}
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, 0.4f, VTBlend_Cubic);
		}
		DisableInput(PC);
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ReleaseCinematic();
		}
	}
	PlayingPC.Reset();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Bowling] left the lane."));
}

void ABowlingLane::BindLaneKeys()
{
	// Once: DisableInput keeps the bindings, so binding on every visit would double them.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;

	using FHandler = void (ABowlingLane::*)();
	auto Bind = [this](const FKey& Key, FHandler Pressed, FHandler Released)
	{
		InputComponent->BindKey(Key, IE_Pressed, this, Pressed);
		if (Released)
		{
			InputComponent->BindKey(Key, IE_Released, this, Released);
		}
	};

	for (const FKey& Key : { EKeys::A, EKeys::Gamepad_DPad_Left })
	{
		Bind(Key, &ABowlingLane::OnStanceLeftPressed, &ABowlingLane::OnStanceLeftReleased);
	}
	for (const FKey& Key : { EKeys::D, EKeys::Gamepad_DPad_Right })
	{
		Bind(Key, &ABowlingLane::OnStanceRightPressed, &ABowlingLane::OnStanceRightReleased);
	}
	for (const FKey& Key : { EKeys::Left, EKeys::Gamepad_LeftShoulder })
	{
		Bind(Key, &ABowlingLane::OnAimLeftPressed, &ABowlingLane::OnAimLeftReleased);
	}
	for (const FKey& Key : { EKeys::Right, EKeys::Gamepad_RightShoulder })
	{
		Bind(Key, &ABowlingLane::OnAimRightPressed, &ABowlingLane::OnAimRightReleased);
	}
	for (const FKey& Key : { EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl })
	{
		Bind(Key, &ABowlingLane::OnFinePressed, &ABowlingLane::OnFineReleased);
	}
	for (const FKey& Key : { EKeys::W, EKeys::Up, EKeys::Gamepad_DPad_Up })
	{
		Bind(Key, &ABowlingLane::OnHookMore, nullptr);
	}
	for (const FKey& Key : { EKeys::S, EKeys::Down, EKeys::Gamepad_DPad_Down })
	{
		Bind(Key, &ABowlingLane::OnHookLess, nullptr);
	}
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_RightTrigger })
	{
		Bind(Key, &ABowlingLane::OnBowlPressed, &ABowlingLane::OnBowlReleased);
	}
	for (const FKey& Key : { EKeys::Enter, EKeys::R, EKeys::Gamepad_Special_Right })
	{
		Bind(Key, &ABowlingLane::OnNewGame, nullptr);
	}
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		Bind(Key, &ABowlingLane::OnLeave, nullptr);
	}
	InputComponent->BindAxisKey(EKeys::MouseX, this, &ABowlingLane::OnMouseX);
}

void ABowlingLane::OnStanceLeftPressed()   { bStanceLeft = true; }
void ABowlingLane::OnStanceLeftReleased()  { bStanceLeft = false; }
void ABowlingLane::OnStanceRightPressed()  { bStanceRight = true; }
void ABowlingLane::OnStanceRightReleased() { bStanceRight = false; }
void ABowlingLane::OnAimLeftPressed()      { bAimLeft = true; }
void ABowlingLane::OnAimLeftReleased()     { bAimLeft = false; }
void ABowlingLane::OnAimRightPressed()     { bAimRight = true; }
void ABowlingLane::OnAimRightReleased()    { bAimRight = false; }
void ABowlingLane::OnFinePressed()         { bFine = true; }
void ABowlingLane::OnFineReleased()        { bFine = false; }
void ABowlingLane::OnMouseX(float Value)   { PendingMouseAim += Value; }
void ABowlingLane::OnHookMore()            { Hook = FMath::Clamp(Hook + BowlingLaneDetail::HookStep, -1.0, 1.0); }
void ABowlingLane::OnHookLess()            { Hook = FMath::Clamp(Hook - BowlingLaneDetail::HookStep, -1.0, 1.0); }

void ABowlingLane::OnBowlPressed()
{
	const EBowlingPhase Phase = Sim.Phase();
	if (Phase == EBowlingPhase::Idle || Phase == EBowlingPhase::GameOver)
	{
		OnNewGame();   // the release of this press must not also bowl, so no charge
		return;
	}
	if (Phase == EBowlingPhase::Ready)
	{
		bCharging = true;
		ChargeSeconds = 0.0f;
	}
}

void ABowlingLane::OnBowlReleased()
{
	if (!bCharging)
	{
		return;
	}
	// Power first, THEN end the charge: CurrentPower() reads 0 once it is over (the pool
	// table's first build shot every stroke at minimum speed that way).
	const float Power = CurrentPower();
	bCharging = false;
	Sim.Roll(StanceY, AimAngle, Power, Hook);
}

void ABowlingLane::OnNewGame()
{
	if (Sim.Phase() == EBowlingPhase::Rolling)
	{
		return;
	}
	bCharging = false;
	Sim.StartGame();
	Sim.TakeEvents();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Bowling] new game."));
}

void ABowlingLane::OnLeave()
{
	StopPlaying();
}

float ABowlingLane::CurrentPower() const
{
	if (!bCharging)
	{
		return 0.0f;
	}
	const float Phase = FMath::Fmod(ChargeSeconds / PowerChargeSeconds, 2.0f);
	return Phase <= 1.0f ? Phase : 2.0f - Phase;
}

void ABowlingLane::UpdateControls(float DeltaSeconds)
{
	if (Sim.Phase() != EBowlingPhase::Ready)
	{
		PendingMouseAim = 0.0f;
		return;
	}
	const FBowlingLaneLayout& L = Sim.Layout();
	const double Fine = bFine ? 0.2 : 1.0;

	// A is the bowler's left, which is the sim's +Y.
	const double Slide = ((bStanceLeft ? 1.0 : 0.0) - (bStanceRight ? 1.0 : 0.0)) * StanceSpeed * DeltaSeconds * Fine;
	StanceY = FMath::Clamp(StanceY + Slide, -(L.LaneHalfWidth - L.BallRadius), L.LaneHalfWidth - L.BallRadius);

	double Degrees = ((bAimLeft ? 1.0 : 0.0) - (bAimRight ? 1.0 : 0.0)) * AimDegreesPerSecond * DeltaSeconds;
	Degrees -= PendingMouseAim * MouseAimDegreesPerUnit;   // mouse right aims right
	PendingMouseAim = 0.0f;
	AimAngle = FMath::Clamp(AimAngle + FMath::DegreesToRadians(Degrees * Fine), -BowlingLaneDetail::AimLimitRadians, BowlingLaneDetail::AimLimitRadians);
}

void ABowlingLane::SyncBall(float DeltaSeconds)
{
	if (!Ball)
	{
		return;
	}
	const FBowlingLaneLayout& L = Sim.Layout();
	const bool bRolling = Sim.IsBallOnLane();
	const bool bWaiting = Sim.Phase() == EBowlingPhase::Ready || Sim.Phase() == EBowlingPhase::Idle || Sim.Phase() == EBowlingPhase::GameOver;
	Ball->SetVisibility(bRolling || bWaiting);
	if (bRolling)
	{
		// The sim's spin, mirrored into the Lane frame (see the header), turns the mesh.
		const FBowlingVec3 W = Sim.BallSpin();
		const FVector Omega(-W.X, W.Y, -W.Z);
		const double Rate = Omega.Size();
		if (Rate > 1e-6 && DeltaSeconds > 0.0f)
		{
			BallRotation = (FQuat(Omega / Rate, Rate * DeltaSeconds) * BallRotation).GetNormalized();
		}
		const double Height = Sim.IsBallInGutter() ? L.BallRadius - BowlingLaneDetail::GutterDrop : L.BallRadius;
		PlaceBallMesh(SimToLocal(Sim.BallPosition(), Height));
	}
	else if (bWaiting)
	{
		// Waiting on the foul line where he stands, ready to go.
		PlaceBallMesh(SimToLocal(FBowlingVec2(-L.BallRadius, StanceY), L.BallRadius));
	}
}

void ABowlingLane::PlaceBallMesh(const FVector& Centre)
{
	if (Ball)
	{
		Ball->SetRelativeLocationAndRotation(Centre - BallRotation.RotateVector(BallMeshCentre), BallRotation);
	}
}

void ABowlingLane::SyncPins()
{
	for (int32 I = 0; I < FBowlingSim::PinCount && I < PinMeshes.Num(); ++I)
	{
		UStaticMeshComponent* Pin = PinMeshes[I];
		if (!Pin)
		{
			continue;
		}
		const EPinState State = Sim.PinState(I);
		Pin->SetVisibility(State != EPinState::Gone);
		if (State == EPinState::Gone)
		{
			continue;
		}
		FQuat Rotation = PinUpright;
		if (State == EPinState::Down)
		{
			// Tipping over about its base toward the way it was hit, flat in a quarter second.
			const double Tip = FMath::DegreesToRadians(FMath::Min(90.0, Sim.PinSecondsDown(I) * BowlingLaneDetail::PinTipDegreesPerSecond));
			const double Dir = Sim.PinFallDirection(I);
			const FVector Toward = SimToLocal(FBowlingVec2(FMath::Cos(Dir), FMath::Sin(Dir)), 0.0).GetSafeNormal();
			const FVector Axis = FVector::CrossProduct(FVector::UpVector, Toward).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				Rotation = FQuat(Axis, Tip) * PinUpright;
			}
		}
		Pin->SetRelativeLocationAndRotation(SimToLocal(Sim.PinPosition(I), 0.0), Rotation);
	}
}

void ABowlingLane::SyncAimLine()
{
	if (!AimLine)
	{
		return;
	}
	const bool bShow = bPlaying && Sim.Phase() == EBowlingPhase::Ready;
	AimLine->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	const FBowlingVec2 Dir(FMath::Cos(AimAngle), FMath::Sin(AimAngle));
	const FBowlingVec2 From(0.0, StanceY);
	const FBowlingVec2 Mid = From + Dir * (BowlingLaneDetail::AimLineLength * 0.5);
	const FVector LocalDir = SimToLocal(Dir, 0.0);
	AimLine->SetRelativeLocationAndRotation(SimToLocal(Mid, 0.3), LocalDir.Rotation());
	AimLine->SetRelativeScale3D(FVector(BowlingLaneDetail::AimLineLength / BowlingLaneDetail::ShapeSize, 1.0 / BowlingLaneDetail::ShapeSize, 0.1 / BowlingLaneDetail::ShapeSize));
}

void ABowlingLane::SyncCamera(float DeltaSeconds, bool bSnap)
{
	if (!LaneCamera)
	{
		return;
	}
	const FBowlingLaneLayout& L = Sim.Layout();
	FVector Loc;
	FVector Target;
	if (Sim.Phase() == EBowlingPhase::Rolling && Sim.IsBallOnLane())
	{
		// Chasing the ball, until it is close to the pins; then holding there to watch.
		const FVector B = SimToLocal(Sim.BallPosition(), L.BallRadius);
		Loc = FVector(FMath::Min(B.X - 230.0, L.HeadPinX - 380.0), B.Y * 0.6, 75.0);
		Target = FVector(FMath::Min(B.X + 450.0, L.HeadPinX + 40.0), B.Y * 0.5, 15.0);
	}
	else if (Sim.Phase() == EBowlingPhase::Rolling)
	{
		Loc = FVector(L.HeadPinX - 380.0, 0.0, 85.0);
		Target = FVector(L.HeadPinX + 40.0, 0.0, 10.0);
	}
	else
	{
		// On the approach, behind the ball, looking down the lane.
		Loc = FVector(-250.0, -StanceY * 0.8, 140.0);
		Target = FVector(L.HeadPinX, 0.0, 15.0);
	}
	const FRotator Rot = (Target - Loc).Rotation();
	if (bSnap || DeltaSeconds <= 0.0f)
	{
		LaneCamera->SetRelativeLocationAndRotation(Loc, Rot);
		return;
	}
	const float Speed = Sim.Phase() == EBowlingPhase::Rolling ? 6.0f : 3.0f;
	LaneCamera->SetRelativeLocationAndRotation(
		FMath::VInterpTo(LaneCamera->GetRelativeLocation(), Loc, DeltaSeconds, Speed),
		FMath::RInterpTo(LaneCamera->GetRelativeRotation(), Rot, DeltaSeconds, Speed));
}

void ABowlingLane::HandleEvents()
{
	// A strike is a dozen contacts in a few frames: sound only the loudest few.
	TArray<float, TInlineAllocator<32>> Clatter;
	for (const FBowlingEvent& Event : Sim.TakeEvents())
	{
		switch (Event.Type)
		{
		case EBowlingEvent::BallReleased:
			TableSounds::Play(this, ETableSound::BowlRelease, 0.8f);
			TableSounds::Play(this, ETableSound::BowlRoll, FMath::Clamp(static_cast<float>(Event.Value) / 1000.0f, 0.4f, 0.9f));
			BallRotation = FQuat::Identity;
			break;
		case EBowlingEvent::BallHitPin:
		case EBowlingEvent::PinHitPin:
		case EBowlingEvent::PinHitKickback:
			Clatter.Add(FMath::Clamp(static_cast<float>(Event.Value) / 400.0f, 0.15f, 1.0f));
			break;
		case EBowlingEvent::Strike:
			TableSounds::Play(this, ETableSound::Chime100, 0.6f);
			UE_LOG(LogSibeliusGame, Display, TEXT("[Bowling] STRIKE, frame %d."), Event.Index + 1);
			break;
		case EBowlingEvent::Spare:
			TableSounds::Play(this, ETableSound::Chime10, 0.6f);
			break;
		case EBowlingEvent::GameOver:
			BestScore = FMath::Max(BestScore, Event.Index);
			// The session's best is the lane's; the lifetime best is kept for the agents (ChatDeeds.h).
			if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
			{
				Progression->RaiseStat(SibeliusStats::BowlingBest, Event.Index);
			}
			TableSounds::Play(this, ETableSound::Chime1000, 0.7f);
			UE_LOG(LogSibeliusGame, Display, TEXT("[Bowling] game over: %d (best %d)."), Event.Index, BestScore);
			break;
		default:
			break;
		}
	}
	Clatter.Sort(TGreater<float>());
	for (int32 I = 0; I < FMath::Min(Clatter.Num(), 4); ++I)
	{
		TableSounds::Play(this, ETableSound::PinClatter, Clatter[I], FMath::FRandRange(0.9f, 1.12f));
	}
}

// One cell per frame, marks then running total: "X 30", "7/ 49", "9- 58".
FString ABowlingLane::Scorecard() const
{
	const std::vector<int>& Rolls = Sim.Rolls();
	int Totals[FBowlingSim::FrameCount];
	FBowlingSim::ScoreRolls(Rolls, Totals);

	auto Mark = [](int Pins) { return Pins == 0 ? FString(TEXT("-")) : FString::FromInt(Pins); };
	TArray<FString> Cells;
	size_t R = 0;
	for (int32 F = 0; F < FBowlingSim::FrameCount && R < Rolls.size(); ++F)
	{
		FString Marks;
		if (F < FBowlingSim::FrameCount - 1)
		{
			if (Rolls[R] == 10)
			{
				Marks = TEXT("X");
				R += 1;
			}
			else
			{
				Marks = Mark(Rolls[R]);
				if (R + 1 < Rolls.size())
				{
					Marks += Rolls[R] + Rolls[R + 1] == 10 ? FString(TEXT("/")) : Mark(Rolls[R + 1]);
				}
				R += 2;
			}
		}
		else
		{
			// The tenth: up to three balls, each an X, a spare or a count.
			int Standing = 10;
			for (int K = 0; K < 3 && R < Rolls.size(); ++K, ++R)
			{
				const int Pins = Rolls[R];
				if (Pins == 10 && Standing == 10)
				{
					Marks += TEXT("X");
				}
				else if (Pins == Standing)
				{
					Marks += TEXT("/");
				}
				else
				{
					Marks += Mark(Pins);
				}
				Standing = Standing - Pins == 0 ? 10 : Standing - Pins;
			}
		}
		Cells.Add(Totals[F] >= 0 ? FString::Printf(TEXT("%s %d"), *Marks, Totals[F]) : Marks);
	}
	return FString::Join(Cells, TEXT("   |   "));
}

void ABowlingLane::RefreshPanel()
{
	ASibeliusHUD* Hud = bPlaying ? GetHud() : nullptr;
	if (!Hud)
	{
		return;
	}
	const FString Best = BestScore > 0 ? FString::Printf(TEXT("     BEST %d"), BestScore) : FString();
	FString Status;
	FString Controls;
	switch (Sim.Phase())
	{
	case EBowlingPhase::Idle:
		Status = TEXT("THE TRANSHUMAN BOWLING ALLEY");
		Controls = TEXT("SPACE for a new game      ESC to walk away      O back to the office");
		break;
	case EBowlingPhase::GameOver:
		Status = FString::Printf(TEXT("GAME OVER     %d%s"), Sim.Score(), Sim.Score() == BestScore ? TEXT("  -  YOUR BEST") : *Best);
		Controls = Scorecard() + TEXT("\nSPACE for a new game      ESC to walk away      O back to the office");
		break;
	default:
	{
		const FString HookName = FMath::IsNearlyZero(Hook) ? FString(TEXT("straight"))
			: FString::Printf(TEXT("%s %d"), Hook > 0.0 ? TEXT("hook") : TEXT("left-hand hook"), FMath::RoundToInt(FMath::Abs(Hook) * 4.0));
		FString Power;
		if (bCharging)
		{
			const int32 Bars = FMath::RoundToInt(CurrentPower() * 20.0f);
			Power = TEXT("     POWER ") + FString::ChrN(Bars, TEXT('|')) + FString::ChrN(20 - Bars, TEXT('.'));
		}
		Status = FString::Printf(TEXT("FRAME %d     BALL %d     SCORE %d%s\n%s%s"),
			Sim.Frame() + 1, Sim.BallInFrame() + 1, Sim.RunningScore(), *Best, *HookName, *Power);
		Controls = Scorecard() + TEXT("\nMOVE  A D      AIM  mouse or arrows (SHIFT fine)      HOOK  W S      BOWL  hold SPACE, let go\nESC  walk away      O  back to the office");
		break;
	}
	}
	Hud->ShowTablePanel(Status, Controls);
}

void ABowlingLane::DrawLane() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World || !Lane)
	{
		return;
	}
	const FTransform& Frame = Lane->GetComponentTransform();
	const FBowlingLaneLayout& L = Sim.Layout();
	auto P = [&Frame](double X, double Y) { return Frame.TransformPosition(SimToLocal(FBowlingVec2(X, Y), 0.5)); };
	const double W = L.LaneHalfWidth;
	DrawDebugLine(World, P(0.0, W), P(L.PitX, W), FColor::Green, false, -1.0f, 0, 0.6f);
	DrawDebugLine(World, P(0.0, -W), P(L.PitX, -W), FColor::Green, false, -1.0f, 0, 0.6f);
	DrawDebugLine(World, P(0.0, W), P(0.0, -W), FColor::Red, false, -1.0f, 0, 0.8f);          // the foul line
	DrawDebugLine(World, P(L.OilLength, W), P(L.OilLength, -W), FColor::Cyan, false, -1.0f, 0, 0.4f);   // end of the oil
	DrawDebugLine(World, P(L.PitX, W), P(L.PitX, -W), FColor::Orange, false, -1.0f, 0, 0.6f);  // the pit
	const FVector AxisX = Frame.GetUnitAxis(EAxis::X);
	const FVector AxisY = Frame.GetUnitAxis(EAxis::Y);
	for (const FBowlingVec2& Spot : L.PinSpots)
	{
		DrawDebugCircle(World, P(Spot.X, Spot.Y), L.PinRadius, 16, FColor::Yellow, false, -1.0f, 0, 0.5f, AxisX, AxisY, false);
	}
#endif
}

ASibeliusHUD* ABowlingLane::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}
