// BilliardsTable.cpp — see the header for what this actor is for and what it must never do.

#include "BilliardsTable.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"
#include "TableSounds.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
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
namespace BilliardsTableDetail
{
	const FBilliardsTableLayout& DefaultLayout()
	{
		static const FBilliardsTableLayout Layout = FBilliardsTableLayout::MakeDefault();
		return Layout;
	}

	// The cue mesh is 153.96 cm long, lying along X with its TIP at -X (measured: 0.77 cm
	// radius at -77, 2.14 at +77).
	constexpr double CueHalfLength = 76.98;
	constexpr double CueTipGap = 1.5;            // cm between tip and ball, at rest
	constexpr double CuePullBack = 28.0;         // extra cm at full power
	constexpr double CueElevationDegrees = 6.0;  // butt raised over the rail

	constexpr double GuideHeight = 0.25;         // aim guides lie just on the cloth
	constexpr double GuideThickness = 0.5;
	constexpr double ObjectLineLength = 35.0;
	constexpr double DeflectLineLength = 22.0;

	constexpr double SpinStep = 0.25;

	// The engine's basic shapes are 100 cm across.
	constexpr double ShapeSize = 100.0;
}

ABilliardsTable::ABilliardsTable()
{
	PrimaryActorTick.bCanEverTick = true;

	const FBilliardsTableLayout& Layout = BilliardsTableDetail::DefaultLayout();
	Sim = FBilliardsSim(Layout);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TableMesh(TEXT("/Game/Billiard_Table/Meshes/SM_Billiard_Table.SM_Billiard_Table"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CueBallMesh(TEXT("/Game/Billiard_Table/Meshes/SM_White_Billiard_Ball.SM_White_Billiard_Ball"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CueMesh(TEXT("/Game/Billiard_Table/Meshes/SM_Billiard_Cue.SM_Billiard_Cue"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
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

	Table = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Table"));
	Table->SetupAttachment(SceneRoot);
	MakeVisualOnly(Table);
	if (TableMesh.Succeeded())
	{
		Table->SetStaticMesh(TableMesh.Object);
	}

	// The table's outline is 259.7 x 168.7 x 93.7 cm, centred on its pivot, floor at 0.
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetBoxExtent(FVector(130.0, 84.5, 47.0));
	Body->SetRelativeLocation(FVector(0.0, 0.0, 47.0));
	Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->SetGenerateOverlapEvents(false);

	Cloth = CreateDefaultSubobject<USceneComponent>(TEXT("Cloth"));
	Cloth->SetupAttachment(SceneRoot);
	Cloth->SetMobility(EComponentMobility::Movable);
	Cloth->SetRelativeLocation(FVector(0.0, 0.0, Layout.ClothHeight));

	BallMeshes.SetNum(FBilliardsSim::BallCount);
	for (int32 I = 0; I < FBilliardsSim::BallCount; ++I)
	{
		UStaticMeshComponent* BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Ball%02d"), I));
		BallMesh->SetupAttachment(Cloth);
		MakeVisualOnly(BallMesh);
		// Each ball is its own asset. Found here rather than in a static: the hard
		// reference is what matters, and FObjectFinder does not care about lifetime.
		UStaticMesh* Mesh = CueBallMesh.Object;
		if (I > 0)
		{
			const FString Name = FString::Printf(TEXT("SM_Billiard_Ball_%02d"), I);
			ConstructorHelpers::FObjectFinder<UStaticMesh> Numbered(*FString::Printf(TEXT("/Game/Billiard_Table/Meshes/%s.%s"), *Name, *Name));
			Mesh = Numbered.Object;
		}
		if (Mesh)
		{
			BallMesh->SetStaticMesh(Mesh);   // 7.0 cm, centred: no scaling
		}
		BallMesh->SetRelativeLocation(SimToLocal(Sim.BallPosition(I), Layout.BallRadius));
		BallMeshes[I] = BallMesh;
	}

	Cue = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cue"));
	Cue->SetupAttachment(Cloth);
	MakeVisualOnly(Cue);
	Cue->SetCastShadow(true);
	if (CueMesh.Succeeded())
	{
		Cue->SetStaticMesh(CueMesh.Object);
	}

	auto MakeGuide = [&](const TCHAR* Name, ConstructorHelpers::FObjectFinder<UStaticMesh>& Shape) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Guide = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Guide->SetupAttachment(Cloth);
		MakeVisualOnly(Guide);
		Guide->SetCastShadow(false);
		if (Shape.Succeeded())
		{
			Guide->SetStaticMesh(Shape.Object);
		}
		if (GuideMaterial.Succeeded())
		{
			Guide->SetMaterial(0, GuideMaterial.Object);
		}
		Guide->SetVisibility(false);
		return Guide;
	};
	AimLine = MakeGuide(TEXT("AimLine"), CubeMesh);
	ObjectLine = MakeGuide(TEXT("ObjectLine"), CubeMesh);
	DeflectLine = MakeGuide(TEXT("DeflectLine"), CubeMesh);
	GhostDisc = MakeGuide(TEXT("GhostDisc"), CylinderMesh);
	GhostDisc->SetRelativeScale3D(FVector(Layout.BallRadius * 2.0 / BilliardsTableDetail::ShapeSize,
		Layout.BallRadius * 2.0 / BilliardsTableDetail::ShapeSize, 0.2 / BilliardsTableDetail::ShapeSize));

	TableCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TableCamera"));
	TableCamera->SetupAttachment(SceneRoot);
	TableCamera->SetFieldOfView(75.0f);   // the whole table from below the ceiling

	BallRotations.Init(FQuat::Identity, FBilliardsSim::BallCount);
	SyncCamera(0.0f, true);
}

FVector ABilliardsTable::SimToLocal(FBilliardsVec2 P, double Height)
{
	// The sim's plane IS the cloth's: X along the table, Y across. Unreal's yaw turns X
	// toward Y, as the sim's angles do, so an angle means the same thing in both.
	return FVector(P.X, P.Y, Height);
}

void ABilliardsTable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncBalls(0.0f);
	SyncCueAndGuides();
	SyncCamera(0.0f, true);
}

void ABilliardsTable::BeginPlay()
{
	Super::BeginPlay();

	Sim = FBilliardsSim(BilliardsTableDetail::DefaultLayout());
	BallRotations.Init(FQuat::Identity, FBilliardsSim::BallCount);
	AimAngle = 0.0;
	Spin = 0.0;
	SyncBalls(0.0f);
	SyncCueAndGuides();
	SyncCamera(0.0f, true);
}

void ABilliardsTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Whatever tore this down - a level travel, PIE stopping, the actor destroyed with him
	// at the table - he gets his controls and his own eyes back.
	StopPlaying();
	Super::EndPlay(EndPlayReason);
}

bool ABilliardsTable::ShouldTickIfViewportsOnly() const
{
	return bDrawPhysicsLayoutInEditor;
}

void ABilliardsTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!World->IsGameWorld())
	{
		if (bDrawPhysicsLayoutInEditor)
		{
			DrawPhysicsLayout();
		}
		return;
	}

	// The balls only roll while he is at the table: walk away mid-stroke and they wait.
	if (bPlaying)
	{
		UpdateAim(DeltaSeconds);
		if (bCharging)
		{
			ChargeSeconds += DeltaSeconds;
		}
		Sim.Advance(DeltaSeconds);
		HandleEvents();

		// The HUD lease, renewed every frame (the pinball's pattern).
		if (ASibeliusHUD* Hud = GetHud())
		{
			Hud->HoldCinematic(0.25f);
		}
	}

	SyncBalls(DeltaSeconds);
	SyncCueAndGuides();
	SyncCamera(DeltaSeconds, false);
	RefreshPanel();
}

void ABilliardsTable::Interact_Implementation(AActor* Interactor)
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

FText ABilliardsTable::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Billiards", "Prompt", "[E] play pool");
}

