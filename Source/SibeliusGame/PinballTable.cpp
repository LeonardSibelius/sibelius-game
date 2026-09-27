// PinballTable.cpp — see the header for what this actor is for and what it must never do.

#include "PinballTable.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: a unity build pastes files together, and a second file's
// DefaultLayout() would collide (the ProceduralPcm.h lesson).
namespace PinballTableDetail
{
	const FPinballTableLayout& DefaultLayout()
	{
		static const FPinballTableLayout Layout = FPinballTableLayout::MakeDefault();
		return Layout;
	}

	constexpr double BumperCapHeight = 3.0;   // cm
	constexpr double FlipperHeight = 2.5;
	constexpr float FlashDecayPerSecond = 6.0f;
}

APinballTable::APinballTable()
{
	PrimaryActorTick.bCanEverTick = true;

	const FPinballTableLayout& Layout = PinballTableDetail::DefaultLayout();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CabinetMesh(TEXT("/Game/SBG_RetroArcade/Meshes/SM_Pinballingalong.SM_Pinballingalong"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ChromeMaterial(TEXT("/Game/SBG_RetroArcade/Materials/M_aluminum.M_aluminum"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlipperMaterial(TEXT("/Game/SBG_RetroArcade/Materials/M_Flipper.M_Flipper"));
	/* Walt's masked material, Opacity Mask 0 (Content/Pinball, tracked in git). The first
	   choice was the engine's NaniteHiddenSectionMaterial, which on this non-Nanite mesh
	   drew the static flippers bright pink instead of hiding them. */
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HiddenMaterial(TEXT("/Game/Pinball/M_PinballInvisible.M_PinballInvisible"));

	auto MakeVisualOnly = [](UStaticMeshComponent* Component)
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetGenerateOverlapEvents(false);
	};

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	/* THE SINGALONG MESH IS BUILT FACING +X, with its backbox at the mesh's -X end, so it is
	   turned round here: every other default below has the player standing at -X looking up
	   the table along +X. (Walt found this in the editor before the mesh was measured.) */
	Cabinet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cabinet"));
	Cabinet->SetupAttachment(SceneRoot);
	MakeVisualOnly(Cabinet);
	Cabinet->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	if (CabinetMesh.Succeeded())
	{
		Cabinet->SetStaticMesh(CabinetMesh.Object);
	}

	// The cabinet's footprint once turned: player end at X -104, backbox at +51.5.
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetBoxExtent(FVector(79.0, 39.0, 89.0));
	Body->SetRelativeLocation(FVector(-26.0, 1.7, 89.0));
	Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->SetGenerateOverlapEvents(false);

	/* THE PLAYFIELD FRAME, FROM THE MESH (2026-09-27). Table (0, 0) is the playfield's
	   bottom-left corner as MakeDefault defines it - mesh (x 76, z 27.4) - on the fitted
	   playfield surface, height 85.40 there. The modelled playfield rises 2.21 degrees toward
	   the backbox (the physics still rolls the ball as if it were 6.5, the real-machine
	   figure; the eye does not see the difference, the hand does). Mesh to actor, with the
	   cabinet turned 180: actor = (-x, -z, y). If the drawn layout ever lands mirrored across
	   the table, that last step is the thing to check. */
	Playfield = CreateDefaultSubobject<USceneComponent>(TEXT("Playfield"));
	Playfield->SetupAttachment(SceneRoot);
	Playfield->SetMobility(EComponentMobility::Movable);
	Playfield->SetRelativeLocation(FVector(-76.0, -27.4, 85.40));
	Playfield->SetRelativeRotation(FRotator(2.21, 0.0, 0.0));

	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetupAttachment(Playfield);
	MakeVisualOnly(Ball);
	if (SphereMesh.Succeeded())
	{
		Ball->SetStaticMesh(SphereMesh.Object);   // the engine sphere is 100 cm across
	}
	if (ChromeMaterial.Succeeded())
	{
		Ball->SetMaterial(0, ChromeMaterial.Object);
	}
	Ball->SetRelativeScale3D(FVector(Layout.BallRadius * 2.0 / 100.0));
	Ball->SetRelativeLocation(TableToLocal(Layout.PlungerRest, Layout.BallRadius));

	// Flippers: a pivot that rotates, with a bat hung off it along the pivot's +X. The bat
	// is a flattened cube for now; a real flipper mesh can replace it without touching the
	// physics, because the physics never looks at it.
	auto MakeFlipper = [&](const FPinballFlipperDef& Def, const TCHAR* PivotName, const TCHAR* BatName,
		TObjectPtr<USceneComponent>& OutPivot, TObjectPtr<UStaticMeshComponent>& OutBat)
	{
		OutPivot = CreateDefaultSubobject<USceneComponent>(PivotName);
		OutPivot->SetupAttachment(Playfield);
		OutPivot->SetMobility(EComponentMobility::Movable);
		OutPivot->SetRelativeLocation(TableToLocal(Def.Pivot, 0.0));
		OutPivot->SetRelativeRotation(FRotator(0.0, 90.0 - FMath::RadiansToDegrees(Def.RestAngle), 0.0));

		OutBat = CreateDefaultSubobject<UStaticMeshComponent>(BatName);
		OutBat->SetupAttachment(OutPivot);
		MakeVisualOnly(OutBat);
		if (CubeMesh.Succeeded())
		{
			OutBat->SetStaticMesh(CubeMesh.Object);
		}
		if (FlipperMaterial.Succeeded())
		{
			OutBat->SetMaterial(0, FlipperMaterial.Object);
		}
		const double Span = Def.Length + Def.BaseRadius + Def.TipRadius;
		const double Thickness = Def.BaseRadius + Def.TipRadius;   // the average diameter
		OutBat->SetRelativeLocation(FVector((Def.Length + Def.TipRadius - Def.BaseRadius) * 0.5, 0.0, PinballTableDetail::FlipperHeight * 0.5));
		OutBat->SetRelativeScale3D(FVector(Span / 100.0, Thickness / 100.0, PinballTableDetail::FlipperHeight / 100.0));
	};
	MakeFlipper(Layout.Flippers[0], TEXT("LeftFlipperPivot"), TEXT("LeftFlipper"), LeftFlipperPivot, LeftFlipper);
	MakeFlipper(Layout.Flippers[1], TEXT("RightFlipperPivot"), TEXT("RightFlipper"), RightFlipperPivot, RightFlipper);

	for (int32 Index = 0; Index < static_cast<int32>(Layout.Bumpers.size()); ++Index)
	{
		const FPinballBumper& Bumper = Layout.Bumpers[static_cast<size_t>(Index)];

		UStaticMeshComponent* Cap = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("BumperCap%d"), Index));
		Cap->SetupAttachment(Playfield);
		MakeVisualOnly(Cap);
		if (CylinderMesh.Succeeded())
		{
			Cap->SetStaticMesh(CylinderMesh.Object);
		}
		const double Diameter = Bumper.Radius * 2.0;
		Cap->SetRelativeLocation(TableToLocal(Bumper.Center, PinballTableDetail::BumperCapHeight * 0.5));
		Cap->SetRelativeScale3D(FVector(Diameter / 100.0, Diameter / 100.0, PinballTableDetail::BumperCapHeight / 100.0));
		BumperCaps.Add(Cap);

		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("BumperLight%d"), Index));
		Light->SetupAttachment(Playfield);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetRelativeLocation(TableToLocal(Bumper.Center, 5.0));
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(0.0f);
		Light->SetAttenuationRadius(25.0f);
		Light->SetLightColor(FLinearColor(1.0f, 0.75f, 0.35f));
		Light->SetCastShadows(false);
		BumperLights.Add(Light);
	}

	// Just in front of the backglass, which faces the player at X +35.2 once the cabinet is
	// turned. Low on the glass, where a machine of that era kept its score.
	ScoreText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ScoreText"));
	ScoreText->SetupAttachment(SceneRoot);
	ScoreText->SetMobility(EComponentMobility::Movable);
	ScoreText->SetRelativeLocation(FVector(33.5, 0.0, 128.0));
	ScoreText->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	ScoreText->SetHorizontalAlignment(EHTA_Center);
	ScoreText->SetVerticalAlignment(EVRTA_TextCenter);
	ScoreText->SetWorldSize(5.0f);
	ScoreText->SetTextRenderColor(FColor(255, 196, 90));
	ScoreText->SetText(FText::FromString(TEXT("SINGALONG")));

	// Standing at the front of the cabinet (X -104), eyes above the lockdown bar, looking
	// up the length of the table.
	TableCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TableCamera"));
	TableCamera->SetupAttachment(SceneRoot);
	TableCamera->SetRelativeLocation(FVector(-140.0, 0.0, 158.0));
	TableCamera->SetRelativeRotation(FRotator(-30.0, 0.0, 0.0));
	TableCamera->SetFieldOfView(60.0f);

	if (HiddenMaterial.Succeeded())
	{
		HiddenFlipperMaterial = HiddenMaterial.Object;
	}
}

