// CurioCollectionSubsystem.cpp — see header.

#include "CurioCollectionSubsystem.h"
#include "ElsewhereSaveGame.h"
#include "ElsewhereSubsystem.h"
#include "SibeliusSaveIO.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogCurio, Log, All);

void UCurioCollectionSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);
	LoadFromDisk();   // pick up where the player left off (the Cabinet persists)
}

bool UCurioCollectionSubsystem::CollectCurio(FName CurioId, FName PlaceTypeId)
{
	const UElsewhereSubsystem* Elsewhere = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UElsewhereSubsystem>() : nullptr;
	const FCurioDef* Def = Elsewhere ? Elsewhere->FindCurio(CurioId) : nullptr;
	if (!Def)
	{
		UE_LOG(LogCurio, Warning, TEXT("[Curio] CollectCurio: unknown curio '%s' — ignored."), *CurioId.ToString());
		return false;
	}

	const bool bNew = Collection.Add(*Def, PlaceTypeId);
	UE_LOG(LogCurio, Display, TEXT("[Curio] collected '%s' (%s)%s — score=%d, unique=%d, total=%d"),
		*CurioId.ToString(), *PlaceTypeId.ToString(), bNew ? TEXT(" [NEW]") : TEXT(" [dupe]"),
		Collection.Score, Collection.Owned.Num(), Collection.TotalCollected);

	SaveToDisk();
	OnCollectionChanged.Broadcast();
	return bNew;
}

bool UCurioCollectionSubsystem::SaveToDisk() const
{
	UElsewhereSaveGame* Save = NewObject<UElsewhereSaveGame>();
	Save->SaveVersion = UElsewhereSaveGame::CurrentSaveVersion;
	Save->Collection = Collection;
	const bool bOk = FSibeliusSaveIO::Commit(Save, SaveSlotName());
	if (!bOk)
	{
		UE_LOG(LogCurio, Error, TEXT("[Curio] SaveToDisk FAILED (slot '%s')."), SaveSlotName());
	}
	return bOk;
}

bool UCurioCollectionSubsystem::LoadFromDisk()
{
	UElsewhereSaveGame* Save = Cast<UElsewhereSaveGame>(FSibeliusSaveIO::Load(SaveSlotName()));
	if (!Save)
	{
		return false;   // no save yet — start with an empty Cabinet
	}
	Collection = Save->Collection;
	UE_LOG(LogCurio, Log, TEXT("[Curio] loaded: score=%d, unique=%d, total=%d"),
		Collection.Score, Collection.Owned.Num(), Collection.TotalCollected);
	OnCollectionChanged.Broadcast();
	return true;
}