void ABilliardsTable::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	bCharging = false;
	ChargeSeconds = 0.0f;
	bAimLeftHeld = bAimRightHeld = bFineHeld = false;
	PendingMouseAim = 0.0f;
	StickAim = 0.0f;

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	SyncCamera(0.0f, true);
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);

	// Our bindings sit above the pawn's and consume their keys, so E here means "leave the
	// table", not "interact".
	EnableInput(PC);
	BindTableKeys();

	UE_LOG(LogSibeliusGame, Display, TEXT("[Billiards] at the table (%s)."), *GetActorNameOrLabel());
}

void ABilliardsTable::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	bCharging = false;
	ChargeSeconds = 0.0f;
	bAimLeftHeld = bAimRightHeld = bFineHeld = false;
	Sim.TakeEvents();

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
	UE_LOG(LogSibeliusGame, Display, TEXT("[Billiards] left the table."));
}

void ABilliardsTable::BindTableKeys()
{
	// Once: DisableInput keeps the bindings, so binding on every visit would double them.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;

	using FHandler = void (ABilliardsTable::*)();
	auto Bind = [this](const FKey& Key, FHandler Pressed, FHandler Released)
	{
		InputComponent->BindKey(Key, IE_Pressed, this, Pressed);
		if (Released)
		{
			InputComponent->BindKey(Key, IE_Released, this, Released);
		}
	};

	for (const FKey& Key : { EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left })
	{
		Bind(Key, &ABilliardsTable::OnAimLeftPressed, &ABilliardsTable::OnAimLeftReleased);
	}
	for (const FKey& Key : { EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right })
	{
		Bind(Key, &ABilliardsTable::OnAimRightPressed, &ABilliardsTable::OnAimRightReleased);
	}
	for (const FKey& Key : { EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl, EKeys::Gamepad_LeftShoulder })
	{
		Bind(Key, &ABilliardsTable::OnFinePressed, &ABilliardsTable::OnFineReleased);
	}
	for (const FKey& Key : { EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up })
	{
		Bind(Key, &ABilliardsTable::OnSpinUp, nullptr);
	}
	for (const FKey& Key : { EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down })
	{
		Bind(Key, &ABilliardsTable::OnSpinDown, nullptr);
	}
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_RightTrigger })
	{
		Bind(Key, &ABilliardsTable::OnShotPressed, &ABilliardsTable::OnShotReleased);
	}
	for (const FKey& Key : { EKeys::Enter, EKeys::R, EKeys::Gamepad_Special_Right })
	{
		Bind(Key, &ABilliardsTable::OnRack, nullptr);
	}
	for (const FKey& Key : { EKeys::V, EKeys::Tab, EKeys::Gamepad_FaceButton_Top })
	{
		Bind(Key, &ABilliardsTable::OnToggleView, nullptr);
	}
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		Bind(Key, &ABilliardsTable::OnLeave, nullptr);
	}

	InputComponent->BindAxisKey(EKeys::MouseX, this, &ABilliardsTable::OnMouseX);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &ABilliardsTable::OnStickX);
}

