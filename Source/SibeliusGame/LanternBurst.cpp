// LanternBurst.cpp — see the header for what this actor shows and how he plays it.

#include "LanternBurst.h"

#include "LanternBurstSaveGame.h"
#include "SibeliusGame.h"   // LogSibeliusGame
#include "SibeliusHUD.h"
#include "SibeliusSaveIO.h"
#include "TableSounds.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace LanternDetail
{
	using namespace LanternBurst;

	constexpr double ShapeSize = 100.0;                                     // the basic shapes are 100 cm across
	constexpr double OrbSize = 40.0;                                        // an orb's diameter, cm
	constexpr double Radius = Columns * OrbSize / (2.0 * UE_DOUBLE_PI);     // the lantern, to the orbs' centres
	constexpr double RowStep = OrbSize * RowHeight;
	constexpr double EmberZ = 115.0;                                        // the line, above the ground
	constexpr double Row0Top = EmberZ + (DeathRow - 0.5) * RowStep;         // row 0's centres, crown at the top
	constexpr double LauncherZ = Row0Top - (DeathRow + 1) * RowStep;        // a row under the embers, always
	constexpr double HookZ = Row0Top + 320.0;
	constexpr int32 EmberCount = 40;
	constexpr int32 ChainCount = 4;
	constexpr int32 SpikeCount = 10;
	constexpr double GuideSpacing = 0.42;                                   // diameters between guide dots
	constexpr float Gravity = 980.0f;
	const TCHAR* const SaveSlot = TEXT("LanternBurst");

	// Gem colours, in LINEAR (the Go table's lesson: these are not sRGB), bright enough to glow.
	const FLinearColor Colors[MaxColors + 1] = {
		FLinearColor(1.00f, 0.06f, 0.10f),   // ruby
		FLinearColor(0.08f, 0.32f, 1.00f),   // sapphire
		FLinearColor(1.00f, 0.58f, 0.04f),   // amber
		FLinearColor(0.08f, 0.95f, 0.28f),   // emerald
		FLinearColor(0.60f, 0.14f, 1.00f),   // amethyst
		FLinearColor(0.70f, 0.95f, 1.00f),   // pearl
		FLinearColor(1.00f, 0.95f, 0.80f),   // the spark: white-hot
	};
	const FLinearColor EmberColor(1.0f, 0.22f, 0.03f);
	const FLinearColor WarmLight(1.0f, 0.72f, 0.42f);

	struct FPaint
	{
		FLinearColor Color;
		float Glow = 0.0f;   // 0 = the plain lit shape material
	};
	const FPaint Gold{ FLinearColor(0.30f, 0.17f, 0.035f), 0.0f };
	const FPaint DarkMetal{ FLinearColor(0.06f, 0.05f, 0.05f), 0.0f };
	const FPaint BandGlow{ FLinearColor(1.0f, 0.70f, 0.30f), 3.0f };
	const FPaint CoreGlow{ FLinearColor(1.0f, 0.55f, 0.20f), 2.5f };   // a thin flame, not a drum (the first was a pale solid drum behind the orbs)
	constexpr double FlameWidth = 14.0;
	const FPaint HookGlow{ FLinearColor(1.0f, 0.80f, 0.45f), 4.0f };
	const FPaint CradleGlow{ FLinearColor(0.55f, 0.85f, 1.0f), 3.0f };

	FString Grouped(int64 Value)
	{
		return FText::AsNumber(Value).ToString();
	}

	float Ease(float T)
	{
		// Out with a little overshoot: an orb hung on the lantern settles, it does not just appear.
		T = FMath::Clamp(T, 0.0f, 1.0f);
		const float C = 1.7f;
		return 1.0f + (C + 1.0f) * FMath::Pow(T - 1.0f, 3.0f) + C * FMath::Pow(T - 1.0f, 2.0f);
	}

	// One instanced sphere set: transforms and four custom floats each (R, G, B, glow).
	struct FInstances
	{
		TArray<FTransform> Xf;
		TArray<float> Data;
		void Add(const FVector& At, double Diameter, const FLinearColor& Color, float Glow)
		{
			Xf.Add(FTransform(FRotator::ZeroRotator, At, FVector(Diameter / ShapeSize)));
			Data.Append({ Color.R, Color.G, Color.B, Glow });
		}
		void Apply(UInstancedStaticMeshComponent* Ism) const
		{
			if (!Ism)
			{
				return;
			}
			if (Ism->GetInstanceCount() != Xf.Num())
			{
				Ism->ClearInstances();
				if (Xf.Num() > 0)
				{
					Ism->AddInstances(Xf, false, false);
				}
			}
			else if (Xf.Num() > 0)
			{
				Ism->BatchUpdateInstancesTransforms(0, Xf, false, false, true);
			}
			for (int32 I = 0; I < Xf.Num(); ++I)
			{
				Ism->SetCustomData(I, TArrayView<const float>(Data.GetData() + I * 4, 4), false);
			}
			Ism->MarkRenderStateDirty();
		}
	};
}

