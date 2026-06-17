// ElsewhereBuilder.cpp — see header. Deterministic C++ assembly (the PCG seam).

#include "ElsewhereBuilder.h"
#include "ElsewhereGen.h"
#include "ElsewhereSubsystem.h"
#include "Curio.h"
#include "ReturnDoor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"

DEFINE_LOG_CATEGORY_STATIC(LogElsewhereBuilder, Log, All);

AElsewhereBuilder::AElsewhereBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// ISM renders fine at runtime (unlike a bare runtime UStaticMeshComponent — the
	// null-proxy lesson), and is exactly how a modular kit tiles a floor cheaply.
	FloorISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FloorISM"));
	FloorISM->SetupAttachment(SceneRoot);
	FloorISM->SetCanEverAffectNavigation(false);
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		FloorISM->SetStaticMesh(Cube);
	}

	PropISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PropISM"));
	PropISM->SetupAttachment(SceneRoot);
	PropISM->SetCanEverAffectNavigation(false);
	if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		PropISM->SetStaticMesh(Cyl);
	}

	MoodLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MoodLight"));
	MoodLight->SetupAttachment(SceneRoot);
	MoodLight->SetAttenuationRadius(3000.f);
	MoodLight->CastShadows = false;   // public UPROPERTY on ULightComponentBase
}

void AElsewhereBuilder::BeginPlay()
{
	Super::BeginPlay();

	// Runtime path: pull the plan + registry the Sauce Door staged before travel.
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UElsewhereSubsystem* Elsewhere = GI ? GI->GetSubsystem<UElsewhereSubsystem>() : nullptr;
	if (!Elsewhere || !Elsewhere->HasStagedElsewhere())
	{
		UE_LOG(LogElsewhereBuilder, Warning,
			TEXT("[%s] no staged Elsewhere at BeginPlay — nothing to build (open this map via the Sauce Door)."),
			*GetName());
		return;
	}

	BuildFromPlan(Elsewhere->GetStagedPlan(), Elsewhere->GetPlaceTypes(), Elsewhere->GetCurios());
}

int32 AElsewhereBuilder::BuildFromPlan(
	const FElsewherePlan& Plan,
	const TArray<FPlaceTypeDef>& Places,
	const TArray<FCurioDef>& Curios)
{
	if (!Plan.IsValid())
	{
		return 0;
	}
	const FPlaceTypeDef* Place = FElsewhereGen::FindPlace(Places, Plan.PlaceTypeId);
	const FCurioDef* CurioDef = FElsewhereGen::FindCurio(Curios, Plan.CurioId);
	if (!Place || !CurioDef)
	{
		UE_LOG(LogElsewhereBuilder, Error, TEXT("[%s] plan references missing place/curio — aborting build."), *GetName());
		return 0;
	}

	const int32 PropCount = AssembleGeometry(*Place, Plan.LayoutSeed);
	ApplyMood(*Place, Plan.MoodSeed);

	UWorld* World = GetWorld();
	if (!World)
	{
		return PropCount;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The ONE curio (§6), placed + configured from the plan.
	const FTransform CurioXf(FRotator::ZeroRotator, GetActorLocation() + CurioOffset);
	SpawnedCurio = World->SpawnActor<ACurio>(ACurio::StaticClass(), CurioXf, SpawnParams);
	if (SpawnedCurio)
	{
		SpawnedCurio->Configure(Plan.CurioId, Plan.PlaceTypeId, Place->CurioGlowColor);
	}

	// The way home (§3 step 6).
	const FTransform ReturnXf(FRotator::ZeroRotator, GetActorLocation() + ReturnDoorOffset);
	SpawnedReturnDoor = World->SpawnActor<AReturnDoor>(AReturnDoor::StaticClass(), ReturnXf, SpawnParams);

	UE_LOG(LogElsewhereBuilder, Display,
		TEXT("[%s] built '%s' (seed=%d): %d props, curio='%s', return door=%s"),
		*GetName(), *Place->Id.ToString(), Plan.Seed, PropCount, *Plan.CurioId.ToString(),
		SpawnedReturnDoor ? TEXT("ok") : TEXT("FAILED"));

	return PropCount;
}

int32 AElsewhereBuilder::AssembleGeometry(const FPlaceTypeDef& Place, int32 LayoutSeed)
{
	// --- PCG SEAM (see header): a UPCGComponent->Generate() would replace this block.
	FloorISM->ClearInstances();
	PropISM->ClearInstances();

	const FVector Origin = GetActorLocation();
	const float TileSize = 200.f;   // engine cube is 100cm; 2x = a 2m floor tile
	const int32 TilesX = FMath::Max(1, FMath::RoundToInt(Place.RoomExtent.X / TileSize));
	const int32 TilesY = FMath::Max(1, FMath::RoundToInt(Place.RoomExtent.Y / TileSize));

	// Modular floor grid (centered on the builder).
	for (int32 X = -TilesX; X <= TilesX; ++X)
	{
		for (int32 Y = -TilesY; Y <= TilesY; ++Y)
		{
			const FVector Loc = Origin + FVector(X * TileSize, Y * TileSize, -50.f);
			const FTransform Tile(FRotator::ZeroRotator, Loc, FVector(TileSize / 100.f, TileSize / 100.f, 0.2f));
			FloorISM->AddInstance(Tile, /*bWorldSpace=*/true);
		}
	}

	// Seeded prop scatter — count + placement are pure functions of LayoutSeed, so the
	// same seed reproduces the same room (the determinism guarantee).
	FRandomStream Rng(LayoutSeed);
	const int32 PropCount = Rng.RandRange(Place.PropCountMin, Place.PropCountMax);
	for (int32 i = 0; i < PropCount; ++i)
	{
		const FVector Loc = Origin + FVector(
			Rng.FRandRange(-Place.RoomExtent.X, Place.RoomExtent.X),
			Rng.FRandRange(-Place.RoomExtent.Y, Place.RoomExtent.Y),
			0.f);
		const float Scale = Rng.FRandRange(0.6f, 1.8f);
		const FRotator Rot(0.f, Rng.FRandRange(0.f, 360.f), 0.f);
		const FTransform Prop(Rot, Loc, FVector(Scale, Scale, Rng.FRandRange(1.0f, 3.0f)));
		PropISM->AddInstance(Prop, /*bWorldSpace=*/true);
	}

	return PropCount;
}

void AElsewhereBuilder::ApplyMood(const FPlaceTypeDef& Place, int32 MoodSeed)
{
	// Tint the ambient glow from the place, with a small seeded jitter so two visits to
	// the same place-type still feel a touch different (§4 variation lever).
	FRandomStream Rng(MoodSeed);
	const float Jitter = Rng.FRandRange(0.85f, 1.15f);
	MoodLight->SetLightColor(Place.AmbientColor);
	MoodLight->SetIntensity(Place.LightIntensity * 2000.f * Jitter);
	MoodLight->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, Place.RoomExtent.Z * 0.5f));
}