void ABilliardsTable::OnAimLeftPressed()   { bAimLeftHeld = true; }
void ABilliardsTable::OnAimLeftReleased()  { bAimLeftHeld = false; }
void ABilliardsTable::OnAimRightPressed()  { bAimRightHeld = true; }
void ABilliardsTable::OnAimRightReleased() { bAimRightHeld = false; }
void ABilliardsTable::OnFinePressed()      { bFineHeld = true; }
void ABilliardsTable::OnFineReleased()     { bFineHeld = false; }
void ABilliardsTable::OnMouseX(float Value) { PendingMouseAim += Value; }
void ABilliardsTable::OnStickX(float Value) { StickAim = FMath::Abs(Value) > 0.15f ? Value : 0.0f; }

void ABilliardsTable::OnSpinUp()
{
	Spin = FMath::Clamp(Spin + BilliardsTableDetail::SpinStep, -1.0, 1.0);
}

void ABilliardsTable::OnSpinDown()
{
	Spin = FMath::Clamp(Spin - BilliardsTableDetail::SpinStep, -1.0, 1.0);
}

void ABilliardsTable::OnShotPressed()
{
	const EBilliardsPhase Phase = Sim.Phase();
	if (Phase == EBilliardsPhase::Idle || Phase == EBilliardsPhase::Cleared)
	{
		// The same key racks a new game. Its release must not also shoot, so no charge.
		OnRack();
		return;
	}
	if (Phase == EBilliardsPhase::Aiming)
	{
		bCharging = true;
		ChargeSeconds = 0.0f;
	}
}

