// OrbitGolfCourse.cpp — see the header for what this actor shows and how he plays it.

#include "OrbitGolfCourse.h"

#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"
#include "TableSounds.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
namespace OrbitCourseDetail
{
	constexpr int32 MaxMoons = 3;          // the most any hole of the course has
	constexpr double ShapeSize = 100.0;    // the engine's basic shapes are 100 cm across
	constexpr double PoleHeight = 250.0;
	constexpr int32 TrailLength = 60;
	constexpr float TrailEvery = 0.04f;
	constexpr int32 GuidePoints = 12;
	constexpr double GuideSeconds = 1.0;
	constexpr double MaxPitchDegrees = 85.0;
	constexpr double MinPitchDegrees = -60.0;   // down, for a putt off a moon's underside

	struct FPaint
	{
		FLinearColor Color;
		float Glow = 0.0f;
	};
	const FLinearColor MoonColors[MaxMoons] = {
		FLinearColor(0.55f, 0.42f, 0.90f), FLinearColor(0.20f, 0.72f, 0.66f), FLinearColor(0.95f, 0.62f, 0.25f) };
	const FPaint RingPaint{ FLinearColor(0.70f, 0.66f, 0.85f), 1.0f };
	const FPaint MatPaint{ FLinearColor(0.05f, 0.30f, 0.08f), 0.0f };
	// The bag of clubs, in its own space: Z up from the platform, +X toward the tee. Clubs stand
	// grip down, heads up, as in any bag; the rim glows so it is found in the dusk.
	enum class EShape : uint8 { Cube, Cylinder, Sphere };
	struct FBagPart
	{
		const TCHAR* Name;
		EShape Shape;
		FVector Size;
		FVector Where;
		FPaint Paint;
		bool bGlow;
	};
	const FLinearColor Leather(0.30f, 0.03f, 0.05f);
	const FLinearColor Steel(0.72f, 0.72f, 0.75f);
	const FBagPart BagPartsSpec[] = {
		{ TEXT("BagBody"),    EShape::Cylinder, FVector(28.0, 28.0, 88.0), FVector(0.0, 0.0, 46.0),   { Leather, 0.0f }, false },
		{ TEXT("BagBase"),    EShape::Cylinder, FVector(31.0, 31.0, 10.0), FVector(0.0, 0.0, 6.0),    { FLinearColor(0.03f, 0.03f, 0.04f), 0.0f }, false },
		{ TEXT("BagBand"),    EShape::Cylinder, FVector(29.0, 29.0, 3.0),  FVector(0.0, 0.0, 62.0),   { FLinearColor(0.85f, 0.85f, 0.85f), 0.0f }, false },
		{ TEXT("BagRim"),     EShape::Cylinder, FVector(31.0, 31.0, 7.0),  FVector(0.0, 0.0, 90.0),   { FLinearColor(0.45f, 0.35f, 0.90f), 1.5f }, true },
		{ TEXT("BagPocket"),  EShape::Cube,     FVector(10.0, 16.0, 34.0), FVector(14.0, 0.0, 38.0),  { FLinearColor(0.20f, 0.02f, 0.03f), 0.0f }, false },
		{ TEXT("BagStrap"),   EShape::Cube,     FVector(3.0, 6.0, 56.0),   FVector(-15.5, 0.0, 52.0), { FLinearColor(0.02f, 0.02f, 0.02f), 0.0f }, false },
		{ TEXT("DriverShaft"), EShape::Cylinder, FVector(1.6, 1.6, 62.0),  FVector(5.0, -6.0, 101.0), { Steel, 0.0f }, false },
		{ TEXT("DriverCover"), EShape::Sphere,   FVector(13.0, 11.0, 15.0), FVector(5.0, -6.0, 137.0), { FLinearColor(0.80f, 0.10f, 0.10f), 0.0f }, false },
		{ TEXT("WoodShaft"),  EShape::Cylinder, FVector(1.6, 1.6, 56.0),   FVector(-5.0, -7.0, 98.0), { Steel, 0.0f }, false },
		{ TEXT("WoodCover"),  EShape::Sphere,   FVector(12.0, 10.0, 13.0), FVector(-5.0, -7.0, 131.0), { FLinearColor(0.15f, 0.60f, 0.55f), 0.0f }, false },
		{ TEXT("IronShaftA"), EShape::Cylinder, FVector(1.4, 1.4, 48.0),   FVector(6.0, 5.0, 94.0),   { Steel, 0.0f }, false },
		{ TEXT("IronHeadA"),  EShape::Cube,     FVector(3.0, 10.0, 6.0),   FVector(6.0, 8.5, 117.0),  { Steel, 0.0f }, false },
		{ TEXT("IronShaftB"), EShape::Cylinder, FVector(1.4, 1.4, 52.0),   FVector(-2.0, 2.0, 96.0),  { Steel, 0.0f }, false },
		{ TEXT("IronHeadB"),  EShape::Cube,     FVector(3.0, 10.0, 6.0),   FVector(-2.0, 5.5, 121.0), { Steel, 0.0f }, false },
		{ TEXT("PutterShaft"), EShape::Cylinder, FVector(1.4, 1.4, 44.0),  FVector(-7.0, 6.0, 92.0),  { Steel, 0.0f }, false },
		{ TEXT("PutterHead"), EShape::Cube,     FVector(4.0, 12.0, 4.0),   FVector(-7.0, 10.0, 114.0), { FLinearColor(0.08f, 0.08f, 0.09f), 0.0f }, false },
	};
	const FPaint CupPaint{ FLinearColor(1.0f, 1.0f, 1.0f), 4.0f };
	const FPaint PolePaint{ FLinearColor(0.9f, 0.9f, 0.9f), 0.0f };
	const FPaint FlagPaint{ FLinearColor(1.0f, 0.15f, 0.12f), 3.0f };
	const FPaint BallPaint{ FLinearColor(1.0f, 1.0f, 0.95f), 2.0f };
	const FPaint TrailPaint{ FLinearColor(0.4f, 0.85f, 1.0f), 3.0f };
	const FPaint GuidePaint{ FLinearColor(1.0f, 0.75f, 0.25f), 4.0f };
}