FVector APinballTable::TableToLocal(FPinVec2 P, double Height)
{
	// The frame's +X runs up the table and +Y across it, so table (X, Y) is local (Y, X).
	return FVector(P.Y, P.X, Height);
}

void APinballTable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Runs in the editor, so the cabinet shows the way it will in play while it is placed:
	// static flippers hidden, ball on the plunger, score on the glass.
	ApplyHiddenFlipperMaterial();
	for (UStaticMeshComponent* Cap : BumperCaps)
	{
		if (Cap)
		{
			Cap->SetVisibility(bShowBumperCaps);
		}
	}
	BumperFlash.Init(0.0f, BumperLights.Num());
	SyncVisuals(0.0f);
	bScoreTextValid = false;
	RefreshScoreText();
}

void APinballTable::BeginPlay()
{
	Super::BeginPlay();

	Sim = FPinballSim();
	ApplyHiddenFlipperMaterial();
	BumperFlash.Init(0.0f, BumperLights.Num());
	bScoreTextValid = false;
	RefreshScoreText();
	SyncVisuals(0.0f);
}

void APinballTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Whatever tore this down - a level travel, PIE stopping, the actor destroyed with him
	// at the table - he gets his controls and his own eyes back.
	StopPlaying();
	Super::EndPlay(EndPlayReason);
}