void ABilliardsTable::OnShotReleased()
{
	if (!bCharging)
	{
		return;
	}
	// Read the power BEFORE ending the charge: CurrentPower() is 0 when not charging, and
	// the first build shot every stroke at the minimum speed that way.
	const float Power = CurrentPower();
	bCharging = false;
	Sim.Shoot(AimAngle, Power, Spin);
}

void ABilliardsTable::OnRack()
{
	// Not mid-stroke: a rerack while balls roll would throw away a stroke he is watching.
	if (Sim.Phase() == EBilliardsPhase::Rolling)
	{
		return;
	}
	bCharging = false;
	Sim.StartGame();
	Sim.TakeEvents();
	BallRotations.Init(FQuat::Identity, FBilliardsSim::BallCount);
	AimAngle = 0.0;   // the cue ball starts on the head spot, straight up the table at the 1
	Spin = 0.0;
	LastClearedStrokes = 0;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Billiards] racked."));
}

void ABilliardsTable::OnToggleView()
{
	bBehindBallView = !bBehindBallView;
}

void ABilliardsTable::OnLeave()
{
	StopPlaying();
}

float ABilliardsTable::CurrentPower() const
{
	if (!bCharging)
	{
		return 0.0f;
	}
	// Up and back down again: hold too long and it eases off, rather than sticking at full.
	const float Phase = FMath::Fmod(ChargeSeconds / PowerChargeSeconds, 2.0f);
	return Phase <= 1.0f ? Phase : 2.0f - Phase;
}

void ABilliardsTable::UpdateAim(float DeltaSeconds)
{
	if (Sim.Phase() != EBilliardsPhase::Aiming)
	{
		PendingMouseAim = 0.0f;
		return;
	}
	const float Fine = bFineHeld ? FineAimFraction : 1.0f;
	// RIGHT TURNS THE AIM TO THE RIGHT, in both views: seen down the cue it swings right,
	// seen from overhead it turns clockwise. Unreal's right is +90 degrees of yaw from
	// forward, and the sim's angles are yaw, so "right" is simply a larger angle.
	float Degrees = (bAimRightHeld ? 1.0f : 0.0f) - (bAimLeftHeld ? 1.0f : 0.0f) + StickAim;
	Degrees *= AimDegreesPerSecond * DeltaSeconds;
	Degrees += PendingMouseAim * MouseAimDegreesPerUnit;
	Degrees *= Fine;
	PendingMouseAim = 0.0f;

	AimAngle = FMath::UnwindRadians(AimAngle + FMath::DegreesToRadians(static_cast<double>(Degrees)));
}

void ABilliardsTable::AimAtNearestBall()
{
	const FBilliardsVec2 CuePos = Sim.BallPosition(FBilliardsSim::CueBall);
	double Best = 1e30;
	for (int32 I = 1; I < FBilliardsSim::BallCount; ++I)
	{
		if (!Sim.IsOnTable(I))
		{
			continue;
		}
		const FBilliardsVec2 D = Sim.BallPosition(I) - CuePos;
		const double Dist2 = D.X * D.X + D.Y * D.Y;
		if (Dist2 < Best)
		{
			Best = Dist2;
			AimAngle = FMath::Atan2(D.Y, D.X);
		}
	}
}

void ABilliardsTable::SyncBalls(float DeltaSeconds)
{
	const double R = Sim.Layout().BallRadius;
	for (int32 I = 0; I < FBilliardsSim::BallCount && I < BallMeshes.Num(); ++I)
	{
		UStaticMeshComponent* BallMesh = BallMeshes[I];
		if (!BallMesh)
		{
			continue;
		}
		const bool bOnTable = Sim.IsOnTable(I);
		BallMesh->SetVisibility(bOnTable);
		if (!bOnTable)
		{
			continue;
		}

		// Turn the ball by the sim's spin, not by how far it moved: a drawn ball slides
		// forward while it spins backward, and that is the thing worth seeing. The sim's
		// spin vector is in cloth space, and FQuat's axis-angle uses the same algebra.
		const FBilliardsVec3 W = Sim.BallSpin(I);
		const FVector Omega(W.X, W.Y, W.Z);
		const double Rate = Omega.Size();
		if (Rate > 1e-6 && DeltaSeconds > 0.0f && BallRotations.IsValidIndex(I))
		{
			BallRotations[I] = (FQuat(Omega / Rate, Rate * DeltaSeconds) * BallRotations[I]).GetNormalized();
		}
		BallMesh->SetRelativeLocationAndRotation(SimToLocal(Sim.BallPosition(I), R),
			BallRotations.IsValidIndex(I) ? BallRotations[I] : FQuat::Identity);
	}
}

