// SaveSubsystem.cpp — SIB-29 Ch5 Phase 1. Runtime chokepoint; delegates the actual
// disk I/O to FSibeliusSaveIO (which is GameInstance-free and works headless too).

#include "SaveSubsystem.h"
#include "SibeliusSaveIO.h"

bool USaveSubsystem::CommitSave(USaveGame* SaveObject, const FString& SlotName, int32 UserIndex) const
{
	return FSibeliusSaveIO::Commit(SaveObject, SlotName, UserIndex);
}

USaveGame* USaveSubsystem::LoadSave(const FString& SlotName, int32 UserIndex) const
{
	return FSibeliusSaveIO::Load(SlotName, UserIndex);
}

bool USaveSubsystem::HasSave(const FString& SlotName, int32 UserIndex) const
{
	return FSibeliusSaveIO::Has(SlotName, UserIndex);
}

bool USaveSubsystem::DeleteSave(const FString& SlotName, int32 UserIndex) const
{
	return FSibeliusSaveIO::Delete(SlotName, UserIndex);
}