AOrbitGolfCourse::AOrbitGolfCourse()
{
	using namespace OrbitCourseDetail;
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlainMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/ModularSciFiEnv_K/Materials/Base/M_LampEmiss_MAT.M_LampEmiss_MAT"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleMaterial(TEXT("/Game/StainedGlass3D/Materials/M_BlackMarbleFloor.M_BlackMarbleFloor"));
	UMaterialInterface* Plain = PlainMaterial.Succeeded() ? PlainMaterial.Object.Get() : nullptr;
	UMaterialInterface* Glow = GlowMaterial.Succeeded() ? GlowMaterial.Object.Get() : nullptr;
	UMaterialInterface* Marble = MarbleMaterial.Succeeded() ? MarbleMaterial.Object.Get() : Plain;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	auto MakeShape = [&](const TCHAR* Name, USceneComponent* Parent, ConstructorHelpers::FObjectFinder<UStaticMesh>& Shape,
		UMaterialInterface* Material, FVector Size, FVector Where) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Part->SetGenerateOverlapEvents(false);
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

	const FOrbitCourse& Course = Sim.Course();
	const double Rp = Course.PlatformRadius;
	Platform = MakeShape(TEXT("Platform"), SceneRoot, CylinderMesh, Marble, FVector(2.0 * Rp, 2.0 * Rp, 150.0), FVector(0.0, 0.0, -75.0));
	Platform->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	TeeMat = MakeShape(TEXT("TeeMat"), SceneRoot, CubeMesh, Plain, FVector(70.0, 70.0, 1.0), FVector(0.0, 0.0, 0.5));
	// The bag at the back of the platform, leaning back a little as a bag on its stand does.
	Bag = CreateDefaultSubobject<USceneComponent>(TEXT("Bag"));
	Bag->SetupAttachment(SceneRoot);
	Bag->SetMobility(EComponentMobility::Movable);
	Bag->SetRelativeLocationAndRotation(FVector(-Rp + 50.0, 0.0, 0.0), FRotator(10.0, 0.0, 0.0));
	for (const FBagPart& Spec : BagPartsSpec)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh>& Shape =
			Spec.Shape == EShape::Cube ? CubeMesh : Spec.Shape == EShape::Sphere ? SphereMesh : CylinderMesh;
		BagParts.Add(MakeShape(Spec.Name, Bag, Shape, Spec.bGlow ? Glow : Plain, Spec.Size, Spec.Where));
	}

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetupAttachment(Bag);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetBoxExtent(FVector(30.0, 30.0, 75.0));
	Body->SetRelativeLocation(FVector(0.0, 0.0, 75.0));
	Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->SetGenerateOverlapEvents(false);

	for (int32 I = 0; I < MaxMoons; ++I)
	{
		UStaticMeshComponent* Moon = MakeShape(*FString::Printf(TEXT("Moon%d"), I), SceneRoot, SphereMesh, Plain, FVector(100.0), FVector::ZeroVector);
		Moon->SetVisibility(false);
		Moons.Add(Moon);
		UStaticMeshComponent* Ring = MakeShape(*FString::Printf(TEXT("MoonRing%d"), I), Moon, CylinderMesh, Glow, FVector(1.0, 1.0, 1.0), FVector::ZeroVector);
		Ring->SetCastShadow(false);
		MoonRings.Add(Ring);
	}

	Cup = CreateDefaultSubobject<USceneComponent>(TEXT("Cup"));
	Cup->SetupAttachment(SceneRoot);
	Cup->SetMobility(EComponentMobility::Movable);
	CupRing = MakeShape(TEXT("CupRing"), Cup, CylinderMesh, Glow, FVector(2.0 * Course.CupRadius, 2.0 * Course.CupRadius, 1.0), FVector(0.0, 0.0, 0.5));
	FlagPole = MakeShape(TEXT("FlagPole"), Cup, CylinderMesh, Plain, FVector(3.0, 3.0, PoleHeight), FVector(0.0, 0.0, PoleHeight * 0.5));
	Flag = MakeShape(TEXT("Flag"), Cup, CubeMesh, Glow, FVector(60.0, 2.0, 40.0), FVector(31.5, 0.0, PoleHeight - 20.0));

	const double Rb = Course.BallRadius;
	Ball = MakeShape(TEXT("Ball"), SceneRoot, SphereMesh, Glow, FVector(2.0 * Rb), FVector(0.0, 0.0, Rb));

	auto MakeDots = [&](const TCHAR* Name) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* Dots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Dots->SetupAttachment(SceneRoot);
		Dots->SetMobility(EComponentMobility::Movable);
		Dots->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Dots->SetCastShadow(false);
		if (SphereMesh.Succeeded())
		{
			Dots->SetStaticMesh(SphereMesh.Object);
		}
		if (Glow)
		{
			Dots->SetMaterial(0, Glow);
		}
		return Dots;
	};
	Trail = MakeDots(TEXT("Trail"));
	Guide = MakeDots(TEXT("Guide"));

	CourseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CourseCamera"));
	CourseCamera->SetupAttachment(SceneRoot);
	CourseCamera->SetFieldOfView(80.0f);
	SyncCamera(0.0f, true);
}