ALanternBurst::ALanternBurst()
{
	using namespace LanternDetail;
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlainMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/ModularSciFiEnv_K/Materials/Base/M_LampEmiss_MAT.M_LampEmiss_MAT"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OrbFinder(TEXT("/Game/LanternBurst/M_LanternOrb.M_LanternOrb"));
	UMaterialInterface* Plain = PlainMaterial.Succeeded() ? PlainMaterial.Object.Get() : nullptr;
	UMaterialInterface* Lamp = GlowMaterial.Succeeded() ? GlowMaterial.Object.Get() : Plain;
	OrbMaterial = OrbFinder.Succeeded() ? OrbFinder.Object.Get() : nullptr;

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

	// The crown: a gold lid a little wider than the lantern, a glowing band, a ring of spikes.
	Crown = CreateDefaultSubobject<USceneComponent>(TEXT("Crown"));
	Crown->SetupAttachment(SceneRoot);
	Crown->SetMobility(EComponentMobility::Movable);
	const double LidWidth = 2.0 * Radius + OrbSize + 16.0;
	CrownParts.Add(MakeShape(TEXT("Lid"), Crown, CylinderMesh, Plain, FVector(LidWidth, LidWidth, 14.0), FVector(0.0, 0.0, 7.0)));
	CrownParts.Add(MakeShape(TEXT("Band"), Crown, CylinderMesh, Lamp, FVector(LidWidth + 6.0, LidWidth + 6.0, 5.0), FVector(0.0, 0.0, 3.0)));
	CrownParts.Add(MakeShape(TEXT("Dome"), Crown, SphereMesh, Plain, FVector(LidWidth * 0.55, LidWidth * 0.55, 60.0), FVector(0.0, 0.0, 14.0)));
	for (int32 I = 0; I < SpikeCount; ++I)
	{
		const double A = 2.0 * UE_DOUBLE_PI * I / SpikeCount;
		const FVector Where(FMath::Cos(A) * LidWidth * 0.46, FMath::Sin(A) * LidWidth * 0.46, 30.0);
		CrownParts.Add(MakeShape(*FString::Printf(TEXT("Spike%d"), I), Crown, ConeMesh, Plain, FVector(14.0, 14.0, 34.0), Where));
	}
	for (int32 I = 0; I < ChainCount; ++I)
	{
		UStaticMeshComponent* Chain = MakeShape(*FString::Printf(TEXT("Chain%d"), I), SceneRoot, CylinderMesh, Plain, FVector(3.0, 3.0, 100.0), FVector::ZeroVector);
		Chain->SetCastShadow(false);
		Chains.Add(Chain);
	}
	Hook = MakeShape(TEXT("Hook"), SceneRoot, SphereMesh, Lamp, FVector(22.0), FVector(0.0, 0.0, HookZ));
	Core = MakeShape(TEXT("Core"), SceneRoot, CylinderMesh, Lamp, FVector(FlameWidth, FlameWidth, 100.0), FVector::ZeroVector);
	Core->SetCastShadow(false);

	// The launcher at the lantern's foot, facing his camera; the next orb's stand to its right.
	const FVector Foot(-Radius, 0.0, 0.0);
	LauncherParts.Add(MakeShape(TEXT("CradlePost"), SceneRoot, CylinderMesh, Plain, FVector(24.0, 24.0, LauncherZ - 22.0), Foot + FVector(0.0, 0.0, (LauncherZ - 22.0) * 0.5)));
	LauncherParts.Add(MakeShape(TEXT("CradleRing"), SceneRoot, CylinderMesh, Lamp, FVector(34.0, 34.0, 4.0), Foot + FVector(0.0, 0.0, LauncherZ - 22.0)));
	LauncherParts.Add(MakeShape(TEXT("NextPost"), SceneRoot, CylinderMesh, Plain, FVector(16.0, 16.0, LauncherZ - 30.0), Foot + FVector(0.0, 52.0, (LauncherZ - 30.0) * 0.5)));
	LauncherParts.Add(MakeShape(TEXT("NextRing"), SceneRoot, CylinderMesh, Lamp, FVector(24.0, 24.0, 3.0), Foot + FVector(0.0, 52.0, LauncherZ - 30.0)));

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetupAttachment(SceneRoot);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetBoxExtent(FVector(30.0, 55.0, 65.0));
	Body->SetRelativeLocation(Foot + FVector(0.0, 24.0, 65.0));
	Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->SetGenerateOverlapEvents(false);

	Column = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Column"));
	Column->SetupAttachment(SceneRoot);
	Column->SetMobility(EComponentMobility::Movable);
	Column->InitCapsuleSize(Radius + OrbSize * 0.5, 260.0f);
	Column->SetRelativeLocation(FVector(0.0, 0.0, 300.0));
	Column->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Column->SetCollisionResponseToAllChannels(ECR_Ignore);
	Column->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Column->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);   // his E: see the header
	Column->SetGenerateOverlapEvents(false);

	auto MakeSet = [&](const TCHAR* Name, bool bShadows) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* Set = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Set->SetupAttachment(SceneRoot);
		Set->SetMobility(EComponentMobility::Movable);
		Set->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Set->SetGenerateOverlapEvents(false);
		Set->SetCastShadow(bShadows);
		Set->NumCustomDataFloats = 4;
		if (SphereMesh.Succeeded())
		{
			Set->SetStaticMesh(SphereMesh.Object);
		}
		if (OrbMaterial)
		{
			Set->SetMaterial(0, OrbMaterial);
		}
		return Set;
	};
	Orbs = MakeSet(TEXT("Orbs"), true);
	Live = MakeSet(TEXT("Live"), false);
	Guide = MakeSet(TEXT("Guide"), false);
	Effects = MakeSet(TEXT("Effects"), false);
	Embers = MakeSet(TEXT("Embers"), false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(LightCandelas);
	Glow->SetAttenuationRadius(1100.0f);
	Glow->SetLightColor(WarmLight);
	Glow->SetCastShadows(false);

	LanternCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("LanternCamera"));
	LanternCamera->SetupAttachment(SceneRoot);
	LanternCamera->SetFieldOfView(CameraFieldOfView);
	SyncCamera(0.0f, true);
}

// ---------------------------------------------------------------------------------------------
// Setting up

void ALanternBurst::ApplyPaint()
{
	using namespace LanternDetail;
	auto Paint = [](UStaticMeshComponent* Part, const FPaint& With)
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
	for (int32 I = 0; I < CrownParts.Num(); ++I)
	{
		Paint(CrownParts[I], I == 1 ? BandGlow : Gold);
	}
	for (UStaticMeshComponent* Chain : Chains)
	{
		Paint(Chain, DarkMetal);
	}
	Paint(Hook, HookGlow);
	Paint(Core, CoreGlow);
	for (int32 I = 0; I < LauncherParts.Num(); ++I)
	{
		Paint(LauncherParts[I], (I % 2) ? CradleGlow : DarkMetal);
	}
	// A level saved before the orb material existed holds none: find it by name.
	if (!OrbMaterial)
	{
		OrbMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LanternBurst/M_LanternOrb.M_LanternOrb"));
		if (!OrbMaterial)
		{
			UE_LOG(LogSibeliusGame, Warning, TEXT("[Lantern] M_LanternOrb is missing - run Tools/Scripts/make_lantern_assets.py."));
		}
	}
	for (UInstancedStaticMeshComponent* Set : { Orbs.Get(), Live.Get(), Guide.Get(), Effects.Get(), Embers.Get() })
	{
		if (Set && OrbMaterial && Set->GetMaterial(0) != OrbMaterial)
		{
			Set->SetMaterial(0, OrbMaterial);
		}
	}
}

void ALanternBurst::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPaint();
	// In the editor the lantern hangs a sample stage, so it can be judged where it stands.
	if (Sim.Phase() == ELanternPhase::Ready)
	{
		Sim.NewGame(2026);
		Sim.TakeEvents();
	}
	SyncFrame();
	Draw();
	SyncCamera(0.0f, true);
}

void ALanternBurst::BeginPlay()
{
	Super::BeginPlay();
	ApplyPaint();
	LoadBest();
	Fx.Reset();
	NewGame();
	SyncFrame();
	SyncCamera(0.0f, true);
}

void ALanternBurst::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlaying();
	Super::EndPlay(EndPlayReason);
}

void ALanternBurst::NewGame()
{
	const uint32 Seed = static_cast<uint32>(FDateTime::Now().GetTicks() ^ (FDateTime::Now().GetTicks() >> 32)) | 1u;
	Sim.NewGame(Seed);
	bLanternFallen = false;
	bFlying = false;
	bNewBestThisGame = false;
	BestAtGameStart = BestScore;
	VisualLowered = 0.0f;
	PreviewShots = -1;
	HandleEvents();
}

// ---------------------------------------------------------------------------------------------
// Playing

void ALanternBurst::Interact_Implementation(AActor* Interactor)
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

FText ALanternBurst::GetInteractionPrompt_Implementation() const
{
	return bPlaying ? FText::GetEmpty() : NSLOCTEXT("LanternBurst", "Prompt", "[E] play Lantern Burst");
}

void ALanternBurst::StartPlaying(APlayerController* PC)
{
	PlayingPC = PC;
	bPlaying = true;
	bLeftHeld = bRightHeld = false;
	LastMouse = FVector2D(-1.0, -1.0);
	TurnTarget = Turn;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	// His body steps aside, as at the pool tables: its own actions are not ours to consume.
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
		Pawn->SetActorHiddenInGame(true);
	}
	// A pointer to aim with. Game-and-UI so a click in the view is still a key this actor hears.
	PC->SetShowMouseCursor(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	if (Sim.Phase() == ELanternPhase::GameOver)
	{
		NewGame();
	}
	SyncCamera(0.0f, true);
	PC->SetViewTargetWithBlend(this, 0.8f, VTBlend_Cubic);
	EnableInput(PC);
	BindKeys();
	ASibeliusHUD::Toast(this, TEXT("LANTERN BURST  -  three of a colour burst, and whatever they held falls"), 5.0f, SibeliusToast::Info);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Lantern] playing (%s), best %lld."), *GetActorNameOrLabel(), BestScore);
}

