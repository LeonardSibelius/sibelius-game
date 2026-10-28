// HyperBilliardsTable.cpp — see the header for what this actor shows and how he plays it.

#include "HyperBilliardsTable.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"
#include "TableSounds.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace HyperTableDetail
{
	const FHyperTableLayout& DefaultLayout()
	{
		static const FHyperTableLayout Layout = FHyperTableLayout::MakeDefault();
		return Layout;
	}

	// The cloth's top above the dais, where the actor's origin is.
	constexpr double ClothHeight = 85.0;
	constexpr double RailWidth = 10.0;
	constexpr double RailRise = 7.0;      // above the cloth
	constexpr double GlowSize = 1.2;

	// The living room's ball meshes are 7.0 cm across, centred; the sim's balls are 10.
	constexpr double MeshBallRadius = 3.5;
	constexpr double HiddenSlice = 0.3;   // a slice thinner than this is not drawn

	// The cue mesh, as at the living-room table (BilliardsTable.cpp).
	constexpr double CueHalfLength = 76.98;
	constexpr double CueTipGap = 1.5;
	constexpr double CuePullBack = 28.0;
	constexpr double CueElevationDegrees = 6.0;

	constexpr double GuideHeight = 0.25;
	constexpr double GuideThickness = 0.6;
	constexpr double ObjectLineLength = 35.0;
	constexpr double SpinStep = 0.25;
	constexpr double MaxTiltDegrees = 75.0;

	// The W-board: behind the far (+Y) rail, raised, facing the player at -Y. Its X is the sim's
	// X and its Z the sim's W, one to one, so a dot hangs straight above its ball.
	constexpr double BoardBack = 40.0;      // its face this far beyond the far rail
	constexpr double BoardRaise = 75.0;     // its centre this far above the cloth
	constexpr double BoardMargin = 12.0;
	constexpr double DotOut = 5.0;          // dots stand this far in front of its face

	// The engine's basic shapes are 100 cm across.
	constexpr double ShapeSize = 100.0;

	// The six places a pocket can open, in the cloth's X-Y: the 12 pockets of the sim, which
	// sit in pairs at -W and +W over each of these.
	inline FVector2D PocketSpot(int32 I, const FHyperTableLayout& L)
	{
		static const double Xs[6] = { -1.0, 1.0, -1.0, 1.0, 0.0, 0.0 };
		static const double Ys[6] = { -1.0, -1.0, 1.0, 1.0, -1.0, 1.0 };
		return FVector2D(Xs[I] * L.HalfLength, Ys[I] * L.HalfWidth);
	}

	// Paint: (base colour, glow). Glow 0 is a plain painted surface.
	struct FPaint
	{
		FLinearColor Color;
		float Glow = 0.0f;
	};
	const FPaint FeltPaint{ FLinearColor(0.02f, 0.20f, 0.10f), 0.0f };
	const FPaint WoodPaint{ FLinearColor(0.05f, 0.025f, 0.012f), 0.0f };
	const FPaint HolePaint{ FLinearColor(0.0f, 0.0f, 0.0f), 0.0f };
	const FPaint BoardPaint{ FLinearColor(0.008f, 0.012f, 0.03f), 0.0f };
	const FPaint RailGlowPaint{ FLinearColor(0.15f, 0.75f, 1.0f), 6.0f };
	const FPaint SlicePaint{ FLinearColor(0.15f, 0.85f, 1.0f), 14.0f };
	const FPaint BoardPocketPaint{ FLinearColor(1.0f, 0.2f, 0.6f), 1.5f };
	constexpr double BoardPocketSize = 7.0;   // a pocket's marker on the board, cm across
	const FPaint AimPaint{ FLinearColor(1.0f, 0.7f, 0.2f), 6.0f };
}

