// BranchTypes.h
//
// SIB-28 — Ch4 "Test-Drive", Phase 0 (the seam). The bounded branch snapshot +
// the subsystem state enum. Mirrors docs/ch4-spike-notes.md exactly: a declared
// set of bool-per-object + the inventory ledger, never a world dump.
//
// Identity is an in-session registry index (Ch5 Deploy promotes it to a stable
// FGuid — FBuildRecord::SiteId in CompileTypes.h is the groundwork).

#pragma once

#include "CoreMinimal.h"
#include "CompileTypes.h"   // EResourceType
#include "BranchTypes.generated.h"

// Subsystem FSM: Main (the canonical world), Branched (a sandbox is active),
// Resolving (a merge/discard is committing — the one-resolution-per-branch latch).
UENUM()
enum class EBranchState : uint8
{
	Main,
	Branched,
	Resolving
};

// One registered branchable's packed declared state (a single bool today).
USTRUCT()
struct FBranchObjectState
{
	GENERATED_BODY()

	UPROPERTY() int32 RegistryIndex = INDEX_NONE;
	UPROPERTY() uint8 State = 0;
};

// The inventory ledger captured whole (2 entries today: Book, Key).
USTRUCT()
struct FResourceEntry
{
	GENERATED_BODY()

	UPROPERTY() EResourceType Resource = EResourceType::Book;
	UPROPERTY() int32 Count = 0;
};

// The entire branch snapshot — bounded by construction (no dynamic actors, no
// world dump). This is the same shape Ch5 Deploy will serialize.
USTRUCT()
struct FBranchManifest
{
	GENERATED_BODY()

	UPROPERTY() TArray<FResourceEntry> Resources;
	UPROPERTY() TArray<FBranchObjectState> Objects;

	int32 Num() const { return Resources.Num() + Objects.Num(); }
	void Reset() { Resources.Reset(); Objects.Reset(); }
};