bool APinballTable::ShouldTickIfViewportsOnly() const
{
	return bDrawPhysicsLayoutInEditor;
}

void APinballTable::Tick(float DeltaSeconds)
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

	// The game only runs while he is at the table. Walk away mid-ball and it waits for you.
	if (bPlaying)
	{
		if (bPlungerHeld)
		{
			PlungerHeldSeconds += DeltaSeconds;
		}
		Sim.Advance(DeltaSeconds);
		HandleEvents();

		// A LEASE, renewed every frame: the crosshair and the objective stay off the table
		// while he plays, and come back by themselves if this actor stops ticking.
		if (ASibeliusHUD* Hud = GetHud())
		{
			Hud->HoldCinematic(0.25f);
		}
	}

	PlayQueuedSounds();   // outside bPlaying: a score still rings out after he steps away
	SyncVisuals(DeltaSeconds);
	RefreshScoreText();

	if (bDrawPhysicsLayoutInGame)
	{
		DrawPhysicsLayout();
	}
}

void APinballTable::Interact_Implementation(AActor* Interactor)
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

FText APinballTable::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("Pinball", "Prompt", "[E] play pinball");
}

void APinballTable::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	bPlungerHeld = false;
	PlungerHeldSeconds = 0.0f;

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);

	// Our bindings sit above the pawn's and consume their keys, so E here means "leave the
	// table", not "interact" (the SequenceCue pattern).
	EnableInput(PC);
	BindTableKeys();

	bScoreTextValid = false;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Pinball] at the table (%s)."), *GetActorNameOrLabel());
}

void APinballTable::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	bPlungerHeld = false;
	PlungerHeldSeconds = 0.0f;
	Sim.SetFlipper(0, false);
	Sim.SetFlipper(1, false);
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
	UE_LOG(LogSibeliusGame, Display, TEXT("[Pinball] left the table."));
}