AHyperBilliardsTable::AHyperBilliardsTable()
{
	using namespace HyperTableDetail;
	PrimaryActorTick.bCanEverTick = true;

	const FHyperTableLayout& Layout = DefaultLayout();
	Sim = FHyperBilliardsSim(Layout);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CueBallMesh(TEXT("/Game/Billiard_Table/Meshes/SM_White_Billiard_Ball.SM_White_Billiard_Ball"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CueMesh(TEXT("/Game/Billiard_Table/Meshes/SM_Billiard_Cue.SM_Billiard_Cue"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlainMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/ModularSciFiEnv_K/Materials/Base/M_LampEmiss_MAT.M_LampEmiss_MAT"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleMaterial(TEXT("/Game/StainedGlass3D/Materials/M_BlackMarbleFloor.M_BlackMarbleFloor"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	auto MakeVisualOnly = [](UStaticMeshComponent* Component)
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetGenerateOverlapEvents(false);
	};
	// A box of engine shape, Size cm (X, Y, Z), centred at Where, under Parent.
	auto MakeShape = [&](const TCHAR* Name, USceneComponent* Parent, ConstructorHelpers::FObjectFinder<UStaticMesh>& Shape,
		UMaterialInterface* Material, FVector Size, FVector Where) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		MakeVisualOnly(Part);
		if (Shape.Succeeded())
		{
			Part->SetStaticMesh(Shape.Object);
		}
		if (Material)
		{
			Part->SetMaterial(0, Material);
		}
		Part->SetRelativeLocation(Where);
		Part->SetRelativeScale3D(Size / ShapeSize);
		return Part;
	};
	UMaterialInterface* Plain = PlainMaterial.Succeeded() ? PlainMaterial.Object.Get() : nullptr;
	UMaterialInterface* Glow = GlowMaterial.Succeeded() ? GlowMaterial.Object.Get() : nullptr;
	UMaterialInterface* Marble = MarbleMaterial.Succeeded() ? MarbleMaterial.Object.Get() : Plain;

	const double HL = Layout.HalfLength;
	const double HW = Layout.HalfWidth;
	const double HD = Layout.HalfDepth;
	const double OuterL = 2.0 * (HL + RailWidth);
	const double OuterW = 2.0 * (HW + RailWidth);

	// The dais: 8 m across, its top the origin, 1.5 m deep so it meets a slope.
	Dais = MakeShape(TEXT("Dais"), SceneRoot, CylinderMesh, Marble, FVector(800.0, 800.0, 150.0), FVector(0.0, 0.0, -75.0));
	Dais->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Base = MakeShape(TEXT("Base"), SceneRoot, CubeMesh, Plain, FVector(OuterL - 10.0, OuterW - 10.0, ClothHeight - 4.0),
		FVector(0.0, 0.0, (ClothHeight - 4.0) * 0.5));
	Felt = MakeShape(TEXT("Felt"), SceneRoot, CubeMesh, Plain, FVector(OuterL, OuterW, 4.0), FVector(0.0, 0.0, ClothHeight - 2.0));

	const double RailZ = ClothHeight - 4.0 + (RailRise + 4.0) * 0.5;
	const double RailH = RailRise + 4.0;
	Rails.Add(MakeShape(TEXT("RailSouth"), SceneRoot, CubeMesh, Plain, FVector(OuterL, RailWidth, RailH), FVector(0.0, -(HW + RailWidth * 0.5), RailZ)));
	Rails.Add(MakeShape(TEXT("RailNorth"), SceneRoot, CubeMesh, Plain, FVector(OuterL, RailWidth, RailH), FVector(0.0, HW + RailWidth * 0.5, RailZ)));
	Rails.Add(MakeShape(TEXT("RailHead"), SceneRoot, CubeMesh, Plain, FVector(RailWidth, 2.0 * HW, RailH), FVector(-(HL + RailWidth * 0.5), 0.0, RailZ)));
	Rails.Add(MakeShape(TEXT("RailFoot"), SceneRoot, CubeMesh, Plain, FVector(RailWidth, 2.0 * HW, RailH), FVector(HL + RailWidth * 0.5, 0.0, RailZ)));

	const double GlowZ = ClothHeight + RailRise + GlowSize * 0.5;
	RailGlows.Add(MakeShape(TEXT("GlowSouth"), SceneRoot, CubeMesh, Glow, FVector(2.0 * HL, GlowSize, GlowSize), FVector(0.0, -(HW + GlowSize * 0.5), GlowZ)));
	RailGlows.Add(MakeShape(TEXT("GlowNorth"), SceneRoot, CubeMesh, Glow, FVector(2.0 * HL, GlowSize, GlowSize), FVector(0.0, HW + GlowSize * 0.5, GlowZ)));
	RailGlows.Add(MakeShape(TEXT("GlowHead"), SceneRoot, CubeMesh, Glow, FVector(GlowSize, 2.0 * HW, GlowSize), FVector(-(HL + GlowSize * 0.5), 0.0, GlowZ)));
	RailGlows.Add(MakeShape(TEXT("GlowFoot"), SceneRoot, CubeMesh, Glow, FVector(GlowSize, 2.0 * HW, GlowSize), FVector(HL + GlowSize * 0.5, 0.0, GlowZ)));

	for (int32 I = 0; I < 6; ++I)
	{
		const FVector2D Spot = PocketSpot(I, Layout);
		UStaticMeshComponent* Mouth = MakeShape(*FString::Printf(TEXT("PocketMouth%d"), I), SceneRoot, CylinderMesh, Plain,
			FVector(1.0, 1.0, 0.3), FVector(Spot.X, Spot.Y, ClothHeight + 0.15));
		Mouth->SetVisibility(false);
		PocketMouths.Add(Mouth);
	}

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetBoxExtent(FVector(HL + RailWidth, HW + RailWidth, (ClothHeight + RailRise) * 0.5));
	Body->SetRelativeLocation(FVector(0.0, 0.0, (ClothHeight + RailRise) * 0.5));
	Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->SetGenerateOverlapEvents(false);

	Cloth = CreateDefaultSubobject<USceneComponent>(TEXT("Cloth"));
	Cloth->SetupAttachment(SceneRoot);
	Cloth->SetMobility(EComponentMobility::Movable);
	Cloth->SetRelativeLocation(FVector(0.0, 0.0, ClothHeight));

	Board = CreateDefaultSubobject<USceneComponent>(TEXT("Board"));
	Board->SetupAttachment(SceneRoot);
	Board->SetMobility(EComponentMobility::Movable);
	Board->SetRelativeLocation(FVector(0.0, HW + RailWidth + BoardBack, ClothHeight + BoardRaise));

	BoardPanel = MakeShape(TEXT("BoardPanel"), Board, CubeMesh, Plain,
		FVector(2.0 * (HL + BoardMargin), 2.0, 2.0 * (HD + BoardMargin)), FVector(0.0, 1.0, 0.0));
	BoardPanel->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BoardSlice = MakeShape(TEXT("BoardSlice"), Board, CubeMesh, Glow, FVector(2.0 * (HL + BoardMargin), 0.8, 0.8), FVector(0.0, -1.0, 0.0));
	BoardAim = MakeShape(TEXT("BoardAim"), Board, CubeMesh, Glow, FVector(10.0, 0.6, 0.6), FVector(0.0, -DotOut, 0.0));
	BoardAim->SetVisibility(false);

	// The pockets on the board: X at the corners and the middle, W at both ends - six places,
	// each the two pockets at -Y and +Y. A disc whose axis points at the player.
	for (int32 I = 0; I < 6; ++I)
	{
		const double X = (I % 3 - 1) * HL;
		const double W = I < 3 ? -HD : HD;
		UStaticMeshComponent* Ring = MakeShape(*FString::Printf(TEXT("BoardPocket%d"), I), Board, CylinderMesh, Glow,
			FVector(2.0 * Layout.PocketCapture, 2.0 * Layout.PocketCapture, 0.5), FVector(X, -0.5, W));
		Ring->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
		BoardPockets.Add(Ring);
	}

	BallMeshes.SetNum(FHyperBilliardsSim::BallCount);
	BoardDots.SetNum(FHyperBilliardsSim::BallCount);
	for (int32 I = 0; I < FHyperBilliardsSim::BallCount; ++I)
	{
		UStaticMesh* Mesh = CueBallMesh.Object;
		if (I > 0)
		{
			const FString Name = FString::Printf(TEXT("SM_Billiard_Ball_%02d"), I);
			ConstructorHelpers::FObjectFinder<UStaticMesh> Numbered(*FString::Printf(TEXT("/Game/Billiard_Table/Meshes/%s.%s"), *Name, *Name));
			Mesh = Numbered.Object;
		}
		UStaticMeshComponent* BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Ball%02d"), I));
		BallMesh->SetupAttachment(Cloth);
		MakeVisualOnly(BallMesh);
		UStaticMeshComponent* Dot = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Dot%02d"), I));
		Dot->SetupAttachment(Board);
		MakeVisualOnly(Dot);
		Dot->SetCastShadow(false);
		if (Mesh)
		{
			BallMesh->SetStaticMesh(Mesh);
			Dot->SetStaticMesh(Mesh);
		}
		BallMeshes[I] = BallMesh;
		BoardDots[I] = Dot;
	}

	Cue = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cue"));
	Cue->SetupAttachment(Cloth);
	MakeVisualOnly(Cue);
	if (CueMesh.Succeeded())
	{
		Cue->SetStaticMesh(CueMesh.Object);
	}
	Cue->SetVisibility(false);

	AimLine = MakeShape(TEXT("AimLine"), Cloth, CubeMesh, Glow, FVector(10.0, GuideThickness, 0.1), FVector::ZeroVector);
	ObjectLine = MakeShape(TEXT("ObjectLine"), Cloth, CubeMesh, Glow, FVector(10.0, GuideThickness, 0.1), FVector::ZeroVector);
	for (UStaticMeshComponent* Guide : { AimLine.Get(), ObjectLine.Get() })
	{
		Guide->SetCastShadow(false);
		Guide->SetVisibility(false);
	}

	TableCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TableCamera"));
	TableCamera->SetupAttachment(SceneRoot);
	TableCamera->SetFieldOfView(70.0f);

	BallRotations.Init(FQuat::Identity, FHyperBilliardsSim::BallCount);
	SyncCamera(0.0f, true);
}

FVector AHyperBilliardsTable::SimToCloth(FHyperVec3 P, double Height)
{
	// The sim's X-Y IS the cloth's, and Unreal's yaw turns X toward Y as the sim's heading does.
	return FVector(P.X, P.Y, Height);
}

FVector AHyperBilliardsTable::SimToBoard(FHyperVec3 P)
{
	return FVector(P.X, -HyperTableDetail::DotOut, P.W);
}

void AHyperBilliardsTable::ApplyPaint()
{
	using namespace HyperTableDetail;
	auto Paint = [](UStaticMeshComponent* Part, const FPaint& With)
	{
		if (!Part)
		{
			return;
		}
		UMaterialInstanceDynamic* MID = Part->CreateAndSetMaterialInstanceDynamic(0);
		if (!MID)
		{
			return;
		}
		// The plain shapes' material takes "Color"; the kit's lamp takes the rest.
		MID->SetVectorParameterValue(TEXT("Color"), With.Color);
		MID->SetVectorParameterValue(TEXT("BaseColor"), With.Color);
		if (With.Glow > 0.0f)
		{
			MID->SetVectorParameterValue(TEXT("Emissive"), With.Color);
			MID->SetScalarParameterValue(TEXT("Intens"), With.Glow);
			MID->SetScalarParameterValue(TEXT("TurnOn"), 1.0f);
		}
	};
	Paint(Felt, FeltPaint);
	Paint(Base, WoodPaint);
	for (UStaticMeshComponent* Rail : Rails)
	{
		Paint(Rail, WoodPaint);
	}
	for (UStaticMeshComponent* Glow : RailGlows)
	{
		Paint(Glow, RailGlowPaint);
	}
	for (UStaticMeshComponent* Mouth : PocketMouths)
	{
		Paint(Mouth, HolePaint);
	}
	Paint(BoardPanel, BoardPaint);
	Paint(BoardSlice, SlicePaint);
	Paint(BoardAim, AimPaint);
	for (UStaticMeshComponent* Ring : BoardPockets)
	{
		Paint(Ring, BoardPocketPaint);
		// Small markers, not the pockets' full 24 cm capture: drawn that size they outshone the
		// balls on the board and lit the dais pink (Walt's first look, 2026-10-25).
		if (Ring)
		{
			Ring->SetRelativeScale3D(FVector(BoardPocketSize / ShapeSize, BoardPocketSize / ShapeSize, 0.5 / ShapeSize));
		}
	}
	Paint(AimLine, RailGlowPaint);
	Paint(ObjectLine, AimPaint);
}

void AHyperBilliardsTable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPaint();
	SyncBalls(0.0f);
	SyncPockets();
	SyncBoard();
	SyncCamera(0.0f, true);
}

void AHyperBilliardsTable::BeginPlay()
{
	Super::BeginPlay();
	Sim = FHyperBilliardsSim(HyperTableDetail::DefaultLayout());
	BallRotations.Init(FQuat::Identity, FHyperBilliardsSim::BallCount);
	Heading = Tilt = Spin = 0.0;
	SliceW = SliceTarget = 0.0;
	ApplyPaint();
	SyncBalls(0.0f);
	SyncPockets();
	SyncBoard();
	SyncCamera(0.0f, true);
}

void AHyperBilliardsTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Whatever tore this down - a level travel, PIE stopping - he gets his controls and his eyes back.
	StopPlaying();
	Super::EndPlay(EndPlayReason);
}

void AHyperBilliardsTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// The balls only roll while he is at the table: walk away mid-stroke and they wait.
	if (bPlaying)
	{
		UpdateAim(DeltaSeconds);
		Sim.Advance(DeltaSeconds);
		HandleEvents();
		if (ASibeliusHUD* Hud = GetHud())
		{
			Hud->HoldCinematic(0.25f);   // the lease, renewed every frame (the pinball's pattern)
		}
	}
	UpdateSlice(DeltaSeconds);

	SyncBalls(DeltaSeconds);
	SyncPockets();
	SyncCueAndGuides();
	SyncBoard();
	SyncCamera(DeltaSeconds, false);
	RefreshPanel();
}

void AHyperBilliardsTable::Interact_Implementation(AActor* Interactor)
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

FText AHyperBilliardsTable::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("HyperBilliards", "Prompt", "[E] play 4D billiards");
}

void AHyperBilliardsTable::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	bCharging = false;
	ChargeSeconds = 0.0f;
	bAimLeftHeld = bAimRightHeld = bTiltUpHeld = bTiltDownHeld = bSliceUpHeld = bSliceDownHeld = bFineHeld = false;
	PendingMouseAim = 0.0f;

	bPreviousCursor = PC->bShowMouseCursor;
	PC->bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
	PC->SetInputMode(InputMode);
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	// HIS BODY STEPS ASIDE. The pawn's own actions are Enhanced Input, which our key bindings do
	// not consume: SPACE also made him jump (Walt, 2026-10-25: "manny jumps into the air when you
	// press space"). And seen from the table's camera, not his own, his body stood in the shot.
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
		Pawn->SetActorHiddenInGame(true);
	}
	SyncCamera(0.0f, true);
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);

	// Our bindings sit above the pawn's and consume their keys, so E here means "leave".
	EnableInput(PC);
	BindTableKeys();
	UE_LOG(LogSibeliusGame, Display, TEXT("[HyperBilliards] at the table (%s)."), *GetActorNameOrLabel());
}

void AHyperBilliardsTable::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	bCharging = false;
	ChargeSeconds = 0.0f;
	bAimLeftHeld = bAimRightHeld = bTiltUpHeld = bTiltDownHeld = bSliceUpHeld = bSliceDownHeld = bFineHeld = false;
	Sim.TakeEvents();

	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->bShowMouseCursor = bPreviousCursor;
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->EnableInput(PC);
			Pawn->SetActorHiddenInGame(false);
			PC->SetViewTargetWithBlend(Pawn, 0.4f, VTBlend_Cubic);
		}
		DisableInput(PC);
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ReleaseCinematic();
		}
	}
	PlayingPC.Reset();
	UE_LOG(LogSibeliusGame, Display, TEXT("[HyperBilliards] left the table."));
}