void ALanternBurst::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	bLeftHeld = bRightHeld = false;
	if (APlayerController* PC = PlayingPC.Get())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->EnableInput(PC);
			Pawn->SetActorHiddenInGame(false);
			PC->SetViewTargetWithBlend(Pawn, 0.5f, VTBlend_Cubic);
		}
		DisableInput(PC);
		if (ASibeliusHUD* Hud = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			Hud->ReleaseCinematic();
		}
	}
	PlayingPC.Reset();
	SaveBest();
	UE_LOG(LogSibeliusGame, Display, TEXT("[Lantern] walked away at %lld, stage %d."), Sim.Score(), Sim.Stage());
}

void ALanternBurst::BindKeys()
{
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	bKeysBound = true;
	using FHandler = void (ALanternBurst::*)();
	auto Bind = [this](const FKey& Key, FHandler Pressed, FHandler Released)
	{
		InputComponent->BindKey(Key, IE_Pressed, this, Pressed);
		if (Released)
		{
			InputComponent->BindKey(Key, IE_Released, this, Released);
		}
	};
	for (const FKey& Key : { EKeys::LeftMouseButton, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom })
	{
		Bind(Key, &ALanternBurst::OnFire, nullptr);
	}
	for (const FKey& Key : { EKeys::RightMouseButton, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Left })
	{
		Bind(Key, &ALanternBurst::OnSwap, nullptr);
	}
	for (const FKey& Key : { EKeys::A, EKeys::Left, EKeys::Gamepad_DPad_Left })
	{
		Bind(Key, &ALanternBurst::OnLeftPressed, &ALanternBurst::OnLeftReleased);
	}
	for (const FKey& Key : { EKeys::D, EKeys::Right, EKeys::Gamepad_DPad_Right })
	{
		Bind(Key, &ALanternBurst::OnRightPressed, &ALanternBurst::OnRightReleased);
	}
	Bind(EKeys::MouseScrollUp, &ALanternBurst::OnWheelUp, nullptr);
	Bind(EKeys::MouseScrollDown, &ALanternBurst::OnWheelDown, nullptr);
	Bind(EKeys::R, &ALanternBurst::OnRestart, nullptr);
	Bind(EKeys::Gamepad_Special_Right, &ALanternBurst::OnRestart, nullptr);
	for (const FKey& Key : { EKeys::Escape, EKeys::E, EKeys::Gamepad_FaceButton_Right })
	{
		Bind(Key, &ALanternBurst::OnLeave, nullptr);
	}
}

void ALanternBurst::OnLeftPressed()   { bLeftHeld = true; }
void ALanternBurst::OnLeftReleased()  { bLeftHeld = false; }
void ALanternBurst::OnRightPressed()  { bRightHeld = true; }
void ALanternBurst::OnRightReleased() { bRightHeld = false; }
void ALanternBurst::OnWheelUp()       { if (!bFlying) TurnTarget += 2.0 * UE_DOUBLE_PI / LanternBurst::Columns; }
void ALanternBurst::OnWheelDown()     { if (!bFlying) TurnTarget -= 2.0 * UE_DOUBLE_PI / LanternBurst::Columns; }
void ALanternBurst::OnLeave()         { StopPlaying(); }

void ALanternBurst::OnFire()
{
	switch (Sim.Phase())
	{
	case ELanternPhase::Playing:
		if (!bFlying)
		{
			Launch();
		}
		break;
	case ELanternPhase::StageClear:
		Sim.NextStage();
		HandleEvents();
		break;
	case ELanternPhase::GameOver:
	case ELanternPhase::Ready:
		NewGame();
		break;
	}
}

void ALanternBurst::OnSwap()
{
	if (Sim.Phase() == ELanternPhase::Playing && !bFlying && Sim.Current() != Sim.Next())
	{
		Sim.Swap();
		TableSounds::Play(this, ETableSound::LanternStick, 0.35f, 0.75f);
	}
}

// R twice: one stray key must not throw away a good game.
void ALanternBurst::OnRestart()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Sim.Phase() == ELanternPhase::GameOver || Now < RestartArmedUntil)
	{
		RestartArmedUntil = -1.0;
		SaveBest();
		NewGame();
		return;
	}
	RestartArmedUntil = Now + 2.0;
	ASibeliusHUD::Toast(this, TEXT("Press R again to start over"), 2.0f, SibeliusToast::Info);
}

void ALanternBurst::Launch()
{
	RefreshPreview();
	if (!bPreviewValid || !PreviewTrace.bLands || PreviewTrace.Path.size() < 2)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Lantern] launch refused: preview %d, lands %d, path %d."),
			bPreviewValid ? 1 : 0, PreviewTrace.bLands ? 1 : 0, static_cast<int32>(PreviewTrace.Path.size()));
		return;
	}
	FlightPath.Reset();
	FlightLength.Reset();
	double Total = 0.0;
	for (size_t I = 0; I < PreviewTrace.Path.size(); ++I)
	{
		const FVector2D P(PreviewTrace.Path[I].X, PreviewTrace.Path[I].Y);
		if (I > 0)
		{
			Total += FVector2D::Distance(P, FlightPath.Last());
		}
		FlightPath.Add(P);
		FlightLength.Add(Total);
	}
	FlightDistance = 0.0;
	FlightLaunch = FrontColumn();
	FlightAngle = AimAngle;
	FlightColor = Sim.Current();
	bFlying = true;
	TurnTarget = Turn;   // the lantern holds still while the orb climbs it
	TableSounds::Play(this, ETableSound::LanternShoot, 0.55f, FMath::FRandRange(0.95f, 1.05f));
}

void ALanternBurst::Land()
{
	bFlying = false;
	if (!Sim.Shoot(FlightLaunch, FlightAngle))
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Lantern] the shot from column %.2f at %.1f degrees did not land."),
			FlightLaunch, FMath::RadiansToDegrees(FlightAngle));
	}
	HandleEvents();
}

void ALanternBurst::TickFlight(float DeltaSeconds)
{
	if (!bFlying)
	{
		return;
	}
	FlightDistance += ShotSpeed * DeltaSeconds;
	if (FlightLength.Num() == 0 || FlightDistance >= FlightLength.Last())
	{
		Land();
	}
}