void AOrbitGolfCourse::ApplyPaint()
{
	using namespace OrbitCourseDetail;
	auto Paint = [](UMeshComponent* Part, const FPaint& With)
	{
		UMaterialInstanceDynamic* MID = Part ? Part->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
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
	Paint(TeeMat, MatPaint);
	for (int32 I = 0; I < BagParts.Num() && I < static_cast<int32>(UE_ARRAY_COUNT(BagPartsSpec)); ++I)
	{
		Paint(BagParts[I], BagPartsSpec[I].Paint);
	}
	for (int32 I = 0; I < Moons.Num(); ++I)
	{
		Paint(Moons[I], FPaint{ MoonColors[I % MaxMoons], 0.0f });
		Paint(MoonRings[I], RingPaint);
	}
	Paint(CupRing, CupPaint);
	Paint(FlagPole, PolePaint);
	Paint(Flag, FlagPaint);
	Paint(Ball, BallPaint);
	Paint(Trail, TrailPaint);
	Paint(Guide, GuidePaint);
}

void AOrbitGolfCourse::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPaint();
	ShownHole = -1;
	ShowHole();
	SyncBall(0.0f);
}

void AOrbitGolfCourse::BeginPlay()
{
	Super::BeginPlay();
	Sim = FOrbitGolfSim();
	ApplyPaint();
	ShownHole = -1;
	ShowHole();
	AimAtCup();
	SyncBall(0.0f);
	SyncCamera(0.0f, true);
}

