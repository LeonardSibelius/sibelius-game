// Branchable.h
//
// SIB-28 — Ch4 "Test-Drive", Phase 0. A declared world-edit object that owns its
// own branch state. Implemented natively by URefactorableComponent, ABuildSite,
// and AHatchLock — the same "own and revert your own state" principle the
// Refactor/Compile chapters already use.

#pragma once

#include "CoreMinimal.h"
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
};