void AHyperBilliardsTable::BindTableKeys()
{
	// Once: DisableInput keeps the bindings, so binding on every visit would double them.
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;

	using FHandler = void (AHyperBilliardsTable::*)();
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
		Bind(Key, &AHyperBilliardsTable::OnAimLeftPressed, &AHyperBilliardsTable::OnAimLeftReleased);
	}
	for (const FKey& Key : { EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right })
	{
		Bind(Key, &AHyperBilliardsTable::OnAimRightPressed, &AHyperBilliardsTable::OnAimRightReleased);
	}
	for (const FKey& Key : { EKeys::Up, EKeys::Gamepad_DPad_Up })
	{
		Bind(Key, &AHyperBilliardsTable::OnTiltUpPressed, &AHyperBilliardsTable::OnTiltUpReleased);
	}
	for (const FKey& Key : { EKeys::Down, EKeys::Gamepad_DPad_Down })
	{
		Bind(Key, &AHyperBilliardsTable::OnTiltDownPressed, &AHyperBilliardsTable::OnTiltDownReleased);
	}
	for (const FKey& Key : { EKeys::Q, EKeys::PageUp })
	{
		Bind(Key, &AHyperBilliardsTable::OnSliceUpPressed, &AHyperBilliardsTable::OnSliceUpReleased);
	}
	for (const FKey& Key : { EKeys::C, EKeys::PageDown })
	{
		Bind(Key, &AHyperBilliardsTable::OnSliceDownPressed, &AHyperBilliardsTable::OnSliceDownReleased);
	}
	Bind(EKeys::MouseScrollUp, &AHyperBilliardsTable::OnWheelUp, nullptr);
	Bind(EKeys::MouseScrollDown, &AHyperBilliardsTable::OnWheelDown, nullptr);
	for (const FKey& Key : { EKeys::F })
	{
		Bind(Key, &AHyperBilliardsTable::OnSliceToCue, nullptr);
	}
	Bind(EKeys::T, &AHyperBilliardsTable::OnNextBall, nullptr);
	for (const FKey& Key : { EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl, EKeys::Gamepad_LeftShoulder })
	{
		Bind(Key, &AHyperBilliardsTable::OnFinePressed, &AHyperBilliardsTable::OnFineReleased);
	}
	Bind(EKeys::W, &AHyperBilliardsTable::OnSpinUp, nullptr);
	Bind(EKeys::S, &AHyperBilliardsTable::OnSpinDown, nullptr);
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_RightTrigger })
	{
		Bind(Key, &AHyperBilliardsTable::OnShotPressed, &AHyperBilliardsTable::OnShotReleased);
	}
	for (const FKey& Key : { EKeys::Enter, EKeys::R, EKeys::Gamepad_Special_Right })
	{
		Bind(Key, &AHyperBilliardsTable::OnRack, nullptr);
	}
	for (const FKey& Key : { EKeys::V, EKeys::Tab, EKeys::Gamepad_FaceButton_Top })
	{
		Bind(Key, &AHyperBilliardsTable::OnToggleView, nullptr);
	}
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		Bind(Key, &AHyperBilliardsTable::OnLeave, nullptr);
	}
	Bind(EKeys::LeftBracket, &AHyperBilliardsTable::OnPowerDown, nullptr);
	Bind(EKeys::RightBracket, &AHyperBilliardsTable::OnPowerUp, nullptr);
	Bind(EKeys::Gamepad_LeftThumbstick, &AHyperBilliardsTable::OnPowerDown, nullptr);
	Bind(EKeys::Gamepad_RightThumbstick, &AHyperBilliardsTable::OnPowerUp, nullptr);
	Bind(EKeys::BackSpace, &AHyperBilliardsTable::OnUndo, nullptr);
	Bind(EKeys::P, &AHyperBilliardsTable::OnPractice, nullptr);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AHyperBilliardsTable::OnCancel);
	Bind(EKeys::LeftMouseButton, &AHyperBilliardsTable::OnPickTarget, nullptr);
	Bind(EKeys::Gamepad_FaceButton_Left, &AHyperBilliardsTable::OnNextBall, nullptr);
	Bind(EKeys::Gamepad_LeftTrigger, &AHyperBilliardsTable::OnUndo, nullptr);
	Bind(EKeys::Gamepad_Special_Left, &AHyperBilliardsTable::OnPractice, nullptr);
	Bind(EKeys::Gamepad_RightShoulder, &AHyperBilliardsTable::OnToggleAssist, nullptr);
	Bind(EKeys::G, &AHyperBilliardsTable::OnToggleAssist, nullptr);
	Bind(EKeys::H, &AHyperBilliardsTable::OnToggleHelp, nullptr);
	InputComponent->BindAxisKey(EKeys::MouseX, this, &AHyperBilliardsTable::OnMouseX);
}

void AHyperBilliardsTable::OnAimLeftPressed()     { bAimLeftHeld = true; }
void AHyperBilliardsTable::OnAimLeftReleased()    { bAimLeftHeld = false; }
void AHyperBilliardsTable::OnAimRightPressed()    { bAimRightHeld = true; }
void AHyperBilliardsTable::OnAimRightReleased()   { bAimRightHeld = false; }
void AHyperBilliardsTable::OnTiltUpPressed()      { bTiltUpHeld = true; }
void AHyperBilliardsTable::OnTiltUpReleased()     { bTiltUpHeld = false; }
void AHyperBilliardsTable::OnTiltDownPressed()    { bTiltDownHeld = true; }
void AHyperBilliardsTable::OnTiltDownReleased()   { bTiltDownHeld = false; }
void AHyperBilliardsTable::OnSliceUpPressed()     { bSliceUpHeld = true; }
void AHyperBilliardsTable::OnSliceUpReleased()    { bSliceUpHeld = false; }
void AHyperBilliardsTable::OnSliceDownPressed()   { bSliceDownHeld = true; }
void AHyperBilliardsTable::OnSliceDownReleased()  { bSliceDownHeld = false; }
void AHyperBilliardsTable::OnFinePressed()        { bFineHeld = true; }
void AHyperBilliardsTable::OnFineReleased()       { bFineHeld = false; }
void AHyperBilliardsTable::OnMouseX(float Value)  {
	if (PlayingPC.IsValid() && PlayingPC->IsInputKeyDown(EKeys::RightMouseButton)) PendingMouseAim += Value;
}

void AHyperBilliardsTable::OnWheelUp()
{
	const double HD = Sim.Layout().HalfDepth;
	SliceTarget = FMath::Clamp(SliceTarget + SliceStepPerNotch, -HD, HD);
}

void AHyperBilliardsTable::OnWheelDown()
{
	const double HD = Sim.Layout().HalfDepth;
	SliceTarget = FMath::Clamp(SliceTarget - SliceStepPerNotch, -HD, HD);
}

void AHyperBilliardsTable::OnSliceToCue()
{
	if (Sim.IsOnTable(FHyperBilliardsSim::CueBall))
	{
		SliceTarget = Sim.BallPosition(FHyperBilliardsSim::CueBall).W;
	}
}