void AOrbitGolfCourse::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlaying();
	Super::EndPlay(EndPlayReason);
}

void AOrbitGolfCourse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (bPlaying)
	{
		UpdateAim(DeltaSeconds);
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
	SyncGuide();
	SyncCamera(DeltaSeconds, false);
	RefreshPanel();
}

void AOrbitGolfCourse::Interact_Implementation(AActor* Interactor)
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

FText AOrbitGolfCourse::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("OrbitGolf", "Prompt", "[E] play orbital golf");
}

void AOrbitGolfCourse::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	bCharging = false;
	bLeftHeld = bRightHeld = bUpHeld = bDownHeld = bFineHeld = false;
	PendingMouse = 0.0f;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	// His body steps aside, as at the pool tables: its own actions are not ours to consume.
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
		Pawn->SetActorHiddenInGame(true);
	}
	if (Sim.Phase() == EOrbitPhase::Idle || Sim.Phase() == EOrbitPhase::Finished)
	{
		Sim.StartRound();
		HandleEvents();
	}
	SyncCamera(0.0f, true);
	PC->SetViewTargetWithBlend(this, CameraBlendSeconds, VTBlend_Cubic);
	EnableInput(PC);
	BindKeys();
	UE_LOG(LogSibeliusGame, Display, TEXT("[OrbitGolf] on the tee (%s)."), *GetActorNameOrLabel());
}

void AOrbitGolfCourse::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	bCharging = false;
	bLeftHeld = bRightHeld = bUpHeld = bDownHeld = bFineHeld = false;
	if (APlayerController* PC = PlayingPC.Get())
	{
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
	UE_LOG(LogSibeliusGame, Display, TEXT("[OrbitGolf] left the course."));
}