void ALanternBurst::HandleEvents()
{
	using namespace LanternDetail;
	const std::vector<FLanternEvent> Events = Sim.TakeEvents();
	bool bBurst = false;
	for (const FLanternEvent& E : Events)
	{
		bBurst |= E.Type == ELanternEvent::Burst || E.Type == ELanternEvent::Sparked;
	}
	int32 Rings = 0;
	TArray<FString> Said;
	for (const FLanternEvent& E : Events)
	{
		switch (E.Type)
		{
		case ELanternEvent::StageStarted:
			AppearClock = 0.0f;
			bLanternFallen = false;
			if (bPlaying)
			{
				ASibeliusHUD::Toast(this, FString::Printf(TEXT("STAGE %d"), E.Index), 2.5f, SibeliusToast::Info);
			}
			break;
		case ELanternEvent::Placed:
			if (!bBurst)
			{
				TableSounds::Play(this, ETableSound::LanternStick, 0.6f, FMath::FRandRange(0.92f, 1.08f));
			}
			break;
		case ELanternEvent::Burst:
		case ELanternEvent::Sparked:
		{
			const bool bSpark = E.Type == ELanternEvent::Sparked;
			for (const FLanternOrb& Orb : E.Orbs)
			{
				FFx Bursting;
				Bursting.Kind = FFx::EKind::Bursting;
				Bursting.Pos = CellPosition(Orb.Row, Orb.Col);
				Bursting.Color = ColorOf(bSpark ? Spark : Orb.Color);
				Bursting.Delay = Orb.Ring * 0.045f;
				Bursting.Size = OrbSize;
				Fx.Add(Bursting);
				Rings = FMath::Max(Rings, Orb.Ring);
			}
			const float Pitch = 1.0f + 0.07f * (E.Index - 1);
			TableSounds::Play(this, bSpark ? ETableSound::LanternSpark : ETableSound::LanternPop, 0.8f, Pitch);
			TableSounds::Play(this, ETableSound::Chime100, 0.25f, Pitch);
			LightFlash = 1.0f;
			FlashColor = ColorOf(bSpark ? Spark : E.Orbs[0].Color);
			Said.Add(FString::Printf(TEXT("%s %d  +%s%s"), bSpark ? TEXT("SPARK BURNS") : TEXT("BURST"), static_cast<int32>(E.Orbs.size()),
				*Grouped(E.Value), E.Index > 1 ? *FString::Printf(TEXT("  (x%d)"), E.Index) : TEXT("")));
			break;
		}
		case ELanternEvent::Fell:
		{
			const int32 N = static_cast<int32>(E.Orbs.size());
			for (const FLanternOrb& Orb : E.Orbs)
			{
				FFx Falling;
				Falling.Kind = FFx::EKind::Falling;
				Falling.Pos = CellPosition(Orb.Row, Orb.Col);
				const FVector Out = FVector(Falling.Pos.X, Falling.Pos.Y, 0.0).GetSafeNormal();
				const FVector Side(-Out.Y, Out.X, 0.0);
				Falling.Vel = Out * FMath::FRandRange(70.0f, 170.0f) + Side * FMath::FRandRange(-50.0f, 50.0f) + FVector(0.0, 0.0, FMath::FRandRange(20.0f, 140.0f));
				Falling.Color = ColorOf(Orb.Color);
				Falling.Delay = Rings * 0.045f + 0.12f + FMath::FRandRange(0.0f, 0.08f);
				Falling.Life = FMath::FRandRange(1.5f, 2.1f);
				Falling.Size = OrbSize;
				Fx.Add(Falling);
			}
			TableSounds::Play(this, ETableSound::LanternFall, FMath::Clamp(0.3f + N * 0.04f, 0.3f, 1.0f));
			Shake = FMath::Max(Shake, FMath::Min(0.5f, N * 0.025f));
			Said.Add(FString::Printf(TEXT("DOWN COME %d  +%s"), N, *Grouped(E.Value)));
			if (N >= 6 && bPlaying)
			{
				ASibeliusHUD::Toast(this, FString::Printf(TEXT("DOWN COME %d!   +%s"), N, *Grouped(E.Value)), 3.0f, SibeliusToast::Good);
			}
			break;
		}
		case ELanternEvent::StreakUp:
			if (E.Index == MaxMultiplier && bPlaying)
			{
				ASibeliusHUD::Toast(this, TEXT("x5 STREAK  -  every burst now scores five times"), 3.0f, SibeliusToast::Good);
			}
			break;
		case ELanternEvent::SparkEarned:
			TableSounds::Play(this, ETableSound::KartPickup, 0.6f);
			if (bPlaying)
			{
				ASibeliusHUD::Toast(this, TEXT("A SPARK!  It burns every orb within two steps  -  SPACE to load it"), 4.0f, SibeliusToast::Good);
			}
			break;
		case ELanternEvent::Missed:
			Said.Add(FString::Printf(TEXT("NO BURST  -  the crown comes down after %d more"), E.Index));
			break;
		case ELanternEvent::Lowered:
			TableSounds::Play(this, ETableSound::LanternLower, 0.8f);
			Shake = FMath::Max(Shake, 0.35f);
			if (bPlaying)
			{
				ASibeliusHUD::Toast(this, TEXT("THE CROWN COMES DOWN A ROW"), 2.5f, SibeliusToast::Bad);
			}
			break;
		case ELanternEvent::StageCleared:
			TableSounds::Play(this, ETableSound::KartFinish, 0.7f);
			LightFlash = 1.0f;
			FlashColor = FLinearColor::White;
			if (bPlaying)
			{
				ASibeliusHUD::Toast(this, FString::Printf(TEXT("STAGE %d CLEAR!   +%s"), E.Index, *Grouped(E.Value)), 4.0f, SibeliusToast::Good);
			}
			break;
		case ELanternEvent::GameOver:
		{
			// Every orb left lets go, the lowest first.
			bLanternFallen = true;
			const int32 Lowest = FMath::Max(0, Sim.LowestRow());
			for (int32 Row = 0; Row < GridRows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					if (Sim.At(Row, Col) == Empty)
					{
						continue;
					}
					FFx Lost;
					Lost.Kind = FFx::EKind::Lost;
					Lost.Pos = CellPosition(Row, Col);
					const FVector Out = FVector(Lost.Pos.X, Lost.Pos.Y, 0.0).GetSafeNormal();
					Lost.Vel = Out * FMath::FRandRange(30.0f, 110.0f) + FVector(0.0, 0.0, FMath::FRandRange(0.0f, 60.0f));
					Lost.Color = ColorOf(Sim.At(Row, Col));
					Lost.Delay = 0.35f + (Lowest - Row) * 0.06f + FMath::FRandRange(0.0f, 0.05f);
					Lost.Life = FMath::FRandRange(1.8f, 2.6f);
					Lost.Size = OrbSize;
					Fx.Add(Lost);
				}
			}
			TableSounds::Play(this, ETableSound::LanternOver, 0.8f);
			Shake = 0.6f;
			if (bPlaying)
			{
				ASibeliusHUD::Toast(this, TEXT("THE LANTERN TOUCHED THE EMBERS"), 4.0f, SibeliusToast::Bad);
			}
			UE_LOG(LogSibeliusGame, Display, TEXT("[Lantern] game over at %lld, stage %d, %d shots."), E.Value, E.Index, Sim.Shots());
			break;
		}
		default:
			break;
		}
	}
	if (Said.Num() > 0)
	{
		LastNote = FString::Join(Said, TEXT("     "));
		LastNoteUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + 2.5;
	}

	// The best is his best ever, saved; the first time a game passes it, he hears of it once.
	if (Sim.Score() > BestScore)
	{
		BestScore = Sim.Score();
		if (!bNewBestThisGame && BestAtGameStart > 0 && bPlaying)
		{
			bNewBestThisGame = true;
			TableSounds::Play(this, ETableSound::KartLap, 0.6f);
			ASibeliusHUD::Toast(this, TEXT("NEW BEST SCORE!"), 3.0f, SibeliusToast::Good);
		}
	}
	BestStage = FMath::Max(BestStage, Sim.Stage());
	if (Sim.Phase() == ELanternPhase::GameOver || Sim.Phase() == ELanternPhase::StageClear)
	{
		SaveBest();
	}
	PreviewShots = -1;
}

// ---------------------------------------------------------------------------------------------
// Aiming

double ALanternBurst::FrontColumn() const
{
	const double X = (UE_DOUBLE_PI - Turn) * LanternBurst::Columns / (2.0 * UE_DOUBLE_PI);
	return FMath::Fmod(FMath::Fmod(X, static_cast<double>(LanternBurst::Columns)) + LanternBurst::Columns, static_cast<double>(LanternBurst::Columns));
}

double ALanternBurst::Row0Z() const
{
	return LanternDetail::Row0Top - VisualLowered * LanternDetail::RowStep;
}

FVector ALanternBurst::PointOnLantern(double X, double Y) const
{
	const double Theta = Turn + 2.0 * UE_DOUBLE_PI * X / LanternBurst::Columns;
	return FVector(LanternDetail::Radius * FMath::Cos(Theta), LanternDetail::Radius * FMath::Sin(Theta), Row0Z() - Y * LanternDetail::OrbSize);
}

