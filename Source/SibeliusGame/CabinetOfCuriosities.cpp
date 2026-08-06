// CabinetOfCuriosities.cpp — see header.

#include "CabinetOfCuriosities.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
#include "CurioCollectionSubsystem.h"
#include "ElsewhereSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	// Force a VALID material so the slots never render as the WorldGridMaterial checker
	// (the engine cube ISM defaulted to it). BasicShapeMaterial is engine-always-present
	// and exposes a "Color" param we tint in BeginPlay — no kit dependency.
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		SlotISM->SetMaterial(0, Base);
	}
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

	// Tint the slot material to the cabinet's filled colour (gold by default).
	if (SlotISM)
	{
		if (UMaterialInstanceDynamic* MID = SlotISM->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FilledColor);
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

	// Render ONLY owned curios — the cabinet starts empty (no cubes) and fills as you
	// collect. (Previously it drew a cube per KNOWN curio, so an unplayed cabinet was a
	// long row of empty placeholder cubes — the "hallway trail".) Owned slots pack into
	// a tidy growing row.
	for (int32 i = 0; i < AllCurioIds.Num(); ++i)
	{
		if (!OwnedIds.Contains(AllCurioIds[i]))
		{
			continue;
		}
		const FVector Loc = Origin + FVector(0.f, Filled * SlotSpacing, 0.f);
		const FTransform Slot(FRotator::ZeroRotator, Loc, FVector(0.4f, 0.4f, 0.4f));
		SlotISM->AddInstance(Slot, /*bWorldSpace=*/true);
		++Filled;
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
	ASibeliusHUD::Toast(this,
		FString::Printf(TEXT("Cabinet of Curiosities: %d / %d found  —  score %d"), S.Filled, S.Total, S.Score),
		4.0f, SibeliusToast::Prize);
}

FText ACabinetOfCuriosities::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("Sibelius", "CabinetPrompt", "Examine the Cabinet of Curiosities [E]");
}