void AOrbitGolfCourse::BindKeys()
{
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;
	using FHandler = void (AOrbitGolfCourse::*)();
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
		Bind(Key, &AOrbitGolfCourse::OnLeftPressed, &AOrbitGolfCourse::OnLeftReleased);
	}
	for (const FKey& Key : { EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right })
	{
		Bind(Key, &AOrbitGolfCourse::OnRightPressed, &AOrbitGolfCourse::OnRightReleased);
	}
	for (const FKey& Key : { EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up })
	{
		Bind(Key, &AOrbitGolfCourse::OnUpPressed, &AOrbitGolfCourse::OnUpReleased);
	}
	for (const FKey& Key : { EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down })
	{
		Bind(Key, &AOrbitGolfCourse::OnDownPressed, &AOrbitGolfCourse::OnDownReleased);
	}
	for (const FKey& Key : { EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl, EKeys::Gamepad_LeftShoulder })
	{
		Bind(Key, &AOrbitGolfCourse::OnFinePressed, &AOrbitGolfCourse::OnFineReleased);
	}
	for (const FKey& Key : { EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_RightTrigger })
	{
		Bind(Key, &AOrbitGolfCourse::OnSwingPressed, &AOrbitGolfCourse::OnSwingReleased);
	}
	for (const FKey& Key : { EKeys::R, EKeys::Gamepad_Special_Right })
	{
		Bind(Key, &AOrbitGolfCourse::OnRestart, nullptr);
	}
	for (const FKey& Key : { EKeys::V, EKeys::Tab, EKeys::Gamepad_FaceButton_Top })
	{
		Bind(Key, &AOrbitGolfCourse::OnToggleView, nullptr);
	}
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		Bind(Key, &AOrbitGolfCourse::OnLeave, nullptr);
	}
	InputComponent->BindAxisKey(EKeys::MouseX, this, &AOrbitGolfCourse::OnMouseX);
}

void AOrbitGolfCourse::OnLeftPressed()    { bLeftHeld = true; }
void AOrbitGolfCourse::OnLeftReleased()   { bLeftHeld = false; }
void AOrbitGolfCourse::OnRightPressed()   { bRightHeld = true; }
void AOrbitGolfCourse::OnRightReleased()  { bRightHeld = false; }
void AOrbitGolfCourse::OnUpPressed()      { bUpHeld = true; }
void AOrbitGolfCourse::OnUpReleased()     { bUpHeld = false; }
void AOrbitGolfCourse::OnDownPressed()    { bDownHeld = true; }
void AOrbitGolfCourse::OnDownReleased()   { bDownHeld = false; }
void AOrbitGolfCourse::OnFinePressed()    { bFineHeld = true; }
void AOrbitGolfCourse::OnFineReleased()   { bFineHeld = false; }
void AOrbitGolfCourse::OnMouseX(float Value) { PendingMouse += Value; }

void AOrbitGolfCourse::OnSwingPressed()
{
	switch (Sim.Phase())
	{
	case EOrbitPhase::Holed:
		Sim.NextHole();   // the same key goes on; its release must not also swing, so no charge
		HandleEvents();
		break;
	case EOrbitPhase::Idle:
	case EOrbitPhase::Finished:
		Sim.StartRound();
		HandleEvents();
		break;
	case EOrbitPhase::Aiming:
		bCharging = true;
		ChargeSeconds = 0.0f;
		break;
	default:
		break;
	}
}

void AOrbitGolfCourse::OnSwingReleased()
{
	if (!bCharging)
	{
		return;
	}
	const float Power = CurrentPower();   // before ending the charge: 0 when not charging
	bCharging = false;
	Sim.Shoot(Yaw, Pitch, Power);
}

void AOrbitGolfCourse::OnRestart()
{
	if (Sim.Phase() == EOrbitPhase::Flying)
	{
		return;
	}
	bCharging = false;
	Sim.StartRound();
	HandleEvents();
}

void AOrbitGolfCourse::OnToggleView() { bOverview = !bOverview; }
void AOrbitGolfCourse::OnLeave()      { StopPlaying(); }

float AOrbitGolfCourse::CurrentPower() const
{
	if (!bCharging)
	{
		return 0.0f;
	}
	const float Phase = FMath::Fmod(ChargeSeconds / PowerChargeSeconds, 2.0f);
	return Phase <= 1.0f ? Phase : 2.0f - Phase;
}