FVector ALanternBurst::CellPosition(int32 Row, int32 Col) const
{
	const FLanternPoint P = FLanternBurstSim::CenterOf(Row, Col);
	return PointOnLantern(P.X, P.Y);
}

FVector ALanternBurst::LauncherPosition() const
{
	return FVector(-LanternDetail::Radius, 0.0, LanternDetail::LauncherZ);
}

FVector ALanternBurst::NextPosition() const
{
	return FVector(-LanternDetail::Radius, 52.0, LanternDetail::LauncherZ - 16.0);
}

bool ALanternBurst::FacesFront(double X) const
{
	const double Theta = Turn + 2.0 * UE_DOUBLE_PI * X / LanternBurst::Columns;
	return -FMath::Cos(Theta) > 0.2;
}

// The mouse points at a place on the lantern (or beside it, which reads as its edge); the shot is
// aimed from the launcher straight at that place on the unrolled lantern.
void ALanternBurst::UpdateAim()
{
	using namespace LanternDetail;
	const APlayerController* PC = PlayingPC.Get();
	float MouseX = 0.0f, MouseY = 0.0f;
	if (!PC || !PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	// A still mouse leaves the aim where it is. Turning the lantern needs no new aim: the place
	// under the pointer and the launcher turn away from it together.
	const FVector2D Mouse(MouseX, MouseY);
	if (Mouse.Equals(LastMouse, 0.5))
	{
		return;
	}
	LastMouse = Mouse;
	FVector Origin, Direction;
	if (!PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return;
	}
	const FTransform& Frame = GetActorTransform();
	const FVector O = Frame.InverseTransformPosition(Origin);
	const FVector D = Frame.InverseTransformVector(Direction).GetSafeNormal();
	const double A = D.X * D.X + D.Y * D.Y;
	if (A < 1e-9)
	{
		return;
	}
	const double B = 2.0 * (O.X * D.X + O.Y * D.Y);
	const double C = O.X * O.X + O.Y * O.Y - Radius * Radius;
	const double Disc = B * B - 4.0 * A * C;
	FVector P;
	if (Disc >= 0.0 && (-B - FMath::Sqrt(Disc)) / (2.0 * A) > 0.0)
	{
		P = O + D * ((-B - FMath::Sqrt(Disc)) / (2.0 * A));
	}
	else
	{
		const double T = FMath::Max(0.0, -(O.X * D.X + O.Y * D.Y) / A);
		P = O + D * T;
		const FVector Flat = FVector(P.X, P.Y, 0.0).GetSafeNormal();
		P = FVector(Flat.X * Radius, Flat.Y * Radius, P.Z);
	}
	const double Theta = FMath::Atan2(P.Y, P.X);
	const double X = (Theta - Turn) * Columns / (2.0 * UE_DOUBLE_PI);
	const double DX = FLanternBurstSim::WrapDelta(X - FrontColumn());
	const double Y = (Row0Z() - P.Z) / OrbSize;
	const double DY = FMath::Max(Sim.LauncherY() - Y, 0.25);
	AimAngle = FMath::Clamp(FMath::Atan2(DX, DY), -MaxAimAngle, MaxAimAngle);
}

void ALanternBurst::RefreshPreview()
{
	using namespace LanternDetail;
	const int32 Hand = Sim.Current() * 100 + Sim.Next();
	if (Sim.Phase() != ELanternPhase::Playing)
	{
		bPreviewValid = false;
		return;
	}
	if (Turn == PreviewTurn && AimAngle == PreviewAngle && Sim.Shots() == PreviewShots && Hand == PreviewHand)
	{
		return;
	}
	PreviewTurn = Turn;
	PreviewAngle = AimAngle;
	PreviewShots = Sim.Shots();
	PreviewHand = Hand;
	PreviewBurst.Reset();
	PreviewFall.Reset();
	PreviewResult = FLanternBurstSim::FPreview();
	PreviewTrace = Sim.Trace(FrontColumn(), AimAngle, true);
	bPreviewValid = PreviewTrace.bLands;
	if (!bPreviewValid)
	{
		return;
	}
	// Played on a copy, to light what it would burst and what it would bring down.
	FLanternBurstSim Trial = Sim;
	Trial.TakeEvents();
	Trial.PlaceAt(PreviewTrace.Cell.Row, PreviewTrace.Cell.Col);
	PreviewResult.bLegal = true;
	PreviewResult.Cell = PreviewTrace.Cell;
	for (const FLanternEvent& E : Trial.TakeEvents())
	{
		const bool bGone = E.Type == ELanternEvent::Burst || E.Type == ELanternEvent::Sparked;
		if (!bGone && E.Type != ELanternEvent::Fell)
		{
			continue;
		}
		for (const FLanternOrb& Orb : E.Orbs)
		{
			(bGone ? PreviewBurst : PreviewFall).Add(Orb.Row * Columns + Orb.Col);
		}
		(bGone ? PreviewResult.Burst : PreviewResult.Fall) += static_cast<int32>(E.Orbs.size());
	}
	PreviewResult.bOverLine = Trial.Phase() == ELanternPhase::GameOver;
	PreviewResult.bClears = Trial.Phase() == ELanternPhase::StageClear;
}

// ---------------------------------------------------------------------------------------------
// Every frame

void ALanternBurst::Tick(float DeltaSeconds)
{
	using namespace LanternDetail;
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	Clock += DeltaSeconds;
	AppearClock += DeltaSeconds;
	LightFlash = FMath::Max(0.0f, LightFlash - DeltaSeconds * 2.5f);
	Shake = FMath::Max(0.0f, Shake - DeltaSeconds * 1.6f);

	if (bPlaying)
	{
		if (!bFlying)
		{
			const double Step = FMath::DegreesToRadians(static_cast<double>(TurnDegreesPerSecond)) * DeltaSeconds;
			TurnTarget += ((bLeftHeld ? 1.0 : 0.0) - (bRightHeld ? 1.0 : 0.0)) * Step;
		}
		if (ASibeliusHUD* Hud = GetHud())
		{
			Hud->HoldCinematic(0.25f);
		}
	}
	else
	{
		TurnTarget += FMath::DegreesToRadians(static_cast<double>(IdleSpin)) * DeltaSeconds;
	}
	Turn += (TurnTarget - Turn) * FMath::Min(1.0, DeltaSeconds * 10.0);
	VisualLowered += (Sim.Lowered() - VisualLowered) * FMath::Min(1.0f, DeltaSeconds * 4.0f);

	if (bPlaying)
	{
		UpdateAim();
		RefreshPreview();
	}
	TickFlight(DeltaSeconds);
	TickEffects(DeltaSeconds);
	SyncFrame();
	Draw();
	SyncCamera(DeltaSeconds, false);
	RefreshPanel();
}

void ALanternBurst::SpawnBurst(const FVector& At, const FLinearColor& Color, int32 Count, float Speed, float Delay)
{
	for (int32 I = 0; I < Count; ++I)
	{
		FFx Bit;
		Bit.Kind = FFx::EKind::Spark;
		Bit.Pos = At;
		Bit.Vel = FMath::VRand() * Speed * FMath::FRandRange(0.5f, 1.0f) + FVector(0.0, 0.0, Speed * 0.25f);
		Bit.Color = Color;
		Bit.Delay = Delay;
		Bit.Life = FMath::FRandRange(0.45f, 0.85f);
		Bit.Size = FMath::FRandRange(5.0f, 9.0f);
		Fx.Add(Bit);
	}
}

void ALanternBurst::TickEffects(float DeltaSeconds)
{
	using namespace LanternDetail;
	TArray<FFx> Born;
	for (int32 I = Fx.Num() - 1; I >= 0; --I)
	{
		FFx& F = Fx[I];
		if (F.Delay > 0.0f)
		{
			F.Delay -= DeltaSeconds;
			continue;
		}
		F.Age += DeltaSeconds;
		bool bDone = false;
		switch (F.Kind)
		{
		case FFx::EKind::Spark:
			F.Vel.Z -= Gravity * 0.6f * DeltaSeconds;
			F.Vel *= FMath::Max(0.0f, 1.0f - 1.8f * DeltaSeconds);
			F.Pos += F.Vel * DeltaSeconds;
			bDone = F.Age >= F.Life;
			break;
		case FFx::EKind::Bursting:
			if (F.Age >= 0.09f)
			{
				for (int32 K = 0; K < 10; ++K)
				{
					FFx Bit;
					Bit.Pos = F.Pos;
					Bit.Vel = FMath::VRand() * 420.0f * FMath::FRandRange(0.4f, 1.0f) + FVector(0.0, 0.0, 90.0);
					Bit.Color = F.Color;
					Bit.Life = FMath::FRandRange(0.45f, 0.85f);
					Bit.Size = FMath::FRandRange(5.0f, 9.0f);
					Born.Add(Bit);
				}
				bDone = true;
			}
			break;
		case FFx::EKind::Falling:
		case FFx::EKind::Lost:
			F.Vel.Z -= Gravity * DeltaSeconds;
			F.Pos += F.Vel * DeltaSeconds;
			if (F.Pos.Z < F.Size * 0.5f && F.Vel.Z < 0.0f)
			{
				F.Pos.Z = F.Size * 0.5f;
				F.Vel.Z *= -0.38f;
				F.Vel.X *= 0.7f;
				F.Vel.Y *= 0.7f;
				++F.Bounces;
			}
			if (F.Age >= F.Life)
			{
				for (int32 K = 0; K < 6; ++K)
				{
					FFx Bit;
					Bit.Pos = F.Pos;
					Bit.Vel = FMath::VRand() * 260.0f * FMath::FRandRange(0.4f, 1.0f) + FVector(0.0, 0.0, 120.0);
					Bit.Color = F.Color;
					Bit.Life = FMath::FRandRange(0.4f, 0.7f);
					Bit.Size = FMath::FRandRange(4.0f, 7.0f);
					Born.Add(Bit);
				}
				bDone = true;
			}
			break;
		}
		if (bDone)
		{
			Fx.RemoveAtSwap(I);
		}
	}
	Fx.Append(Born);
}

FLinearColor ALanternBurst::ColorOf(int32 Orb)
{
	return LanternDetail::Colors[FMath::Clamp(Orb, 0, LanternBurst::MaxColors)];
}

void ALanternBurst::Draw()
{
	using namespace LanternDetail;
	const bool bAiming = bPlaying && !bFlying && bPreviewValid && Sim.Phase() == ELanternPhase::Playing;

	// The lantern.
	FInstances Lantern;
	if (!bLanternFallen)
	{
		for (int32 Row = 0; Row < GridRows; ++Row)
		{
			for (int32 Col = 0; Col < Columns; ++Col)
			{
				const int32 Orb = Sim.At(Row, Col);
				if (Orb == Empty)
				{
					continue;
				}
				const int32 Key = Row * Columns + Col;
				const float Grow = Ease((AppearClock - Row * 0.07f - Col * 0.004f) / 0.35f);
				if (Grow <= 0.01f)
				{
					continue;
				}
				FLinearColor Color = ColorOf(Orb);
				float Shine = 1.15f + 0.15f * FMath::Sin(Clock * 1.7f + Row * 0.9f + Col * 1.3f);
				double Size = OrbSize * Grow;
				FVector At = CellPosition(Row, Col);
				if (Orb == Spark)
				{
					Shine = 4.0f + 2.0f * FMath::Sin(Clock * 23.0f + Col);
				}
				if (bAiming && PreviewBurst.Contains(Key))
				{
					// What this shot would burst: white-hot and swelling with the beat.
					Color = FMath::Lerp(Color, FLinearColor::White, 0.35f);
					Shine = 5.0f + 2.0f * FMath::Sin(Clock * 12.0f);
					Size *= 1.0 + 0.07 * FMath::Sin(Clock * 12.0);
				}
				else if (bAiming && PreviewFall.Contains(Key))
				{
					// What it would bring down: trembling, as if about to let go.
					Shine = 2.8f + 1.2f * FMath::Sin(Clock * 9.0f + Col * 2.1f);
					At.Z += 1.5 * FMath::Sin(Clock * 31.0 + Row * 1.7 + Col);
				}
				Lantern.Add(At, Size, Color, Shine);
			}
		}
	}
	Lantern.Apply(Orbs);

	// The hand, the orb in flight and the ghost where the shot will stick.
	FInstances Hand;
	if (Sim.Phase() == ELanternPhase::Playing)
	{
		const float Breathe = 0.9f * FMath::Sin(Clock * 3.0f);
		if (!bFlying)
		{
			Hand.Add(LauncherPosition(), OrbSize, ColorOf(Sim.Current()), (Sim.Current() == Spark ? 5.0f : 2.4f) + Breathe);
		}
		Hand.Add(NextPosition(), OrbSize * 0.7, ColorOf(Sim.Next()), (Sim.Next() == Spark ? 4.0f : 1.6f));
		if (bFlying && FlightPath.Num() > 0)
		{
			int32 Leg = 1;
			while (Leg < FlightLength.Num() - 1 && FlightLength[Leg] < FlightDistance)
			{
				++Leg;
			}
			const int32 From = FMath::Max(0, Leg - 1);
			const double Span = FMath::Max(1e-6, FlightLength[Leg] - FlightLength[From]);
			const double U = FMath::Clamp((FlightDistance - FlightLength[From]) / Span, 0.0, 1.0);
			const FVector2D P = FMath::Lerp(FlightPath[From], FlightPath[Leg], U);
			Hand.Add(PointOnLantern(P.X, P.Y), OrbSize, ColorOf(FlightColor), 4.0f);
		}
		if (bAiming && PreviewTrace.bLands)
		{
			// The ghost where it will stick: its colour paled toward white, breathing, so it is found at
			// a glance (the first, dim and small, was lost among the orbs).
			const FLinearColor Ghost = PreviewResult.bOverLine ? EmberColor : FMath::Lerp(ColorOf(Sim.Current()), FLinearColor::White, 0.45f);
			Hand.Add(CellPosition(PreviewTrace.Cell.Row, PreviewTrace.Cell.Col), OrbSize * (0.78 + 0.08 * FMath::Sin(Clock * 6.0)), Ghost,
				PreviewResult.bOverLine ? 6.0f + 4.0f * FMath::Sin(Clock * 14.0f) : 3.5f + 1.5f * FMath::Sin(Clock * 6.0f));
		}
	}
	Hand.Apply(Live);

	// The guide: dots up the glass the way the shot will go.
	FInstances Dots;
	if (bAiming && PreviewTrace.Path.size() > 1)
	{
		const FLinearColor Color = FMath::Lerp(ColorOf(Sim.Current()), FLinearColor::White, 0.3f);
		// A dot every GuideSpacing along the path, creeping upward, none on top of the loaded orb.
		double Next = 0.9 + FMath::Fmod(static_cast<double>(Clock) * 1.2, GuideSpacing);
		double Run = 0.0;
		for (size_t I = 1; I < PreviewTrace.Path.size(); ++I)
		{
			const FVector2D A(PreviewTrace.Path[I - 1].X, PreviewTrace.Path[I - 1].Y);
			const FVector2D B(PreviewTrace.Path[I].X, PreviewTrace.Path[I].Y);
			const double Leg = FVector2D::Distance(A, B);
			while (Leg > 1e-9 && Next <= Run + Leg)
			{
				const FVector2D P = FMath::Lerp(A, B, (Next - Run) / Leg);
				Dots.Add(PointOnLantern(P.X, P.Y), 7.0, Color, 3.0f);
				Next += GuideSpacing;
			}
			Run += Leg;
		}
	}
	Dots.Apply(Guide);

	// Sparks, bursting orbs, falling orbs.
	FInstances Flying;
	for (const FFx& F : Fx)
	{
		if (F.Delay > 0.0f)
		{
			if (F.Kind == FFx::EKind::Bursting || F.Kind == FFx::EKind::Falling || F.Kind == FFx::EKind::Lost)
			{
				// Still on the lantern until its moment comes.
				Flying.Add(F.Pos, F.Size, F.Color, F.Kind == FFx::EKind::Bursting ? 4.0f : 1.6f);
			}
			continue;
		}
		const float T = FMath::Clamp(F.Age / FMath::Max(F.Life, 0.01f), 0.0f, 1.0f);
		switch (F.Kind)
		{
		case FFx::EKind::Spark:
			Flying.Add(F.Pos, F.Size * FMath::Sqrt(1.0f - T), F.Color, 8.0f * (1.0f - T) + 0.5f);
			break;
		case FFx::EKind::Bursting:
			Flying.Add(F.Pos, F.Size * (1.0f + 3.5f * F.Age), FMath::Lerp(F.Color, FLinearColor::White, 0.4f), 7.0f + 40.0f * F.Age);
			break;
		case FFx::EKind::Falling:
		case FFx::EKind::Lost:
			Flying.Add(F.Pos, F.Size, F.Color, 1.6f + (T > 0.8f ? (T - 0.8f) * 20.0f : 0.0f));
			break;
		}
	}
	Flying.Apply(Effects);

	// The embers, glowing harder the nearer the lowest orb comes.
	FInstances Coals;
	const int32 Lowest = Sim.LowestRow();
	const float RowsLeft = Lowest < 0 ? 9.0f : static_cast<float>(Sim.LineRow() - 1 - Lowest);
	const float Danger = FMath::Clamp(1.0f - RowsLeft / 3.0f, 0.0f, 1.0f);
	const double Ring = Radius + OrbSize * 0.5 + 14.0;
	for (int32 I = 0; I < EmberCount; ++I)
	{
		const double A = 2.0 * UE_DOUBLE_PI * I / EmberCount;
		const float Flicker = 0.5f + 0.5f * FMath::Sin(Clock * (5.0f + (I % 7)) + I * 2.3f);
		const float Pulse = Danger * (0.5f + 0.5f * FMath::Sin(Clock * (4.0f + 8.0f * Danger)));
		Coals.Add(FVector(FMath::Cos(A) * Ring, FMath::Sin(A) * Ring, EmberZ), 10.0 + 4.0 * Flicker, EmberColor,
			0.6f + 0.7f * Flicker + 9.0f * Pulse);   // dim red coals while it is safe; bloom turned brighter ones yellow
	}
	Coals.Apply(Embers);
}

void ALanternBurst::SyncFrame()
{
	using namespace LanternDetail;
	const double LidZ = Row0Z() + OrbSize * 0.5 + 4.0;
	Crown->SetRelativeLocation(FVector(0.0, 0.0, LidZ));
	Crown->SetRelativeRotation(FRotator(0.0, FMath::RadiansToDegrees(Turn), 0.0));
	const FVector HookAt(0.0, 0.0, HookZ);
	for (int32 I = 0; I < Chains.Num(); ++I)
	{
		const double A = Turn + 2.0 * UE_DOUBLE_PI * (I + 0.5) / Chains.Num();
		const FVector Foot(FMath::Cos(A) * Radius * 0.55, FMath::Sin(A) * Radius * 0.55, LidZ + 40.0);
		const FVector Span = HookAt - Foot;
		Chains[I]->SetRelativeLocationAndRotation((HookAt + Foot) * 0.5, FRotationMatrix::MakeFromZ(Span.GetSafeNormal()).Rotator());
		Chains[I]->SetRelativeScale3D(FVector(3.0 / ShapeSize, 3.0 / ShapeSize, Span.Size() / ShapeSize));
	}
	const int32 Lowest = Sim.LowestRow();
	const double Bottom = Row0Z() - (Lowest < 0 ? 2.0 : Lowest) * RowStep - OrbSize * 0.3;
	const double Height = FMath::Max(10.0, LidZ - Bottom);
	Core->SetRelativeLocation(FVector(0.0, 0.0, Bottom + Height * 0.5));
	Core->SetRelativeScale3D(FVector(FlameWidth / ShapeSize, FlameWidth / ShapeSize, Height / ShapeSize));
	Core->SetVisibility(!bLanternFallen);
	Glow->SetRelativeLocation(FVector(0.0, 0.0, Bottom + Height * 0.4));
	Glow->SetIntensity(LightCandelas * (1.0f + 4.0f * LightFlash));
	Glow->SetLightColor(FMath::Lerp(WarmLight, FlashColor, LightFlash));
}

void ALanternBurst::SyncCamera(float DeltaSeconds, bool bSnap)
{
	if (!LanternCamera)
	{
		return;
	}
	const FVector Loc(-CameraDistance, 0.0, CameraHeight);
	const FVector LookAt(0.0, 0.0, LookHeight);
	FVector Jolt = FVector::ZeroVector;
	if (Shake > 0.0f && !bSnap)
	{
		Jolt = FVector(0.0, FMath::Sin(Clock * 47.0f), FMath::Sin(Clock * 61.0f + 1.0f)) * Shake * 7.0f;
	}
	LanternCamera->SetFieldOfView(CameraFieldOfView);
	LanternCamera->SetRelativeLocationAndRotation(Loc + Jolt, FRotationMatrix::MakeFromX(LookAt - Loc).Rotator());
}

// ---------------------------------------------------------------------------------------------
// The panel, the best score

void ALanternBurst::RefreshPanel()
{
	using namespace LanternDetail;
	ASibeliusHUD* Hud = bPlaying ? GetHud() : nullptr;
	if (!Hud)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	FString Status = FString::Printf(TEXT("LANTERN BURST     STAGE %d     SCORE %s     BEST %s"),
		Sim.Stage(), *Grouped(Sim.Score()), *Grouped(BestScore));
	FString Note;
	FString Controls = TEXT("MOUSE aim   CLICK shoot   RIGHT CLICK / SPACE swap   A D / WHEEL turn the lantern   R R start over   ESC leave");
	switch (Sim.Phase())
	{
	case ELanternPhase::Playing:
	{
		const int32 Misses = Sim.MissesBeforeLower();
		Status += FString::Printf(TEXT("\n%sCROWN COMES DOWN AFTER %d %s"),
			Sim.Streak() > 0 ? *FString::Printf(TEXT("STREAK x%d     "), FMath::Min(Sim.Streak(), MaxMultiplier)) : TEXT(""),
			Misses, Misses == 1 ? TEXT("MORE MISS") : TEXT("MORE MISSES"));
		if (Now < LastNoteUntil)
		{
			Note = LastNote;
		}
		else if (bFlying)
		{
			Note = TEXT("");
		}
		else if (bPreviewValid)
		{
			if (PreviewResult.bOverLine)
			{
				Note = TEXT("OVER THE EMBERS  -  this one would end the game");
			}
			else if (PreviewResult.Burst > 0)
			{
				Note = FString::Printf(TEXT("BURSTS %d"), PreviewResult.Burst);
				if (PreviewResult.Fall > 0)
				{
					Note += FString::Printf(TEXT("   BRINGS DOWN %d"), PreviewResult.Fall);
				}
				if (PreviewResult.bClears)
				{
					Note += TEXT("   CLEARS THE LANTERN");
				}
			}
			else
			{
				Note = TEXT("no burst here  -  a miss");
			}
			const FLanternPoint Cell = FLanternBurstSim::CenterOf(PreviewTrace.Cell.Row, PreviewTrace.Cell.Col);
			if (!FacesFront(Cell.X))
			{
				Note += TEXT("   (round the back - turn the lantern with A D to see it)");
			}
		}
		break;
	}
	case ELanternPhase::StageClear:
		Note = FString::Printf(TEXT("STAGE %d CLEAR!   CLICK for stage %d"), Sim.Stage(), Sim.Stage() + 1);
		Controls = TEXT("CLICK next stage   ESC leave");
		break;
	case ELanternPhase::GameOver:
		Note = FString::Printf(TEXT("THE LANTERN TOUCHED THE EMBERS   -   SCORE %s, STAGE %d%s   -   CLICK to play again"),
			*Grouped(Sim.Score()), Sim.Stage(), bNewBestThisGame || (BestAtGameStart == 0 && Sim.Score() > 0) ? TEXT("   NEW BEST!") : TEXT(""));
		Controls = TEXT("CLICK play again   ESC leave");
		break;
	default:
		break;
	}
	Hud->ShowTablePanel(Status, Controls, 0.25f, Note);
}

ASibeliusHUD* ALanternBurst::GetHud() const
{
	const APlayerController* PC = PlayingPC.Get();
	return PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr;
}

void ALanternBurst::LoadBest()
{
	using namespace LanternDetail;
	if (!FSibeliusSaveIO::Has(SaveSlot))
	{
		return;   // never played. Not an error.
	}
	const ULanternBurstSaveGame* Save = Cast<ULanternBurstSaveGame>(FSibeliusSaveIO::Load(SaveSlot));
	if (!Save || Save->SaveVersion <= 0 || Save->SaveVersion > ULanternBurstSaveGame::CurrentSaveVersion)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Lantern] the best score could not be read; starting from none"));
		return;
	}
	BestScore = Save->BestScore;
	BestStage = Save->BestStage;
}

