// SibeliusSaveIO.cpp — SIB-29 Ch5 Phase 1. GameInstance-free SaveGame disk I/O.

#include "SibeliusSaveIO.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"
#include "SibeliusGame.h"   // LogSibeliusGame

bool FSibeliusSaveIO::Commit(USaveGame* SaveObject, const FString& SlotName, int32 UserIndex)
{
	if (!SaveObject || SlotName.IsEmpty())
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Save] Commit rejected: %s."),
			!SaveObject ? TEXT("null save object") : TEXT("empty slot name"));
		return false;
	}
	const bool bOk = UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, UserIndex);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Save] Commit to slot '%s': %s."),
		*SlotName, bOk ? TEXT("OK") : TEXT("FAILED"));
	return bOk;
}

USaveGame* FSibeliusSaveIO::Load(const FString& SlotName, int32 UserIndex)
{
	if (SlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return nullptr;
	}
	return UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
}

bool FSibeliusSaveIO::Has(const FString& SlotName, int32 UserIndex)
{
	return !SlotName.IsEmpty() && UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

bool FSibeliusSaveIO::Delete(const FString& SlotName, int32 UserIndex)
{
	if (SlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}
	return UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}
