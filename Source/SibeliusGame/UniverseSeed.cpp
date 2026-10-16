// UniverseSeed.cpp - see the header for what the seed is and why it waits for all six powers.

#include "UniverseSeed.h"

#include "ProgressionSubsystem.h"
#include "ProgressionTypes.h"
#include "SibeliusGame.h"
#include "SibeliusHUD.h"
#include "TravelTransitionSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace UniverseSeedDetail
{
	constexpr double ShapeSize = 100.0;          // the engine's basic shapes are a metre across
	constexpr double ColumnHeight = 100.0;
	constexpr double GrainHeight = 132.0;        // the grain's centre, above the floor
	constexpr double GrainSize = 5.0;            // cm across: a grain
	constexpr double HaloSize = 22.0;

	// LINEAR colours, which is what a material multiplies.
	const FLinearColor Stone(0.035f, 0.033f, 0.04f);
	const FLinearColor Light(1.0f, 0.86f, 0.6f);
	const FLinearColor HaloBlue(0.55f, 0.75f, 1.0f);
}

AUniverseSeed::AUniverseSeed()
{
	using namespace UniverseSeedDetail;
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowFinder(TEXT("/Game/Genesis/M_GenesisGlow.M_GenesisGlow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassFinder(TEXT("/Game/Genesis/M_GenesisGlass.M_GenesisGlass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StoneFinder(TEXT("/Game/Go/M_GoSurface.M_GoSurface"));
	GlowMaterial = GlowFinder.Object;
	GlassMaterial = GlassFinder.Object;
	StoneMaterial = StoneFinder.Object;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	auto MakePart = [this](const TCHAR* Name, UStaticMesh* Mesh, const FVector& At, const FVector& Size, bool bSolid)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(SceneRoot);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(At);
		Part->SetRelativeScale3D(Size / ShapeSize);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		if (bSolid)
		{
			// He bumps into the stone; the interaction trace goes to Reach.
			Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
		else
		{
			Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Part->SetCastShadow(false);
		}
		return Part;
	};
	// The cube's and the cylinder's origins are their middles.
	Base = MakePart(TEXT("Base"), CubeMesh.Object, FVector(0.0, 0.0, 4.0), FVector(44.0, 44.0, 8.0), true);
	Column = MakePart(TEXT("Column"), CylinderMesh.Object, FVector(0.0, 0.0, 8.0 + (ColumnHeight - 8.0) * 0.5), FVector(20.0, 20.0, ColumnHeight - 8.0), true);
	Dish = MakePart(TEXT("Dish"), CylinderMesh.Object, FVector(0.0, 0.0, ColumnHeight + 2.0), FVector(36.0, 36.0, 4.0), true);
	Grain = MakePart(TEXT("Grain"), SphereMesh.Object, FVector(0.0, 0.0, GrainHeight), FVector(GrainSize), false);
	Halo = MakePart(TEXT("Halo"), SphereMesh.Object, FVector(0.0, 0.0, GrainHeight), FVector(HaloSize), false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	Glow->SetRelativeLocation(FVector(0.0, 0.0, GrainHeight));
	Glow->SetIntensity(12.0f);
	Glow->SetAttenuationRadius(320.0f);
	Glow->SetLightColor(Light);
	Glow->SetCastShadows(false);

	Reach = CreateDefaultSubobject<UBoxComponent>(TEXT("Reach"));
	Reach->SetupAttachment(SceneRoot);
	Reach->SetRelativeLocation(FVector(0.0, 0.0, 80.0));
	Reach->SetBoxExtent(FVector(35.0, 35.0, 80.0));
	Reach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Reach->SetCollisionResponseToAllChannels(ECR_Ignore);
	Reach->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Reach->SetGenerateOverlapEvents(false);
}

void AUniverseSeed::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Paint();
}

void AUniverseSeed::BeginPlay()
{
	Super::BeginPlay();
	Paint();
}

void AUniverseSeed::Paint()
{
	using namespace UniverseSeedDetail;
	if (StoneMaterial)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(StoneMaterial, this);
		M->SetVectorParameterValue(TEXT("Color"), Stone);
		M->SetScalarParameterValue(TEXT("Roughness"), 0.3f);
		M->SetScalarParameterValue(TEXT("PatternAmount"), 0.0f);
		for (UStaticMeshComponent* Part : { Base.Get(), Column.Get(), Dish.Get() })
		{
			Part->SetMaterial(0, M);
		}
	}
	if (GlowMaterial)
	{
		GrainLight = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		GrainLight->SetVectorParameterValue(TEXT("Color"), Light);
		GrainLight->SetScalarParameterValue(TEXT("Intensity"), 30.0f);
		Grain->SetMaterial(0, GrainLight);
	}
	if (GlassMaterial)
	{
		HaloGlass = UMaterialInstanceDynamic::Create(GlassMaterial, this);
		HaloGlass->SetVectorParameterValue(TEXT("Color"), HaloBlue);
		HaloGlass->SetScalarParameterValue(TEXT("Intensity"), 1.5f);
		HaloGlass->SetScalarParameterValue(TEXT("RimOpacity"), 0.35f);
		HaloGlass->SetScalarParameterValue(TEXT("BaseOpacity"), 0.0f);
		Halo->SetMaterial(0, HaloGlass);
	}
}

int32 AUniverseSeed::PowersHeld() const
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	return Progression ? Progression->NumUnlocked() : 0;
}