void AOrbitGolfCourse::UpdateAim(float DeltaSeconds)
{
	using namespace OrbitCourseDetail;
	if (Sim.Phase() != EOrbitPhase::Aiming)
	{
		PendingMouse = 0.0f;
		return;
	}
	const float Fine = bFineHeld ? FineAimFraction : 1.0f;
	float Turn = ((bRightHeld ? 1.0f : 0.0f) - (bLeftHeld ? 1.0f : 0.0f)) * AimDegreesPerSecond * DeltaSeconds;
	Turn += PendingMouse * MouseAimDegreesPerUnit;
	PendingMouse = 0.0f;
	Yaw = FMath::UnwindRadians(Yaw + FMath::DegreesToRadians(static_cast<double>(Turn * Fine)));
	const float Raise = ((bUpHeld ? 1.0f : 0.0f) - (bDownHeld ? 1.0f : 0.0f)) * AimDegreesPerSecond * DeltaSeconds * Fine;
	Pitch = FMath::Clamp(Pitch + FMath::DegreesToRadians(static_cast<double>(Raise)),
		FMath::DegreesToRadians(MinPitchDegrees), FMath::DegreesToRadians(MaxPitchDegrees));
}

// From wherever the ball lies, turned toward the cup and lofted: a start, not an answer.
void AOrbitGolfCourse::AimAtCup()
{
	const FSpaceVec From = Sim.BallPosition();
	const FSpaceVec To = Sim.CurrentHole().CupPoint();
	Yaw = FMath::Atan2(To.Y - From.Y, To.X - From.X);
	Pitch = FMath::DegreesToRadians(35.0);
}

void AOrbitGolfCourse::ShowHole()
{
	using namespace OrbitCourseDetail;
	const int32 Hole = Sim.HoleNumber();
	if (Hole == ShownHole)
	{
		return;
	}
	ShownHole = Hole;
	const FOrbitHole& H = Sim.CurrentHole();
	for (int32 I = 0; I < Moons.Num(); ++I)
	{
		const bool bUsed = I < static_cast<int32>(H.Moons.size());
		Moons[I]->SetVisibility(bUsed, /*bPropagateToChildren=*/true);
		if (!bUsed)
		{
			continue;
		}
		const FOrbitMoon& M = H.Moons[I];
		Moons[I]->SetRelativeLocation(ToLocal(M.Center));
		Moons[I]->SetRelativeScale3D(FVector(2.0 * M.Radius / ShapeSize));
		// A thin ring round its equator, tilted like the planets in Elise's sky. Rolled, not
		// pitched: from the tee, looking down +X, a pitched disc is a flying saucer. In the moon's
		// scaled space, so 1.0 is the moon's diameter.
		MoonRings[I]->SetRelativeScale3D(FVector(1.6, 1.6, 0.004));
		MoonRings[I]->SetRelativeRotation(FRotator(8.0, 30.0 * I, 24.0 - 12.0 * I));
	}
	const FSpaceVec CupPoint = H.CupPoint();
	const FVector Up = ToLocal(H.CupDirection).GetSafeNormal();
	Cup->SetRelativeLocationAndRotation(ToLocal(CupPoint), FRotationMatrix::MakeFromZ(Up).Rotator());
}

void AOrbitGolfCourse::SyncBall(float DeltaSeconds)
{
	using namespace OrbitCourseDetail;
	const bool bShowBall = Sim.Phase() != EOrbitPhase::Idle;
	Ball->SetVisibility(bShowBall || !bPlaying);
	Ball->SetRelativeLocation(ToLocal(Sim.BallPosition()));

	if (Sim.Phase() == EOrbitPhase::Flying)
	{
		TrailClock += DeltaSeconds;
		if (TrailClock >= TrailEvery)
		{
			TrailClock = 0.0f;
			TrailPoints.Add(ToLocal(Sim.BallPosition()));
			if (TrailPoints.Num() > TrailLength)
			{
				TrailPoints.RemoveAt(0);
			}
		}
	}
	Trail->ClearInstances();
	for (int32 I = 0; I < TrailPoints.Num(); ++I)
	{
		const double Size = 0.04 + 0.06 * I / FMath::Max(1, TrailPoints.Num());   // grows toward the ball
		Trail->AddInstance(FTransform(FRotator::ZeroRotator, TrailPoints[I], FVector(Size)));
	}
}