void APinballTable::BindTableKeys()
{
	// Once. DisableInput takes the component off the input stack but keeps its bindings, so
	// binding again on every visit would fire each flipper once per visit.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;

	using FHandler = void (APinballTable::*)();
	auto Bind = [this](const FKey& Key, FHandler Pressed, FHandler Released)
	{
		InputComponent->BindKey(Key, IE_Pressed, this, Pressed);
		if (Released)
		{
			InputComponent->BindKey(Key, IE_Released, this, Released);
		}
	};

	// The two sides of the keyboard, the arrows, and the triggers: whatever his hands find.
	for (const FKey& Key : { EKeys::LeftShift, EKeys::Z, EKeys::Left, EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_LeftShoulder })
	{
		Bind(Key, &APinballTable::OnLeftFlipperPressed, &APinballTable::OnLeftFlipperReleased);
	}
	for (const FKey& Key : { EKeys::RightShift, EKeys::Slash, EKeys::Right, EKeys::Gamepad_RightTrigger, EKeys::Gamepad_RightShoulder })
	{
		Bind(Key, &APinballTable::OnRightFlipperPressed, &APinballTable::OnRightFlipperReleased);
	}
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::Enter, EKeys::Down, EKeys::Gamepad_FaceButton_Bottom })
	{
		Bind(Key, &APinballTable::OnPlungerPressed, &APinballTable::OnPlungerReleased);
	}
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		Bind(Key, &APinballTable::OnLeave, nullptr);
	}
}

void APinballTable::OnLeftFlipperPressed()   { Sim.SetFlipper(0, true); }
void APinballTable::OnLeftFlipperReleased()  { Sim.SetFlipper(0, false); }
void APinballTable::OnRightFlipperPressed()  { Sim.SetFlipper(1, true); }
void APinballTable::OnRightFlipperReleased() { Sim.SetFlipper(1, false); }

void APinballTable::OnPlungerPressed()
{
	const EPinballPhase Phase = Sim.Phase();
	if (Phase == EPinballPhase::Attract || Phase == EPinballPhase::GameOver)
	{
		// The same key starts the game. The release of this press must not also fire the
		// plunger, so no charge is started.
		Sim.StartGame();
		return;
	}
	if (Phase == EPinballPhase::BallReady)
	{
		bPlungerHeld = true;
		PlungerHeldSeconds = 0.0f;
	}
}

void APinballTable::OnPlungerReleased()
{
	if (!bPlungerHeld)
	{
		return;
	}
	bPlungerHeld = false;
	Sim.Launch(PlungerHeldSeconds / PlungerChargeSeconds);
}

void APinballTable::OnLeave()
{
	StopPlaying();
}

void APinballTable::SyncVisuals(float DeltaSeconds)
{
	const double R = Sim.Layout().BallRadius;
	if (Ball)
	{
		Ball->SetRelativeLocation(TableToLocal(Sim.BallPosition(), R));
		Ball->SetVisibility(Sim.Phase() != EPinballPhase::GameOver);
	}
	if (LeftFlipperPivot)
	{
		LeftFlipperPivot->SetRelativeRotation(FRotator(0.0, 90.0 - FMath::RadiansToDegrees(Sim.FlipperAngle(0)), 0.0));
	}
	if (RightFlipperPivot)
	{
		RightFlipperPivot->SetRelativeRotation(FRotator(0.0, 90.0 - FMath::RadiansToDegrees(Sim.FlipperAngle(1)), 0.0));
	}
	for (int32 Index = 0; Index < BumperLights.Num(); ++Index)
	{
		if (!BumperFlash.IsValidIndex(Index) || !BumperLights[Index])
		{
			continue;
		}
		BumperFlash[Index] = FMath::Max(0.0f, BumperFlash[Index] - DeltaSeconds * PinballTableDetail::FlashDecayPerSecond);
		BumperLights[Index]->SetIntensity(BumperFlash[Index] * BumperFlashIntensity);
	}
}