void ALanternBurst::SaveBest()
{
	using namespace LanternDetail;
	if (BestScore <= 0)
	{
		return;
	}
	ULanternBurstSaveGame* Save = NewObject<ULanternBurstSaveGame>(GetTransientPackage());
	Save->SaveVersion = ULanternBurstSaveGame::CurrentSaveVersion;
	Save->BestScore = BestScore;
	Save->BestStage = BestStage;
	if (!FSibeliusSaveIO::Commit(Save, SaveSlot))
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Lantern] could not write the best score"));
	}
}

// ---------------------------------------------------------------------------------------------
// Dev commands

void ALanternBurst::DevStart()
{
	if (!bPlaying && GetWorld())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			StartPlaying(PC);
		}
	}
}

// The best shot it can find with the lantern turned to any column, the hand as it is: turned
// there, aimed, and fired.
void ALanternBurst::DevShootBest()
{
	if (Sim.Phase() == ELanternPhase::StageClear)
	{
		Sim.NextStage();
		HandleEvents();
		return;
	}
	if (Sim.Phase() != ELanternPhase::Playing || bFlying)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Lantern] dev shot skipped: phase %d, flying %d."), static_cast<int32>(Sim.Phase()), bFlying ? 1 : 0);
		return;
	}
	double Best = -1e18, BestLaunch = FrontColumn(), BestAngle = 0.0;
	for (int32 Col = 0; Col < LanternBurst::Columns * 2; ++Col)
	{
		for (int32 Degrees = -70; Degrees <= 70; Degrees += 5)
		{
			const double From = Col * 0.5, Angle = FMath::DegreesToRadians(static_cast<double>(Degrees));
			const FLanternBurstSim::FPreview P = Sim.Preview(From, Angle);
			if (!P.bLegal)
			{
				continue;
			}
			const double Value = P.Burst * 10.0 + P.Fall * 25.0 - (P.bOverLine ? 1e9 : 0.0) - P.Cell.Row * 0.1;
			if (Value > Best)
			{
				Best = Value;
				BestLaunch = From;
				BestAngle = Angle;
			}
		}
	}
	// Turn the lantern so that column faces the launcher, then fire as he would.
	Turn = TurnTarget = UE_DOUBLE_PI - 2.0 * UE_DOUBLE_PI * BestLaunch / LanternBurst::Columns;
	AimAngle = BestAngle;
	PreviewShots = -1;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Lantern] dev shot: column %.1f, %.0f degrees, worth %.0f."),
		BestLaunch, FMath::RadiansToDegrees(BestAngle), Best);
	Launch();
}

