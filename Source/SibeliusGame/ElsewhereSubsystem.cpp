// ElsewhereSubsystem.cpp — see header.

#include "ElsewhereSubsystem.h"
#include "ElsewhereGen.h"
#include "Engine/DataTable.h"
#include "Misc/DateTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogElsewhere, Log, All);

namespace
{
	// Editor-overridable content. Absent on a fresh clone -> code defaults kick in
	// (same contract as the Carousel's DT_CarouselCharms).
	const TCHAR* PlacesTablePath = TEXT("/Game/Data/DT_ElsewherePlaces.DT_ElsewherePlaces");
	const TCHAR* CuriosTablePath = TEXT("/Game/Data/DT_ElsewhereCurios.DT_ElsewhereCurios");
}

void UElsewhereSubsystem::LoadRegistry(TArray<FPlaceTypeDef>& OutPlaces, TArray<FCurioDef>& OutCurios)
{
	OutPlaces.Reset();
	OutCurios.Reset();

	// Places: DataTable if authored, else code defaults.
	if (UDataTable* PlacesTable = LoadObject<UDataTable>(nullptr, PlacesTablePath))
	{
		PlacesTable->ForeachRow<FPlaceTypeDef>(TEXT("ElsewherePlaces"),
			[&OutPlaces](const FName& /*Key*/, const FPlaceTypeDef& Row) { OutPlaces.Add(Row); });
	}
	if (OutPlaces.Num() == 0)
	{
		FElsewhereGen::BuildDefaultPlaceTypes(OutPlaces);
	}

	// Curios: DataTable if authored, else code defaults.
	if (UDataTable* CuriosTable = LoadObject<UDataTable>(nullptr, CuriosTablePath))
	{
		CuriosTable->ForeachRow<FCurioDef>(TEXT("ElsewhereCurios"),
			[&OutCurios](const FName& /*Key*/, const FCurioDef& Row) { OutCurios.Add(Row); });
	}
	if (OutCurios.Num() == 0)
	{
		FElsewhereGen::BuildDefaultCurios(OutCurios);
	}
}

void UElsewhereSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadRegistry(PlaceTypes, Curios);
	UE_LOG(LogElsewhere, Log, TEXT("[Elsewhere] registry: %d place-types, %d curios."),
		PlaceTypes.Num(), Curios.Num());
}

const FPlaceTypeDef* UElsewhereSubsystem::FindPlace(const FName& Id) const
{
	return FElsewhereGen::FindPlace(PlaceTypes, Id);
}

const FCurioDef* UElsewhereSubsystem::FindCurio(const FName& Id) const
{
	return FElsewhereGen::FindCurio(Curios, Id);
}

FElsewherePlan UElsewhereSubsystem::StageNextElsewhere(int32 Seed)
{
	if (Seed < 0)
	{
		// Draw a non-deterministic seed; XOR a rolling counter so two stages in the
		// same tick still differ. Kept positive (FRandomStream takes any int32, but a
		// stable positive seed is tidier in logs).
		const int32 Clock = static_cast<int32>(FDateTime::UtcNow().GetTicks() & 0x7fffffff);
		Seed = (Clock ^ (++StageCounter * 2654435761u)) & 0x7fffffff;
	}

	StagedPlan = FElsewhereGen::RollPlan(Seed, PlaceTypes, Curios);

	if (StagedPlan.IsValid())
	{
		UE_LOG(LogElsewhere, Log, TEXT("[Elsewhere] staged seed=%d -> place=%s curio=%s"),
			StagedPlan.Seed, *StagedPlan.PlaceTypeId.ToString(), *StagedPlan.CurioId.ToString());
		OnElsewhereStaged.Broadcast(StagedPlan);
	}
	else
	{
		UE_LOG(LogElsewhere, Warning, TEXT("[Elsewhere] StageNextElsewhere produced an INVALID plan (empty content?)."));
	}
	return StagedPlan;
}

void UElsewhereSubsystem::DiscardStagedElsewhere()
{
	StagedPlan = FElsewherePlan();   // the room is throwaway (§4)
}
