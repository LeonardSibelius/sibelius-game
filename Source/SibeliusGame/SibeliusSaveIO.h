// SibeliusSaveIO.h
//
// SIB-29 — Ch5 Phase 1. The actual SaveGame disk I/O, free of any GameInstance /
// subsystem lifecycle so it runs identically in-game AND in a bare commandlet
// (USaveSubsystem has ClassWithin=GameInstance and can't be NewObject'd headless).
// USaveSubsystem is the runtime chokepoint and simply delegates here;
// UBranchSubsystem::RequestDeploy and the headless smoke test call these statics
// directly. This struct is the single place UGameplayStatics save I/O is touched.

#pragma once

#include "CoreMinimal.h"

class USaveGame;

struct SIBELIUSGAME_API FSibeliusSaveIO
{
	// THE write. True on a successful disk write; null object / empty slot rejected.
	static bool Commit(USaveGame* SaveObject, const FString& SlotName, int32 UserIndex = 0);

	// I/O-only load (deserialize the object; does NOT re-apply to the world — that
	// re-apply is Ch5 Phase 2). Null if the slot is absent/unreadable.
	static USaveGame* Load(const FString& SlotName, int32 UserIndex = 0);

	static bool Has(const FString& SlotName, int32 UserIndex = 0);

	static bool Delete(const FString& SlotName, int32 UserIndex = 0);

	// Read the RAW save bytes (header + body) without deserializing the object — for
	// pre-validation, so a corrupt stream is rejected before it reaches the object
	// deserializer (which hard-asserts on garbage). False if absent/unreadable.
	static bool LoadRawBytes(const FString& SlotName, TArray<uint8>& OutBytes, int32 UserIndex = 0);
};
