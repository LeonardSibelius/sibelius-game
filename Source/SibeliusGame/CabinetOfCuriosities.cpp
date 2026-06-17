// CabinetOfCuriosities.cpp — see header.

#include "CabinetOfCuriosities.h"
#include "CurioCollectionSubsystem.h"
#include "ElsewhereSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogCabinet, Log, All);

ACabinetOfCuriosities::ACabinetOfCuriosities()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SlotISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SlotISM"));
	SlotISM->SetupAttachment(SceneRoot);
	SlotISM->SetCanEverAffectNavigation(false);
	SlotISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlotISM->SetCollisionResponseToAllChannels(ECR_Block);   // E focuses the cabinet
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		SlotISM->SetStaticMesh(Cube);
	}
	// Per-instance custom data drives a filled/dim tint in the material (set up in the
	// editor follow-up); until then the count IS the visible fill (slots added only
	// for the registry, scaled up when owned — see RefreshFrom).
	SlotISM->NumCustomDataFloats = 1;
}

void ACabinetOfCuriosities::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (UCurioCollectionSubsystem* Collection = GI->GetSubsystem<UCurioCollectionSubsystem>())
		{
			Collection->OnCollectionChanged.AddDynamic(this, &ACabinetOfCuriosities::HandleCollectionChanged);
		}
	}
	Refresh();
}

void ACabinetOfCuriosities::HandleCollectionChanged()
{
	Refresh();
}

FCabinetState ACabinetOfCuriosities::Refresh()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UElsewhereSubsystem* Elsewhere = GI ? GI->GetSubsystem<UElsewhereSubsystem>() : nullptr;
	UCurioCollectionSubsystem* Collection = GI ? GI->GetSubsystem<UCurioCollectionSubsystem>() : nullptr;
	if (!Elsewhere || !Collection)
	{
		return LastState;
	}

	TArray<FName> AllIds;
	for (const FCurioDef& C : Elsewhere->GetCurios())
	{
		AllIds.Add(C.Id);
	}
	TArray<FName> OwnedIds;
	for (const FCollectedCurio& C : Collection->GetCollection().Owned)
	{
		OwnedIds.Add(C.CurioId);
	}

	return RefreshFrom(AllIds, OwnedIds, Collection->GetScore());
}

FCabinetState ACabinetOfCuriosities::RefreshFrom(const TArray<FName>& AllCurioIds, const TArray<FName>& OwnedIds, int32 Score)
{
	SlotISM->ClearInstances();

	const FVector Origin = GetActorLocation();
	int32 Filled = 0;

	for (int32 i = 0; i < AllCurioIds.Num(); ++i)
	{
		const bool bOwned = OwnedIds.Contains(AllCurioIds[i]);
		if (bOwned)
		{
			++Filled;
		}

		// A row of slots; owned ones stand tall + bright, empty ones sit low + dim.
		const FVector Loc = Origin + FVector(0.f, i * SlotSpacing, 0.f);
		const float Height = bOwned ? 1.0f : 0.3f;
		const FTransform Slot(FRotator::ZeroRotator, Loc, FVector(0.4f, 0.4f, Height));
		const int32 Idx = SlotISM->AddInstance(Slot, /*bWorldSpace=*/true);
		SlotISM->SetCustomDataValue(Idx, 0, bOwned ? 1.0f : 0.0f);   // material reads this for tint
	}

	LastState.Filled = Filled;
	LastState.Total = AllCurioIds.Num();
	LastState.Score = Score;

	UE_LOG(LogCabinet, Log, TEXT("[%s] Cabinet: %d/%d curios on display, score=%d."),
		*GetName(), Filled, AllCurioIds.Num(), Score);
	return LastState;
}

void ACabinetOfCuriosities::Interact_Implementation(AActor* /*Interactor*/)
{
	const FCabinetState S = Refresh();
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
			FString::Printf(TEXT("Cabinet of Curiosities: %d / %d found  —  score %d"), S.Filled, S.Total, S.Score));
	}
}

FText ACabinetOfCuriosities::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("Sibelius", "CabinetPrompt", "Examine the Cabinet of Curiosities [E]");
}