// T: aim at the nearest ball, then the next nearest, and so on round - straight at its centre in
// all three directions, so even a ball far off in W can be lined up without seeing it.
void AHyperBilliardsTable::OnNextBall()
{
	if (Sim.Phase() != EHyperPhase::Aiming || !Sim.IsOnTable(FHyperBilliardsSim::CueBall))
	{
		return;
	}
	const FHyperVec3 CuePos = Sim.BallPosition(FHyperBilliardsSim::CueBall);
	TArray<TPair<double, int32>> ByDistance;
	for (int32 I = 1; I < FHyperBilliardsSim::BallCount; ++I)
	{
		if (Sim.IsOnTable(I))
		{
			const FHyperVec3 D = Sim.BallPosition(I) - CuePos;
			ByDistance.Add(TPair<double, int32>(D.X * D.X + D.Y * D.Y + D.W * D.W, I));
		}
	}
	if (ByDistance.Num() == 0)
	{
		return;
	}
	ByDistance.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
	NextBallPick = NextBallPick % ByDistance.Num();
	SelectedBall = ByDistance[NextBallPick].Value;
	AimAtBall(SelectedBall);
	OnSliceToCue();
	++NextBallPick;
}

void AHyperBilliardsTable::OnSpinUp()
{
	Spin = FMath::Clamp(Spin + HyperTableDetail::SpinStep, -1.0, 1.0);
}

void AHyperBilliardsTable::OnSpinDown()
{
	Spin = FMath::Clamp(Spin - HyperTableDetail::SpinStep, -1.0, 1.0);
}

void AHyperBilliardsTable::OnShotPressed()
{
	const EHyperPhase Phase = Sim.Phase();
	if (Phase == EHyperPhase::Idle || Phase == EHyperPhase::Cleared)
	{
		if (bPractice && Phase == EHyperPhase::Cleared) PracticeLesson = (PracticeLesson + 1) % 3;
		OnRack();   // releasing the advance button must not fire
		return;
	}
	if (Phase == EHyperPhase::Aiming)
	{
		bCharging = true;
		ChargeSeconds = 0.0f;
	}
}

void AHyperBilliardsTable::OnShotReleased()
{
	if (!bCharging)
	{
		return;
	}
	SavedSelectedBall = SelectedBall;
	LastShotFeedback = AimFeedback();
	bShotHitObject = false;
	bExpectedContact = Sim.PredictShot(Heading, Tilt).bHitsBall;
	SavedPower = SelectedPower;
	SavedHeading = Heading; SavedTilt = Tilt; SavedSpin = Spin;
	BeforeShot = Sim;
	bCanUndo = true;
	const float Power = CurrentPower();
	bCharging = false;
	Sim.Shoot(Heading, Tilt, Power, Spin);
}