void ABilliardsTable::SyncCueAndGuides()
{
	using namespace BilliardsTableDetail;

	const bool bAiming = bPlaying && Sim.Phase() == EBilliardsPhase::Aiming && Sim.IsOnTable(FBilliardsSim::CueBall);
	for (UStaticMeshComponent* Part : { Cue.Get(), AimLine.Get(), GhostDisc.Get(), ObjectLine.Get(), DeflectLine.Get() })
	{
		if (Part)
		{
			Part->SetVisibility(bAiming);
		}
	}
	if (!bAiming)
	{
		return;
	}

	const double R = Sim.Layout().BallRadius;
	const FBilliardsVec2 CuePos = Sim.BallPosition(FBilliardsSim::CueBall);
	const FVector Dir(FMath::Cos(AimAngle), FMath::Sin(AimAngle), 0.0);
	const double YawDegrees = FMath::RadiansToDegrees(AimAngle);

	// The cue: tip on the aim line at the height the spin asks for, butt raised, pulled
	// back with the power.
	if (Cue)
	{
		const double TipHeight = R + Spin * FBilliardsSim::MaxTipOffset * R;
		const FVector Tip = SimToLocal(CuePos, TipHeight) - Dir * (R + CueTipGap + CuePullBack * CurrentPower());
		const double Elev = FMath::DegreesToRadians(CueElevationDegrees);
		const FVector Back = -Dir * FMath::Cos(Elev) + FVector::UpVector * FMath::Sin(Elev);
		// The mesh's tip is at its -X, so its +X must point back along the cue.
		Cue->SetRelativeLocationAndRotation(Tip + Back * CueHalfLength, FRotator(CueElevationDegrees, YawDegrees + 180.0, 0.0));
	}

	const FBilliardsAimPrediction Aim = Sim.PredictShot(AimAngle);

	// A flat bar from A to B, lying on the cloth.
	auto LayBar = [](UStaticMeshComponent* Bar, FVector A, FVector B)
	{
		if (!Bar)
		{
			return;
		}
		const FVector D = B - A;
		const double Len = D.Size2D();
		if (Len < 0.5)
		{
			Bar->SetVisibility(false);
			return;
		}
		Bar->SetRelativeLocationAndRotation((A + B) * 0.5, FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.0));
		Bar->SetRelativeScale3D(FVector(Len / ShapeSize, GuideThickness / ShapeSize, 0.1 / ShapeSize));
	};

	const FVector From = SimToLocal(CuePos, GuideHeight) + Dir * R;
	const FVector Contact = SimToLocal(Aim.CueAtContact, GuideHeight);
	LayBar(AimLine, From, Contact - Dir * R);

	if (GhostDisc)
	{
		GhostDisc->SetRelativeLocation(SimToLocal(Aim.CueAtContact, GuideHeight));
	}

	if (Aim.bHitsBall)
	{
		const FVector ObjectPos = SimToLocal(Sim.BallPosition(Aim.Ball), GuideHeight);
		const FVector ObjDir(Aim.ObjectDirection.X, Aim.ObjectDirection.Y, 0.0);
		LayBar(ObjectLine, ObjectPos + ObjDir * R, ObjectPos + ObjDir * (R + ObjectLineLength));

		const FVector DefDir(Aim.CueDeflection.X, Aim.CueDeflection.Y, 0.0);
		if (DefDir.IsNearlyZero())
		{
			if (DeflectLine)
			{
				DeflectLine->SetVisibility(false);
			}
		}
		else
		{
			LayBar(DeflectLine, Contact + DefDir * R, Contact + DefDir * (R + DeflectLineLength));
		}
	}
	else
	{
		if (ObjectLine)
		{
			ObjectLine->SetVisibility(false);
		}
		if (DeflectLine)
		{
			DeflectLine->SetVisibility(false);
		}
	}
}