void AOrbitGolfCourse::SyncGuide()
{
	using namespace OrbitCourseDetail;
	Guide->ClearInstances();
	if (!bPlaying || Sim.Phase() != EOrbitPhase::Aiming)
	{
		return;
	}
	// While he charges, the guide is the shot he is about to play; before, a middling one.
	const double Power = bCharging ? CurrentPower() : 0.5;
	const std::vector<FSpaceVec> Path = Sim.PredictPath(Yaw, Pitch, Power, GuideSeconds, GuidePoints);
	for (size_t I = 1; I < Path.size(); ++I)
	{
		Guide->AddInstance(FTransform(FRotator::ZeroRotator, ToLocal(Path[I]), FVector(0.07)));
	}
}

void AOrbitGolfCourse::SyncCamera(float DeltaSeconds, bool bSnap)
{
	if (!CourseCamera)
	{
		return;
	}
	const FVector B = ToLocal(Sim.BallPosition());
	FVector Loc;
	FVector LookAt;
	const EOrbitPhase Phase = Sim.Phase();
	if (bOverview || Phase == EOrbitPhase::Idle || Phase == EOrbitPhase::Finished)
	{
		// The whole course from behind and above the tee.
		Loc = FVector(-1500.0, 0.0, 1400.0);
		LookAt = FVector(2400.0, 0.0, 800.0);
	}
	else if (Phase == EOrbitPhase::Flying)
	{
		// Chasing it, from behind its flight and a little above.
		const FVector V = ToLocal(Sim.BallVelocity());
		const FVector Back = V.SizeSquared() > 1.0 ? -V.GetSafeNormal() : FVector(-1.0, 0.0, 0.0);
		Loc = B + Back * 650.0 + FVector(0.0, 0.0, 180.0);
		LookAt = B;
	}
	else if (Phase == EOrbitPhase::Holed)
	{
		const FVector Out = ToLocal(Sim.CurrentHole().CupDirection);
		Loc = B + Out * 450.0 + FVector(0.0, 0.0, 120.0) - FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0) * 300.0;
		LookAt = B;
	}
	else
	{
		// Aiming: behind the ball along the shot, looking the way it will go.
		const FVector Flat(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
		const FVector Dir = ToLocal(FOrbitGolfSim::ShotDirection(Yaw, Pitch));
		Loc = B - Flat * 450.0 + FVector(0.0, 0.0, 160.0);
		LookAt = B + Dir * 900.0;
	}
	const FRotator Rot = (LookAt - Loc).Rotation();
	if (bSnap || DeltaSeconds <= 0.0f)
	{
		CourseCamera->SetRelativeLocationAndRotation(Loc, Rot);
		return;
	}
	CourseCamera->SetRelativeLocationAndRotation(FMath::VInterpTo(CourseCamera->GetRelativeLocation(), Loc, DeltaSeconds, 4.0f),
		FMath::RInterpTo(CourseCamera->GetRelativeRotation(), Rot, DeltaSeconds, 4.0f));
}