void AHyperBilliardsTable::OnRack()
{
	if (Sim.Phase() == EHyperPhase::Rolling)
	{
		return;   // a rerack mid-stroke would throw away a stroke he is watching
	}
	bCharging = false;
	bCanUndo = false;
	bUsedUndo = false;
	Sim.StartGame();
	if (bPractice) Sim.StartLesson(PracticeLesson);
	Sim.TakeEvents();
	BallRotations.Init(FQuat::Identity, FHyperBilliardsSim::BallCount);
	Heading = 0.0;   // straight up the table at the apex
	Tilt = 0.0;
	Spin = 0.0;
	SliceTarget = 0.0;
	NextBallPick = 0;
	LastClearedStrokes = 0;
	LastShotFeedback.Empty();
	SelectedBall = -1;
	if (bPractice)
	{
		SelectedBall = 1;
		bDepthAssist = true;
		AimAtBall(1);
		OnSliceToCue();
		SelectedPower = PracticeLesson == 0 ? 0.25f : PracticeLesson == 1 ? 0.35f : 0.7f;
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[HyperBilliards] racked."));
}

void AHyperBilliardsTable::OnToggleView()
{
	bBehindBallView = !bBehindBallView;
}

void AHyperBilliardsTable::OnLeave()
{
	if (bCharging) OnCancel(); else StopPlaying();
}

float AHyperBilliardsTable::CurrentPower() const
{
	return SelectedPower;
}

void AHyperBilliardsTable::UpdateAim(float DeltaSeconds)
{
	if (Sim.Phase() != EHyperPhase::Aiming)
	{
		PendingMouseAim = 0.0f;
		return;
	}
	const float Fine = bFineHeld ? FineAimFraction : 1.0f;
	// Right turns the aim to the right, as at the living-room table: a larger yaw.
	float Degrees = ((bAimRightHeld ? 1.0f : 0.0f) - (bAimLeftHeld ? 1.0f : 0.0f)) * AimDegreesPerSecond * DeltaSeconds;
	Degrees += PendingMouseAim * MouseAimDegreesPerUnit;
	PendingMouseAim = 0.0f;
	Heading = FMath::UnwindRadians(Heading + FMath::DegreesToRadians(static_cast<double>(Degrees * Fine)));

	const float TiltDegrees = ((bTiltUpHeld ? 1.0f : 0.0f) - (bTiltDownHeld ? 1.0f : 0.0f)) * TiltDegreesPerSecond * DeltaSeconds * Fine;
	const double MaxTilt = FMath::DegreesToRadians(HyperTableDetail::MaxTiltDegrees);
	Tilt = FMath::Clamp(Tilt + FMath::DegreesToRadians(static_cast<double>(TiltDegrees)), -MaxTilt, MaxTilt);
}

void AHyperBilliardsTable::UpdateSlice(float DeltaSeconds)
{
	const double HD = Sim.Layout().HalfDepth;
	const double Held = (bSliceUpHeld ? 1.0 : 0.0) - (bSliceDownHeld ? 1.0 : 0.0);
	if (Held != 0.0)
	{
		SliceTarget = FMath::Clamp(SliceTarget + Held * SliceSpeed * DeltaSeconds, -HD, HD);
	}
	SliceW = FMath::FInterpTo(SliceW, SliceTarget, static_cast<double>(DeltaSeconds), 8.0);
}

void AHyperBilliardsTable::AimAtBall(int32 Ball)
{
	const FHyperVec3 D = Sim.BallPosition(Ball) - Sim.BallPosition(FHyperBilliardsSim::CueBall);
	Heading = FMath::Atan2(D.Y, D.X);
	if (bDepthAssist) Tilt = FMath::Atan2(D.W, FMath::Sqrt(D.X * D.X + D.Y * D.Y));
}

void AHyperBilliardsTable::SyncBalls(float DeltaSeconds)
{
	using namespace HyperTableDetail;
	const double R = Sim.Layout().BallRadius;
	for (int32 I = 0; I < FHyperBilliardsSim::BallCount && I < BallMeshes.Num(); ++I)
	{
		UStaticMeshComponent* BallMesh = BallMeshes[I];
		if (!BallMesh)
		{
			continue;
		}
		const FHyperVec3 P = Sim.BallPosition(I);
		const double Slice = Sim.IsOnTable(I) ? FHyperBilliardsSim::SliceRadius(R, P.W, SliceW) : 0.0;
		BallMesh->SetVisibility(Sim.IsOnTable(I));
		if (!Sim.IsOnTable(I)) continue;
		if (Slice <= HiddenSlice)
		{
			// A flattened numbered marker is a projection, not a hittable cross-section.
			BallMesh->SetRelativeLocationAndRotation(SimToCloth(P, 0.6), FQuat::Identity);
			BallMesh->SetRelativeScale3D(FVector(R / MeshBallRadius, R / MeshBallRadius, 0.08));
			continue;
		}

		// Turned by the part of its roll he can see, in the cloth's X-Y: what it does in W
		// cannot be shown in a slice.
		const FHyperVec3 U = Sim.BallRoll(I);
		const FVector Axis(-U.Y, U.X, 0.0);
		const double Rate = Axis.Size() / R;
		if (Rate > 1e-6 && DeltaSeconds > 0.0f && BallRotations.IsValidIndex(I))
		{
			BallRotations[I] = (FQuat(Axis.GetSafeNormal(), Rate * DeltaSeconds) * BallRotations[I]).GetNormalized();
		}
		// The slice's centre sits where the 4D ball's centre does: a full radius above the cloth.
		BallMesh->SetRelativeLocationAndRotation(SimToCloth(P, R), BallRotations.IsValidIndex(I) ? BallRotations[I] : FQuat::Identity);
		BallMesh->SetRelativeScale3D(FVector(Slice / MeshBallRadius));
	}
}

// A pocket's mouth on the cloth is the slice through its capture: as wide as the capture where
// his slice meets the pocket's W, closing as it moves away. Each spot has a pocket at -W and +W.
void AHyperBilliardsTable::SyncPockets()
{
	using namespace HyperTableDetail;
	const FHyperTableLayout& L = Sim.Layout();
	for (int32 I = 0; I < PocketMouths.Num(); ++I)
	{
		UStaticMeshComponent* Mouth = PocketMouths[I];
		if (!Mouth)
		{
			continue;
		}
		const double Open = FMath::Max(FHyperBilliardsSim::SliceRadius(L.PocketCapture, -L.HalfDepth, SliceW),
			FHyperBilliardsSim::SliceRadius(L.PocketCapture, L.HalfDepth, SliceW));
		Mouth->SetVisibility(Open > HiddenSlice);
		Mouth->SetRelativeScale3D(FVector(2.0 * Open / ShapeSize, 2.0 * Open / ShapeSize, 0.3 / ShapeSize));
	}
}

void AHyperBilliardsTable::SyncCueAndGuides()
{
	using namespace HyperTableDetail;
	const double R = Sim.Layout().BallRadius;
	const bool bAiming = bPlaying && Sim.Phase() == EHyperPhase::Aiming && Sim.IsOnTable(FHyperBilliardsSim::CueBall);
	const FHyperVec3 CuePos = Sim.BallPosition(FHyperBilliardsSim::CueBall);
	const double CueSlice = FHyperBilliardsSim::SliceRadius(R, CuePos.W, SliceW);
	// Keep the projected cue and guides available while inspecting another depth.
	const bool bCueSeen = bAiming;

	for (UStaticMeshComponent* Part : { Cue.Get(), AimLine.Get(), ObjectLine.Get() })
	{
		if (Part)
		{
			Part->SetVisibility(bCueSeen);
		}
	}
	if (!bCueSeen)
	{
		return;
	}

	const FVector Dir(FMath::Cos(Heading), FMath::Sin(Heading), 0.0);
	if (Cue)
	{
		const double TipHeight = R + Spin * FHyperBilliardsSim::MaxTipOffset * CueSlice;
		const FVector Tip = SimToCloth(CuePos, TipHeight) - Dir * (CueSlice + CueTipGap + CuePullBack * CurrentPower());
		const double Elev = FMath::DegreesToRadians(CueElevationDegrees);
		const FVector Back = -Dir * FMath::Cos(Elev) + FVector::UpVector * FMath::Sin(Elev);
		Cue->SetRelativeLocationAndRotation(Tip + Back * CueHalfLength,
			FRotator(CueElevationDegrees, FMath::RadiansToDegrees(Heading) + 180.0, 0.0));
	}

	auto LayBar = [](UStaticMeshComponent* Bar, FVector A, FVector B)
	{
		const FVector D = B - A;
		const double Len = D.Size2D();
		if (!Bar || Len < 0.5)
		{
			if (Bar)
			{
				Bar->SetVisibility(false);
			}
			return;
		}
		Bar->SetRelativeLocationAndRotation((A + B) * 0.5, FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.0));
		Bar->SetRelativeScale3D(FVector(Len / ShapeSize, GuideThickness / ShapeSize, 0.1 / ShapeSize));
	};

	// The aim, as far as it goes in X-Y: to where it first touches a ball, or 60 cm.
	const FHyperAimPrediction Aim = Sim.PredictShot(Heading, Tilt);
	const FVector From = SimToCloth(CuePos, GuideHeight);
	const FVector To = Aim.bHitsBall ? SimToCloth(Aim.CueAtContact, GuideHeight) : From + Dir * 60.0 * FMath::Cos(Tilt);
	LayBar(AimLine, From + Dir * CueSlice, To);
	if (Aim.bHitsBall)
	{
		const FVector ObjectPos = SimToCloth(Sim.BallPosition(Aim.Ball), GuideHeight);
		const FVector ObjDir(Aim.ObjectDirection.X, Aim.ObjectDirection.Y, 0.0);
		LayBar(ObjectLine, ObjectPos, ObjectPos + ObjDir * ObjectLineLength);
	}
	else if (ObjectLine)
	{
		ObjectLine->SetVisibility(false);
	}
}

void AHyperBilliardsTable::SyncBoard()
{
	using namespace HyperTableDetail;
	if (BoardSlice)
	{
		BoardSlice->SetRelativeLocation(FVector(0.0, -1.0, SliceW));
	}
	for (int32 I = 0; I < FHyperBilliardsSim::BallCount && I < BoardDots.Num(); ++I)
	{
		if (UStaticMeshComponent* Dot = BoardDots[I])
		{
			Dot->SetVisibility(Sim.IsOnTable(I));
			Dot->SetRelativeLocation(SimToBoard(Sim.BallPosition(I)));
		}
	}

	// While he aims, the shot as the board sees it: from the cue ball to where it touches, in X-W.
	const bool bAiming = bPlaying && Sim.Phase() == EHyperPhase::Aiming && Sim.IsOnTable(FHyperBilliardsSim::CueBall);
	if (!BoardAim)
	{
		return;
	}
	BoardAim->SetVisibility(bAiming);
	if (!bAiming)
	{
		return;
	}
	const FHyperAimPrediction Aim = Sim.PredictShot(Heading, Tilt);
	const FHyperVec3 CuePos = Sim.BallPosition(FHyperBilliardsSim::CueBall);
	const FHyperVec3 Dir(FMath::Cos(Tilt) * FMath::Cos(Heading), FMath::Cos(Tilt) * FMath::Sin(Heading), FMath::Sin(Tilt));
	const FHyperVec3 End = Aim.bHitsBall ? Aim.CueAtContact : CuePos + Dir * 60.0;
	const FVector A = SimToBoard(CuePos);
	const FVector B = SimToBoard(End);
	const FVector D = B - A;
	const double Len = FVector(D.X, 0.0, D.Z).Size();
	if (Len < 0.5)
	{
		BoardAim->SetVisibility(false);   // a shot straight across Y: nothing to show in X-W
		return;
	}
	// The board's X-Z plane: a bar along (dX, dW), turned by pitch about the board's Y.
	BoardAim->SetRelativeLocationAndRotation((A + B) * 0.5, FRotator(FMath::RadiansToDegrees(FMath::Atan2(D.Z, D.X)), 0.0, 0.0));
	BoardAim->SetRelativeScale3D(FVector(Len / ShapeSize, 0.6 / ShapeSize, 0.6 / ShapeSize));
}

