// SaveSubsystem.h
//
// SIB-29 — Ch5 Phase 1. The RUNTIME chokepoint for SaveGame disk I/O. A
// GameInstance subsystem so in-game code reaches it via
// GetGameInstance()->GetSubsystem<USaveSubsystem>(); it delegates the actual work
// to FSibeliusSaveIO. The disk logic lives in that helper (not here) because a
// UGameInstanceSubsystem has ClassWithin=GameInstance and cannot be created in a
// bare commandlet — so UBranchSubsystem::RequestDeploy and the headless smoke test
// call FSibeliusSaveIO directly instead of going through this subsystem.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveSubsystem.generated.h"

class USaveGame;

UCLASS()
class SIBELIUSGAME_API USaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// THE write. Returns true on a successful disk write. Null object / empty slot
	// are rejected (false) — callers never reach UGameplayStatics directly.
	bool CommitSave(USaveGame* SaveObject, const FString& SlotName, int32 UserIndex = 0) const;

	// I/O-only load (returns the deserialized object; does NOT re-apply to the
	// world — that re-apply is Ch5 Phase 2). Null if the slot is absent/unreadable.
	USaveGame* LoadSave(const FString& SlotName, int32 UserIndex = 0) const;

	bool HasSave(const FString& SlotName, int32 UserIndex = 0) const;

	// Centralised deletion (used for the smoke test's sandbox-slot cleanup).
	bool DeleteSave(const FString& SlotName, int32 UserIndex = 0) const;
};