FText AUniverseSeed::GetInteractionPrompt_Implementation() const
{
	const int32 All = static_cast<int32>(EPowerVerb::Count);
	const int32 Held = PowersHeld();
	if (Swell >= 0.0f)
	{
		return FText::GetEmpty();
	}
	return Held >= All
		? NSLOCTEXT("Seed", "Open", "A grain of light     [E] look into it")
		: FText::Format(NSLOCTEXT("Seed", "Shut", "A grain of light. It waits for all six powers - you have {0}."), FText::AsNumber(Held));
}

void AUniverseSeed::Interact_Implementation(AActor* Interactor)
{
	const int32 All = static_cast<int32>(EPowerVerb::Count);
	const int32 Held = PowersHeld();
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Held < All)
	{
		if (ASibeliusHUD* Hud = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
		{
			Hud->ShowBanner(FString::Printf(TEXT("IT WAITS FOR ALL SIX POWERS - YOU HAVE %d OF %d"), Held, All), 4.0f);
		}
		return;
	}
	if (Swell >= 0.0f)
	{
		return;
	}
	// It opens: he stands still and the grain fills his view, then the city (Tick).
	if (PC)
	{
		PC->SetIgnoreMoveInput(true);
	}
	Swell = 0.0f;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Seed] opened with all %d powers: to %s."), All, *CityLevel.ToString());
}

void AUniverseSeed::Tick(float DeltaSeconds)
{
	using namespace UniverseSeedDetail;
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	// At rest it breathes: the grain drifts a little and its light rises and falls.
	const float Breath = 0.5f + 0.5f * FMath::Sin(Age * 1.7f);
	double GrainScale = GrainSize;
	double HaloScale = HaloSize * (0.92 + 0.12 * Breath);
	float Brightness = 22.0f + 18.0f * Breath;
	float LightLevel = 8.0f + 6.0f * Breath;

	if (Swell >= 0.0f)
	{
		// The swell: slow, then all at once - a grain to a wall of light.
		Swell = FMath::Min(1.0f, Swell + DeltaSeconds / FMath::Max(SwellSeconds, 0.2f));
		const double S = FMath::Pow(static_cast<double>(Swell), 3.0);
		GrainScale = FMath::Lerp(GrainSize, 900.0, S);
		HaloScale = FMath::Lerp(HaloSize, 1400.0, S);
		Brightness = FMath::Lerp(40.0f, 400.0f, static_cast<float>(S));
		LightLevel = FMath::Lerp(14.0f, 400.0f, static_cast<float>(S));
		if (Swell >= 1.0f && !bTravelled)
		{
			bTravelled = true;
			UTravelTransitionSubsystem::Travel(this, CityLevel, ArrivalTag);
		}
	}
	const FVector Float(0.0, 0.0, GrainHeight + 2.0 * FMath::Sin(Age * 0.9f));
	Grain->SetRelativeLocationAndRotation(Float, FRotator(0.0, Age * 20.0, 0.0));
	Grain->SetRelativeScale3D(FVector(GrainScale / ShapeSize));
	Halo->SetRelativeLocation(Float);
	Halo->SetRelativeScale3D(FVector(HaloScale / ShapeSize));
	Glow->SetRelativeLocation(Float);
	Glow->SetIntensity(LightLevel);
	if (GrainLight)
	{
		GrainLight->SetScalarParameterValue(TEXT("Intensity"), Brightness);
	}
}
