// Branchable.h
//
// SIB-28 — Ch4 "Test-Drive", Phase 0. A declared world-edit object that owns its
// own branch state. Implemented natively by URefactorableComponent, ABuildSite,
// and AHatchLock — the same "own and revert your own state" principle the
// Refactor/Compile chapters already use.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/Interface.h"
#include "Branchable.generated.h"

UINTERFACE(MinimalAPI)
class UBranchable : public UInterface
{
	GENERATED_BODY()
};

class IBranchable
{
	GENERATED_BODY()

public:
	// Pack this object's declared state (one bool today) for the manifest.
	virtual uint8 CaptureBranchState() const = 0;

	// Write RAW state back. NEVER replay the gameplay verb — Build()/TryUnlock()/
	// Collect() have side effects (spend/grant resources, hide a pickup). The
	// object restores its own exact visuals from this raw bool.
	virtual void RestoreBranchState(uint8 State) = 0;

	// The authored/pristine declared state — the byte CaptureBranchState() returns
	// before any gameplay verb (Refactorable/BuildSite = 0; HatchLock = 1, locked).
	// Ch5 Deploy persists DELTAS: only objects whose current state differs from this
	// get an entry, so an untouched world writes an empty save.
	virtual uint8 GetDefaultBranchState() const = 0;

	// SIB-29 — Ch5 Phase 0 (GUID identity seam). Identity is a STABLE per-object
	// FGuid, not a registry array index: it survives a re-register (and, from Ch5
	// Phase 1, a SaveGame reload). Each implementer stores a persisted
	// UPROPERTY(SaveGame) FGuid, assigned ONCE if invalid and NEVER regenerated on
	// load (the deserialized value is kept). The subsystem indexes/resolves
	// branchables by this GUID and the manifest keys by it.
	//
	//   GetOrCreateBranchId() — assign-once-if-invalid, then return. Call at
	//                           registration so the id exists before first use.
	//   GetBranchId()         — const read (valid only after a create has run).
	virtual FGuid GetOrCreateBranchId() = 0;
	virtual FGuid GetBranchId() const = 0;

protected:
	// Shared assign-once helper for implementers. Never overwrites a valid id, so a
	// value loaded from disk (Ch5) or assigned earlier this session is preserved.
	static void AssignBranchIdIfInvalid(FGuid& Id)
	{
		if (!Id.IsValid())
		{
			Id = FGuid::NewGuid();
		}
	}
};