void AHyperBilliardsTable::SyncCamera(float DeltaSeconds, bool bSnap)
{
	using namespace HyperTableDetail;
	if (!TableCamera)
	{
		return;
	}
	FVector TargetLoc;
	FRotator TargetRot;
	const bool bBehind = bBehindBallView && Sim.IsOnTable(FHyperBilliardsSim::CueBall) && Sim.Phase() != EHyperPhase::Idle;
	if (bBehind)
	{
		const FVector Dir(FMath::Cos(Heading), FMath::Sin(Heading), 0.0);
		const FVector Ball = SimToCloth(Sim.BallPosition(FHyperBilliardsSim::CueBall), ClothHeight);
		TargetLoc = Ball - Dir * 80.0 + FVector(0.0, 0.0, 42.0);
		TargetRot = (Ball + Dir * 60.0 - TargetLoc).Rotation();
	}
	else
	{
		// From the -Y side, high, aimed between the cloth and the W-board: both on screen, the
		// board's dots lined up above their balls - and the board low enough to clear the HUD's
		// score panel at the top of the screen, which covered it at first.
		const double Pitch = FMath::DegreesToRadians(static_cast<double>(OverheadPitchDegrees));
		const FVector Center(0.0, 45.0, ClothHeight + 55.0);
		TargetLoc = Center + FVector(0.0, -FMath::Cos(Pitch), FMath::Sin(Pitch)) * OverheadDistance;
		TargetRot = FRotator(-OverheadPitchDegrees, 90.0, 0.0);
	}
	if (bSnap || DeltaSeconds <= 0.0f)
	{
		TableCamera->SetRelativeLocationAndRotation(TargetLoc, TargetRot);
		return;
	}
	TableCamera->SetRelativeLocationAndRotation(FMath::VInterpTo(TableCamera->GetRelativeLocation(), TargetLoc, DeltaSeconds, 5.0f),
		FMath::RInterpTo(TableCamera->GetRelativeRotation(), TargetRot, DeltaSeconds, 5.0f));
}