void APinballTable::HandleEvents()
{
	for (const FPinballEvent& Event : Sim.TakeEvents())
	{
		switch (Event.Type)
		{
		case EPinballEvent::GameStarted:
			// The score reels spinning back to zero: a rattle of steps.
			for (int32 Step = 0; Step < 10; ++Step)
			{
				QueueSound(ETableSound::ReelClick, 0.5f, 0.045 * Step);
			}
			break;
		case EPinballEvent::BallServed:
			QueueSound(ETableSound::KickoutIn, 0.35f, 0.0);   // the ball rolling into the lane
			break;
		case EPinballEvent::BallLaunched:
			TableSounds::Play(this, ETableSound::Plunger, 0.8f);
			break;
		case EPinballEvent::FlipperUp:
			TableSounds::Play(this, ETableSound::FlipperUp, 0.7f, FMath::FRandRange(0.97f, 1.03f));
			break;
		case EPinballEvent::FlipperDown:
			TableSounds::Play(this, ETableSound::FlipperDown, 0.35f);
			break;
		case EPinballEvent::BumperHit:
			if (BumperFlash.IsValidIndex(Event.Index))
			{
				BumperFlash[Event.Index] = 1.0f;
			}
			TableSounds::Play(this, ETableSound::Bumper, 0.9f, FMath::FRandRange(0.96f, 1.04f));
			QueueScoreChimes(Event.Points);
			break;
		case EPinballEvent::SlingshotHit:
			TableSounds::Play(this, ETableSound::Slingshot, 0.8f, FMath::FRandRange(0.96f, 1.04f));
			QueueScoreChimes(Event.Points);
			break;
		case EPinballEvent::RolloverLit:
		case EPinballEvent::RolloversComplete:
			QueueScoreChimes(Event.Points);
			break;
		case EPinballEvent::KickoutCaptured:
			TableSounds::Play(this, ETableSound::KickoutIn, 0.8f);
			QueueScoreChimes(Event.Points);
			break;
		case EPinballEvent::KickoutEjected:
			TableSounds::Play(this, ETableSound::Bumper, 0.8f, 0.75f);
			break;
		case EPinballEvent::BallDrained:
			TableSounds::Play(this, ETableSound::Drain, 0.8f);
			UE_LOG(LogSibeliusGame, Display, TEXT("[Pinball] ball %d drained at %d."), Event.Index, Sim.Score());
			break;
		case EPinballEvent::GameOver:
			HighScore = FMath::Max(HighScore, Event.Points);
			QueueSound(ETableSound::Chime1000, 0.8f, 0.6);
			UE_LOG(LogSibeliusGame, Display, TEXT("[Pinball] game over: %d (high %d)."), Event.Points, HighScore);
			break;
		default:
			break;
		}
	}
}

void APinballTable::QueueSound(ETableSound Sound, float Volume, double Delay)
{
	const UWorld* World = GetWorld();
	if (!World || SoundQueue.Num() >= 60)   // a hard cap: a stuck queue must not grow forever
	{
		return;
	}
	SoundQueue.Add(FQueuedSound{ World->GetTimeSeconds() + Delay, Sound, Volume });
}

void APinballTable::QueueScoreChimes(int32 Points)
{
	const UWorld* World = GetWorld();
	if (!World || Points <= 0)
	{
		return;
	}
	// One ring per digit, thousands first, a score-motor beat apart, after whatever is
	// still ringing. Anything under ten still rings the 10 chime once.
	constexpr double Beat = 0.11;
	const int32 Thousands = FMath::Min(Points / 1000, 9);
	const int32 Hundreds = (Points % 1000) / 100;
	const int32 Tens = FMath::Max((Points % 100) / 10, (Points < 10) ? 1 : 0);

	const double Now = World->GetTimeSeconds();
	double At = FMath::Max(Now, ChimeFreeAt);
	auto Ring = [this, &At, Now](ETableSound Chime, int32 Count)
	{
		for (int32 K = 0; K < Count; ++K)
		{
			QueueSound(Chime, 0.55f, At - Now);
			At += Beat;
		}
	};
	Ring(ETableSound::Chime1000, Thousands);
	Ring(ETableSound::Chime100, Hundreds);
	Ring(ETableSound::Chime10, Tens);
	// A busy table can outrun the motor; never let the backlog run more than two
	// seconds behind the ball.
	ChimeFreeAt = FMath::Min(At, Now + 2.0);
}

void APinballTable::PlayQueuedSounds()
{
	const UWorld* World = GetWorld();
	if (!World || SoundQueue.Num() == 0)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (int32 I = SoundQueue.Num() - 1; I >= 0; --I)
	{
		if (SoundQueue[I].At <= Now)
		{
			TableSounds::Play(this, SoundQueue[I].Sound, SoundQueue[I].Volume);
			SoundQueue.RemoveAtSwap(I);
		}
	}
}