void ALanternBurst::DevAim(double Degrees)
{
	AimAngle = FMath::Clamp(FMath::DegreesToRadians(Degrees), -LanternBurst::MaxAimAngle, LanternBurst::MaxAimAngle);
	if (const APlayerController* PC = PlayingPC.Get())
	{
		float X = 0.0f, Y = 0.0f;
		if (PC->GetMousePosition(X, Y))
		{
			LastMouse = FVector2D(X, Y);   // so the still mouse does not take it back
		}
	}
}

void ALanternBurst::DevTurn(double Degrees)
{
	Turn = TurnTarget = FMath::DegreesToRadians(Degrees);
}

namespace LanternDetail
{
	// The editor's console hands commands its own world; the lantern being played is in PIE's.
	ALanternBurst* FirstLantern(UWorld* Given, UWorld*& OutWorld)
	{
		OutWorld = Given;
		if (!(Given && Given->IsGameWorld()) && GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() && Context.World()->IsGameWorld())
				{
					OutWorld = Context.World();
					break;
				}
			}
		}
		if (OutWorld)
		{
			for (TActorIterator<ALanternBurst> It(OutWorld); It; ++It)
			{
				return *It;
			}
		}
		return nullptr;
	}

	double DegreesArg(const TArray<FString>& Args)
	{
		return Args.Num() > 0 ? FCString::Atod(*Args[0]) : 0.0;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GLanternPlay(
	TEXT("sib.Lantern.Play"),
	TEXT("Takes the first player to the first Lantern Burst in the level."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		UWorld* PlayWorld = nullptr;
		if (ALanternBurst* Lantern = LanternDetail::FirstLantern(World, PlayWorld))
		{
			Lantern->DevStart();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GLanternAim(
	TEXT("sib.Lantern.Aim"),
	TEXT("Aims Lantern Burst's shot D degrees off straight up (+ toward higher columns)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UWorld* PlayWorld = nullptr;
		if (ALanternBurst* Lantern = LanternDetail::FirstLantern(World, PlayWorld))
		{
			Lantern->DevAim(LanternDetail::DegreesArg(Args));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GLanternTurn(
	TEXT("sib.Lantern.Turn"),
	TEXT("Turns Lantern Burst's lantern to D degrees."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UWorld* PlayWorld = nullptr;
		if (ALanternBurst* Lantern = LanternDetail::FirstLantern(World, PlayWorld))
		{
			Lantern->DevTurn(LanternDetail::DegreesArg(Args));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GLanternFire(
	TEXT("sib.Lantern.Fire"),
	TEXT("Turns Lantern Burst to the best shot it can find and fires it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		UWorld* PlayWorld = nullptr;
		if (ALanternBurst* Lantern = LanternDetail::FirstLantern(World, PlayWorld))
		{
			Lantern->DevShootBest();
		}
	}));