void ABilliardsTable::SyncCamera(float DeltaSeconds, bool bSnap)
{
	if (!TableCamera)
	{
		return;
	}
	const double ClothZ = Sim.Layout().ClothHeight;

	FVector TargetLoc;
	FRotator TargetRot;
	const bool bBehind = bBehindBallView && Sim.IsOnTable(FBilliardsSim::CueBall) && Sim.Phase() != EBilliardsPhase::Idle;
	if (bBehind)
	{
		// Down the cue, a little above it: the view for lining up a thin cut.
		const FVector Dir(FMath::Cos(AimAngle), FMath::Sin(AimAngle), 0.0);
		const FVector Ball = SimToLocal(Sim.BallPosition(FBilliardsSim::CueBall), ClothZ);
		TargetLoc = Ball - Dir * 75.0 + FVector(0.0, 0.0, 38.0);
		TargetRot = (Ball + Dir * 60.0 - TargetLoc).Rotation();
	}
	else
	{
		// From the -Y long side, high, looking down at the middle of the cloth: the whole
		// table on screen, its length across it.
		const double Pitch = FMath::DegreesToRadians(static_cast<double>(OverheadPitchDegrees));
		const FVector Center(0.0, 0.0, ClothZ);
		TargetLoc = Center + FVector(0.0, -FMath::Cos(Pitch), FMath::Sin(Pitch)) * OverheadDistance;
		TargetRot = FRotator(-OverheadPitchDegrees, 90.0, 0.0);
	}

	if (bSnap || DeltaSeconds <= 0.0f)
	{
		TableCamera->SetRelativeLocationAndRotation(TargetLoc, TargetRot);
		return;
	}
	const FVector Loc = FMath::VInterpTo(TableCamera->GetRelativeLocation(), TargetLoc, DeltaSeconds, 5.0f);
	const FRotator Rot = FMath::RInterpTo(TableCamera->GetRelativeRotation(), TargetRot, DeltaSeconds, 5.0f);
	TableCamera->SetRelativeLocationAndRotation(Loc, Rot);
}

void ABilliardsTable::HandleEvents()
{
	// A break is dozens of contacts in one frame. Play only the loudest few of each kind:
	// more than that is mush, and the loud ones are what the ear picks out anyway.
	TArray<float, TInlineAllocator<32>> Clicks;
	TArray<float, TInlineAllocator<16>> Thuds;

	for (const FBilliardsEvent& Event : Sim.TakeEvents())
	{
		switch (Event.Type)
		{
		case EBilliardsEvent::ShotTaken:
			TableSounds::Play(this, ETableSound::CueStrike, FMath::Clamp(0.25f + static_cast<float>(Event.Value) / 900.0f, 0.25f, 1.0f));
			break;
		case EBilliardsEvent::BallHitBall:
			Clicks.Add(FMath::Clamp(static_cast<float>(Event.Value) / 350.0f, 0.04f, 1.0f));
			break;
		case EBilliardsEvent::BallHitCushion:
			Thuds.Add(FMath::Clamp(static_cast<float>(Event.Value) / 300.0f, 0.05f, 0.9f));
			break;
		case EBilliardsEvent::BallPocketed:
			TableSounds::Play(this, ETableSound::PocketDrop, 0.8f, FMath::FRandRange(0.95f, 1.05f));
			UE_LOG(LogSibeliusGame, Verbose, TEXT("[Billiards] the %d ball, pocket %d."), Event.Index, Event.Other);
			break;
		case EBilliardsEvent::CueBallScratched:
			TableSounds::Play(this, ETableSound::PocketDrop, 0.8f, 0.9f);
			UE_LOG(LogSibeliusGame, Verbose, TEXT("[Billiards] scratch, pocket %d."), Event.Other);
			break;
		case EBilliardsEvent::CueBallRespotted:
			// Every other stroke starts aimed where the last one was; a respotted cue ball
			// is somewhere new, so point it at something.
			AimAtNearestBall();
			break;
		case EBilliardsEvent::TableCleared:
			LastClearedStrokes = Event.Index;
			if (BestStrokes == 0 || Event.Index < BestStrokes)
			{
				BestStrokes = Event.Index;
			}
			// The pinball's 100 chime, across the room: the house's "well done".
			TableSounds::Play(this, ETableSound::Chime100, 0.6f);
			UE_LOG(LogSibeliusGame, Display, TEXT("[Billiards] table cleared in %d strokes (best %d)."), Event.Index, BestStrokes);
			break;
		default:
			break;
		}
	}

	Clicks.Sort(TGreater<float>());
	for (int32 I = 0; I < FMath::Min(Clicks.Num(), 4); ++I)
	{
		TableSounds::Play(this, ETableSound::BallClick, Clicks[I], FMath::FRandRange(0.93f, 1.07f));
	}
	Thuds.Sort(TGreater<float>());
	for (int32 I = 0; I < FMath::Min(Thuds.Num(), 3); ++I)
	{
		TableSounds::Play(this, ETableSound::CushionThud, Thuds[I], FMath::FRandRange(0.95f, 1.05f));
	}
}