void AOrbitGolfCourse::HandleEvents()
{
	for (const FOrbitEvent& Event : Sim.TakeEvents())
	{
		switch (Event.Type)
		{
		case EOrbitEvent::HoleStarted:
			ShowHole();
			AimAtCup();
			TrailPoints.Reset();
			break;
		case EOrbitEvent::ShotTaken:
			TrailPoints.Reset();
			TableSounds::Play(this, ETableSound::CueStrike, FMath::Clamp(0.3f + static_cast<float>(Event.Value) / 3600.0f, 0.3f, 1.0f), 0.8f);
			break;
		case EOrbitEvent::Bounced:
			TableSounds::Play(this, ETableSound::CushionThud, FMath::Clamp(static_cast<float>(Event.Value) / 1500.0f, 0.1f, 0.9f));
			break;
		case EOrbitEvent::CameToRest:
			AimAtCup();
			break;
		case EOrbitEvent::Holed:
			TableSounds::Play(this, ETableSound::PocketDrop, 0.9f);
			TableSounds::Play(this, ETableSound::Chime100, 0.6f);
			ASibeliusHUD::Toast(this, FString::Printf(TEXT("IN THE CUP  -  %d %s"), Event.Index, Event.Index == 1 ? TEXT("stroke") : TEXT("strokes")),
				4.0f, SibeliusToast::Good);
			break;
		case EOrbitEvent::OutOfBounds:
			ASibeliusHUD::Toast(this, TEXT("INTO THE GRASS  -  a penalty stroke"), 3.0f, SibeliusToast::Bad);
			break;
		case EOrbitEvent::LostInOrbit:
			ASibeliusHUD::Toast(this, TEXT("LOST IN ORBIT  -  a penalty stroke"), 3.0f, SibeliusToast::Bad);
			break;
		case EOrbitEvent::RoundFinished:
			if (BestRound == 0 || Event.Index < BestRound)
			{
				BestRound = Event.Index;
			}
			UE_LOG(LogSibeliusGame, Display, TEXT("[OrbitGolf] round of %d (par %d), best %d."), Event.Index, static_cast<int32>(Event.Value), BestRound);
			break;
		default:
			break;
		}
	}
}

void AOrbitGolfCourse::RefreshPanel()
{
	ASibeliusHUD* Hud = bPlaying ? GetHud() : nullptr;
	if (!Hud)
	{
		return;
	}
	const FOrbitHole& H = Sim.CurrentHole();
	const int32 ToPar = Sim.TotalStrokes() - Sim.TotalPar();
	const FString Round = Sim.Card().empty() ? FString(TEXT("")) : FString::Printf(TEXT("     ROUND %s"),
		ToPar == 0 ? TEXT("E") : *FString::Printf(TEXT("%+d"), ToPar));
	FString Text;
	FString Controls;
	switch (Sim.Phase())
	{
	case EOrbitPhase::Finished:
		Text = FString::Printf(TEXT("ROUND DONE  -  %d STROKES, PAR %d%s"), Sim.TotalStrokes(), Sim.TotalPar(),
			BestRound > 0 ? *FString::Printf(TEXT("     BEST %d"), BestRound) : TEXT(""));
		Controls = TEXT("SPACE  play again      ESC  walk away");
		break;
	case EOrbitPhase::Holed:
		Text = FString::Printf(TEXT("HOLE %d  %s  -  IN %d, PAR %d%s"), Sim.HoleNumber() + 1, UTF8_TO_TCHAR(H.Name.c_str()),
			Sim.HoleStrokes(), H.Par, *Round);
		Controls = TEXT("SPACE  next hole      ESC  walk away");
		break;
	default:
	{
		FString Power;
		if (bCharging)
		{
			const int32 Bars = FMath::RoundToInt(CurrentPower() * 20.0f);
			Power = TEXT("     POWER ") + FString::ChrN(Bars, TEXT('|')) + FString::ChrN(20 - Bars, TEXT('.'));
		}
		Text = FString::Printf(TEXT("HOLE %d  %s  PAR %d     STROKES %d%s\nLOFT %d deg%s"), Sim.HoleNumber() + 1,
			UTF8_TO_TCHAR(H.Name.c_str()), H.Par, Sim.HoleStrokes(), *Round, FMath::RoundToInt(FMath::RadiansToDegrees(Pitch)), *Power);
		Controls = TEXT("AIM  A D or mouse  (SHIFT fine)     LOFT  W S     SWING  hold SPACE, let go\n"
			"V  the whole course      R  start the round again      ESC  walk away");
		break;
	}
	}
	Hud->ShowTablePanel(Text, Controls);
}

ASibeliusHUD* AOrbitGolfCourse::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}
