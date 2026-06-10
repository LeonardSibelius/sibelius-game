// BranchTypes.h
//
// SIB-28 — Ch4 "Test-Drive", Phase 0 (the seam). The bounded branch snapshot +
// the subsystem state enum. Mirrors docs/ch4-spike-notes.md exactly: a declared
// set of bool-per-object + the inventory ledger, never a world dump.
//
// Identity is a stable per-object FGuid (SIB-29, Ch5 Phase 0) — promoted from the
// in-session registry index. The manifest keys objects AND ledger entries by GUID,
// so it survives a re-register (and, from Ch5 Phase 1, a SaveGame reload).

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
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

// One registered branchable's packed declared state (a single bool today), keyed
// by its stable GUID (SIB-29) rather than a registry array position.
USTRUCT()
struct FBranchObjectState
{
	GENERATED_BODY()

	UPROPERTY() FGuid ObjectId;       // SIB-29: stable identity (was RegistryIndex)
	UPROPERTY() uint8 State = 0;

	// SIB-30 P3: provenance for a RUNTIME-GENERATED object (a Ch6 generated ABuildSite).
	// A placed actor leaves these default and is restored onto its existing actor; a
	// generated object has NO placed actor on reload, so it carries enough to be fully
	// RE-CREATED — catalog id + spawn transform — keyed by the same stable GUID.
	UPROPERTY() bool bGenerated = false;
	UPROPERTY() FName GenerateEntryId;          // catalog EntryId to re-spawn from (valid iff bGenerated)
	UPROPERTY() FTransform GenerateTransform;   // saved spawn transform — applied VERBATIM on re-spawn (P3-3)
};

// The inventory ledger captured whole (2 entries today: Book, Key). The Resource
// enum is the natural key for the count; EntryId carries the entry's stable GUID
// (SIB-29) so the whole manifest is uniformly GUID-keyed for Ch5's save.
USTRUCT()
struct FResourceEntry
{
	GENERATED_BODY()

	UPROPERTY() FGuid EntryId;
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