void ABilliardsTable::RefreshPanel()
{
	ASibeliusHUD* Hud = bPlaying ? GetHud() : nullptr;
	if (!Hud)
	{
		return;
	}

	FString Text;
	FString Controls;
	const FString Best = BestStrokes > 0 ? FString::Printf(TEXT("   BEST %d"), BestStrokes) : FString();
	switch (Sim.Phase())
	{
	case EBilliardsPhase::Idle:
		Text = TEXT("CLEAR THE TABLE\nPot all fifteen in as few strokes as you can.");
		Controls = TEXT("SPACE to rack      E to walk away");
		break;
	case EBilliardsPhase::Cleared:
		Text = FString::Printf(TEXT("CLEARED IN %d STROKES%s"),
			LastClearedStrokes, LastClearedStrokes == BestStrokes ? TEXT("  -  YOUR BEST") : *Best);
		Controls = TEXT("SPACE to rack again      E to walk away");
		break;
	default:
	{
		const FString SpinName = FMath::IsNearlyZero(Spin) ? FString(TEXT("centre"))
			: Spin > 0.0 ? FString::Printf(TEXT("follow %d"), FMath::RoundToInt(Spin * 4.0))
			: FString::Printf(TEXT("draw %d"), FMath::RoundToInt(-Spin * 4.0));
		FString Power;
		if (bCharging)
		{
			const int32 Bars = FMath::RoundToInt(CurrentPower() * 20.0f);
			Power = TEXT("   POWER ") + FString::ChrN(Bars, TEXT('|')) + FString::ChrN(20 - Bars, TEXT('.'));
		}
		Text = FString::Printf(TEXT("STROKES %d     LEFT %d%s\nSPIN %s%s"),
			Sim.Strokes(), Sim.BallsLeft(), *Best, *SpinName, *Power);
		Controls = TEXT("AIM  A D or mouse  (SHIFT fine)      SPIN  W S      SHOOT  hold SPACE, let go\nV  view down the cue      R  rerack      E  walk away");
		break;
	}
	}

	Hud->ShowTablePanel(Text, Controls);
}

void ABilliardsTable::DrawPhysicsLayout() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World || !Cloth)
	{
		return;
	}
	const FTransform& Frame = Cloth->GetComponentTransform();
	auto ToWorld = [&Frame](FBilliardsVec2 P) { return Frame.TransformPosition(SimToLocal(P, 0.3)); };

	const FBilliardsTableLayout& Layout = Sim.Layout();
	for (const FBilliardsCushion& C : Layout.Cushions)
	{
		DrawDebugLine(World, ToWorld(C.A), ToWorld(C.B), FColor::Green, false, -1.0f, 0, 0.3f);
	}
	const FVector AxisX = Frame.GetUnitAxis(EAxis::X);
	const FVector AxisY = Frame.GetUnitAxis(EAxis::Y);
	for (const FBilliardsPocket& P : Layout.Pockets)
	{
		DrawDebugCircle(World, ToWorld(P.Center), P.Radius * Frame.GetScale3D().X, 24, FColor::Yellow, false, -1.0f, 0, 0.3f, AxisX, AxisY, false);
	}
	DrawDebugPoint(World, ToWorld(Layout.FootSpot), 8.0f, FColor::White, false, -1.0f);
	DrawDebugPoint(World, ToWorld(Layout.HeadSpot), 8.0f, FColor::White, false, -1.0f);
#endif
}

ASibeliusHUD* ABilliardsTable::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}