void APinballTable::RefreshScoreText()
{
	if (!ScoreText)
	{
		return;
	}
	const int32 Score = Sim.Score();
	const int32 BallNumber = Sim.BallNumber();
	const EPinballPhase Phase = Sim.Phase();
	if (bScoreTextValid && Score == ShownScore && BallNumber == ShownBall && Phase == ShownPhase)
	{
		return;
	}

	FString Text;
	switch (Phase)
	{
	case EPinballPhase::Attract:
		// The first time he stands here this is the only instruction he gets, so it names
		// the keys.
		Text = HighScore > 0
			? FString::Printf(TEXT("HIGH SCORE %d\nENTER TO PLAY"), HighScore)
			: FString(TEXT("SINGALONG\nENTER TO PLAY\nSHIFT FLIPS - SPACE PLUNGES"));
		break;
	case EPinballPhase::BallReady:
	case EPinballPhase::InPlay:
		Text = FString::Printf(TEXT("%d\nBALL %d"), Score, BallNumber);
		break;
	case EPinballPhase::GameOver:
		Text = FString::Printf(TEXT("GAME OVER  %d\nHIGH %d - ENTER TO PLAY"), Score, HighScore);
		break;
	}
	ScoreText->SetText(FText::FromString(Text));

	ShownScore = Score;
	ShownBall = BallNumber;
	ShownPhase = Phase;
	bScoreTextValid = true;
}

void APinballTable::ApplyHiddenFlipperMaterial()
{
	if (!Cabinet || !HiddenFlipperMaterial || HideMaterialsNamed.IsEmpty())
	{
		return;
	}
	const UStaticMesh* Mesh = Cabinet->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}
	// Ask the MESH which slot is which, not the component: once overridden, the component
	// reports our material, and the name test would stop finding the slot.
	for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
	{
		const UMaterialInterface* Original = Mesh->GetMaterial(Slot);
		if (Original && Original->GetName().Contains(HideMaterialsNamed))
		{
			Cabinet->SetMaterial(Slot, HiddenFlipperMaterial);
		}
	}
}

void APinballTable::DrawPhysicsLayout() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World || !Playfield)
	{
		return;
	}
	const FTransform& Frame = Playfield->GetComponentTransform();
	const FVector AxisX = Frame.GetUnitAxis(EAxis::X);
	const FVector AxisY = Frame.GetUnitAxis(EAxis::Y);
	const double Scale = Frame.GetScale3D().X;
	auto ToWorld = [&Frame](FPinVec2 P) { return Frame.TransformPosition(TableToLocal(P, 0.3)); };

	const FPinballTableLayout& Layout = Sim.Layout();
	for (const FPinballSegment& Segment : Layout.Segments)
	{
		const FColor Color = Segment.Kind == EPinballSurface::Slingshot ? FColor::Orange
			: Segment.Kind == EPinballSurface::OneWayGate ? FColor::Cyan
			: Segment.Kind == EPinballSurface::Rubber ? FColor::White
			: FColor::Green;
		DrawDebugLine(World, ToWorld(Segment.A), ToWorld(Segment.B), Color, false, -1.0f, 0, 0.25f);
	}
	for (const FPinballBumper& Bumper : Layout.Bumpers)
	{
		DrawDebugCircle(World, ToWorld(Bumper.Center), Bumper.Radius * Scale, 24, FColor::Yellow, false, -1.0f, 0, 0.25f, AxisX, AxisY, false);
	}
	for (const FPinballRollover& Rollover : Layout.Rollovers)
	{
		DrawDebugCircle(World, ToWorld(Rollover.Center), Rollover.Radius * Scale, 16, FColor::Magenta, false, -1.0f, 0, 0.2f, AxisX, AxisY, false);
	}
	// The posts are the alignment check: every white ring should sit on a modelled post.
	for (const FPinballPost& Post : Layout.Posts)
	{
		DrawDebugCircle(World, ToWorld(Post.Center), Post.Radius * Scale, 12, FColor::White, false, -1.0f, 0, 0.2f, AxisX, AxisY, false);
	}
	for (const FPinballKickout& Hole : Layout.Kickouts)
	{
		DrawDebugCircle(World, ToWorld(Hole.Center), Hole.Radius * Scale, 16, FColor::Blue, false, -1.0f, 0, 0.3f, AxisX, AxisY, false);
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		DrawDebugLine(World, ToWorld(Layout.Flippers[Side].Pivot), ToWorld(Sim.FlipperTip(Side)), FColor::White, false, -1.0f, 0, 0.6f);
	}
	DrawDebugLine(World, ToWorld(FPinVec2(0.0, Layout.DrainY)), ToWorld(FPinVec2(Layout.ShooterLaneMinX, Layout.DrainY)), FColor::Red, false, -1.0f, 0, 0.25f);
#endif
}

ASibeliusHUD* APinballTable::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}