void AHyperBilliardsTable::HandleEvents()
{
	// The living room's way: only the loudest few contacts of each kind in a frame.
	TArray<float, TInlineAllocator<32>> Clicks;
	TArray<float, TInlineAllocator<16>> Thuds;
	for (const FHyperEvent& Event : Sim.TakeEvents())
	{
		switch (Event.Type)
		{
		case EHyperEvent::ShotTaken:
			TableSounds::Play(this, ETableSound::CueStrike, FMath::Clamp(0.25f + static_cast<float>(Event.Value) / 900.0f, 0.25f, 1.0f));
			break;
		case EHyperEvent::BallHitBall:
			if (Event.Index == 0 || Event.Other == 0) bShotHitObject = true;
			if (Event.Index == 0 || Event.Other == 0) LastShotFeedback = FString::Printf(TEXT("Contact: ball %d. Use the gold line to steer it toward a pocket."), Event.Index == 0 ? Event.Other : Event.Index);
			Clicks.Add(FMath::Clamp(static_cast<float>(Event.Value) / 350.0f, 0.04f, 1.0f));
			break;
		case EHyperEvent::BallHitCushion:
			Thuds.Add(FMath::Clamp(static_cast<float>(Event.Value) / 300.0f, 0.05f, 0.9f));
			break;
		case EHyperEvent::BallPocketed:
			LastShotFeedback = FString::Printf(TEXT("Ball %d potted!"), Event.Index);
			TableSounds::Play(this, ETableSound::Chime100, 0.35f);
			TableSounds::Play(this, ETableSound::PocketDrop, 0.8f);
			break;
		case EHyperEvent::CueBallScratched:
			TableSounds::Play(this, ETableSound::PocketDrop, 0.8f, Event.Type == EHyperEvent::CueBallScratched ? 0.9f : FMath::FRandRange(0.95f, 1.05f));
			break;
		case EHyperEvent::CueBallRespotted:
		{
			// Somewhere new: point it at something.
			NextBallPick = 0;
			OnNextBall();
			break;
		}
		case EHyperEvent::ShotEnded:
			if (!bShotHitObject && bExpectedContact) LastShotFeedback = TEXT("Stopped before contact. Try more power, or pick a closer ball.");
			// Every stroke starts with the cue ball seen.
			OnSliceToCue();
			NextBallPick = 0;
			break;
		case EHyperEvent::TableCleared:
			LastClearedStrokes = Event.Index;
			if (!bPractice && !bUsedUndo && (BestStrokes == 0 || Event.Index < BestStrokes))
			{
				BestStrokes = Event.Index;
			}
			TableSounds::Play(this, ETableSound::Chime100, 0.6f);
			UE_LOG(LogSibeliusGame, Display, TEXT("[HyperBilliards] table cleared in %d strokes (best %d)."), Event.Index, BestStrokes);
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

void AHyperBilliardsTable::RefreshPanel()
{
	ASibeliusHUD* Hud = bPlaying ? GetHud() : nullptr;
	if (!Hud) return;
	const bool bAiming = Sim.Phase() == EHyperPhase::Aiming;
	FString Text = FString::Printf(TEXT("4D BILLIARDS   %d LEFT   %d STROKES"), Sim.BallsLeft(), Sim.Strokes());
	FString Note = bAiming ? AimFeedback() : LastShotFeedback;
	if (bAiming && !LastShotFeedback.IsEmpty()) Note += TEXT("\nLAST SHOT: ") + LastShotFeedback;
	FString Controls = TEXT("Click ball / X target   A D / hold RMB aim   [ ] / L3 R3 power   SPACE / A shoot\nP / View lessons   LT / Backspace retry   V / Y camera   H help   ESC / B leave");
	if (Sim.Phase() == EHyperPhase::Idle)
	{
		Text = TEXT("4D BILLIARDS - choose a ball, see its depth, make the shot");
		Note = TEXT("Start with three short lessons, or rack a full table.");
		Controls = TEXT("P / View lessons   SPACE / A full table   ESC / B leave");
	}
	else if (Sim.Phase() == EHyperPhase::Cleared)
	{
		Text = bPractice ? FString::Printf(TEXT("LESSON %d COMPLETE - %d STROKES"), PracticeLesson + 1, Sim.Strokes()) : FString::Printf(TEXT("TABLE CLEARED - %d STROKES"), Sim.Strokes());
		Note = bPractice ? (PracticeLesson == 2 ? TEXT("All three lessons complete! Try the full table, or replay the lessons.") : TEXT("Lesson complete. Strokes include scratch penalties.")) : TEXT("Well played!");
		Controls = bPractice ? TEXT("SPACE / A next lesson   P / View full table   Backspace / LT retry   ESC / B leave") : TEXT("SPACE / A play again   P / View lessons   ESC / B leave");
	}
	else if (Sim.Phase() == EHyperPhase::Rolling)
		Controls = TEXT("Backspace / LT retry   V / Y camera   ESC / B leave");
	if (bAiming)
	{
		Text += FString::Printf(TEXT("\nPOWER %d%%   DEPTH %+d deg   ASSIST %s"), FMath::RoundToInt(SelectedPower * 100), FMath::RoundToInt(FMath::RadiansToDegrees(Tilt)), bDepthAssist ? TEXT("ON") : TEXT("OFF"));
		if (bPractice)
		{
			const TCHAR* Lessons[] = { TEXT("1/3: A simple pot - follow the gold object-ball line."), TEXT("2/3: Across depths - the target is in another slice. G / RB toggles assistance."), TEXT("3/3: Bank shot - send the ball to the end cushion, then the side pocket.") };
			Text = FString(Lessons[PracticeLesson]) + TEXT("\n") + Text;
		}
		if (bShowHelp) Controls = TEXT("UP DOWN / D-pad tilt depth   G / RB depth assist   T / X select target\nWheel / Q C inspect slices   F cue slice   W S spin   SHIFT / LB fine aim\nR / Menu restart   RMB cancels armed shot   H close help");
	}
	TArray<FTableWorldMarker> Markers;
	if (bAiming && SelectedBall > 0 && Sim.IsOnTable(SelectedBall))
	{
		const auto P = Sim.BallPosition(SelectedBall);
		const double DW = P.W - Sim.BallPosition(0).W;
		const FString Label = FString::Printf(TEXT("BALL %d   DEPTH %+.0f"), SelectedBall, DW);
		Markers.Add({ Cloth->GetComponentTransform().TransformPosition(SimToCloth(P, Sim.Layout().BallRadius)), Label });
		Markers.Add({ Board->GetComponentTransform().TransformPosition(SimToBoard(P)), FString::Printf(TEXT("%d IN DEPTH VIEW"), SelectedBall) });
	}
	Hud->ShowTableMarkers(Markers);
	Hud->ShowTablePanel(Text, Controls, 0.25f, Note, bAiming ? SelectedPower : -1.0f);
}

ASibeliusHUD* AHyperBilliardsTable::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}

void AHyperBilliardsTable::OnCancel() { bCharging = false; }
void AHyperBilliardsTable::OnPowerUp() { if (Sim.Phase() == EHyperPhase::Aiming) SelectedPower = FMath::Min(1.0f, SelectedPower + (bFineHeld ? 0.01f : 0.05f)); }
void AHyperBilliardsTable::OnPowerDown() { if (Sim.Phase() == EHyperPhase::Aiming) SelectedPower = FMath::Max(0.05f, SelectedPower - (bFineHeld ? 0.01f : 0.05f)); }
void AHyperBilliardsTable::OnUndo()
{
	if (!bCanUndo) return;
	SelectedBall = SavedSelectedBall;
	LastShotFeedback = TEXT("Shot restored - try another angle or power.");
	SelectedPower = SavedPower;
	Heading = SavedHeading; Tilt = SavedTilt; Spin = SavedSpin;
	Sim = BeforeShot;
	Sim.TakeEvents();
	bCanUndo = false;
	bUsedUndo = true;
	bCharging = false;
	OnSliceToCue();
}
void AHyperBilliardsTable::OnPractice() {
	bPractice = !bPractice;
	PracticeLesson = 0;
	bCharging = false;
	Sim = FHyperBilliardsSim();
	OnRack();
}

void AHyperBilliardsTable::OnToggleAssist()
{
	bDepthAssist = !bDepthAssist;
	if (bDepthAssist && SelectedBall > 0 && Sim.IsOnTable(SelectedBall) && Sim.Phase() == EHyperPhase::Aiming) AimAtBall(SelectedBall);
}
void AHyperBilliardsTable::OnToggleHelp() { bShowHelp = !bShowHelp; }

void AHyperBilliardsTable::OnPickTarget()
{
	APlayerController* PC = PlayingPC.Get();
	if (!PC || Sim.Phase() != EHyperPhase::Aiming) return;
	float MX, MY;
	if (!PC->GetMousePosition(MX, MY)) return;
	int32 Pick = -1;
	float Best = 32.0f * 32.0f;
	for (int32 I = 1; I < FHyperBilliardsSim::BallCount; ++I)
	{
		if (!Sim.IsOnTable(I)) continue;
		for (const FVector& World : { BallMeshes[I]->GetComponentLocation(), BoardDots[I]->GetComponentLocation() })
		{
			FVector2D Screen;
			if (!PC->ProjectWorldLocationToScreen(World, Screen, true)) continue;
			const float Distance = FVector2D::DistSquared(Screen, FVector2D(MX, MY));
			if (Distance < Best) { Best = Distance; Pick = I; }
		}
	}
	if (Pick > 0) { SelectedBall = Pick; AimAtBall(Pick); OnSliceToCue(); }
}

FString AHyperBilliardsTable::AimFeedback() const
{
	const auto Hit = Sim.PredictShot(Heading, Tilt);
	if (Hit.bHitsBall)
		return FString::Printf(TEXT("FIRST CONTACT: ball %d%s"), Hit.Ball, Hit.Ball == SelectedBall ? TEXT(" - target aligned") : TEXT(" - select it to inspect depth"));
	if (SelectedBall <= 0 || !Sim.IsOnTable(SelectedBall)) return TEXT("Click a ball or press T / X to select a target. Flat discs mark other depths.");
	const auto D = Sim.BallPosition(SelectedBall) - Sim.BallPosition(0);
	const double Along = D.X * FMath::Cos(Heading) + D.Y * FMath::Sin(Heading);
	const double Across = FMath::Abs(D.X * FMath::Sin(Heading) - D.Y * FMath::Cos(Heading));
	if (Along > 0 && Across < 2.0 * Sim.Layout().BallRadius)
	{
		const double Gap = Along * FMath::Tan(Tilt) - D.W;
		return FString::Printf(TEXT("MISSES BALL %d: depth %s by %.0f cm. Adjust UP / DOWN, or enable G / RB assist."), SelectedBall, Gap > 0 ? TEXT("too far") : TEXT("too near"), FMath::Abs(Gap));
	}
	return TEXT("Aim left or right toward the selected ball. Depth alone will not make contact.");
}
