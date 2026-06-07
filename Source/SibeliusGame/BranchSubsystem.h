// BranchSubsystem.h
//
// SIB-28 — Ch4 "Test-Drive", Phase 0 (the seam). Owns the branch FSM and
// captures the bounded declared set (FBranchManifest) over IBranchable + the
// inventory ledger.
//
// Phase 0 is a skeleton: EnterBranch captures the declared set; Discard/Merge
// transition the FSM and honor the restore-writes-RAW-state rule from day one
// (RestoreDeclaredSet never replays gameplay verbs). Enter/Discard behaviour is
// finished in Phase 1, Merge + the idempotency guard hardened in Phase 2, and
// nesting added in Phase 3.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BranchTypes.h"
#include "BranchSubsystem.generated.h"

class UWorld;
class UInventoryComponent;

UCLASS()
class SIBELIUSGAME_API UBranchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Production: live only in real gameplay worlds. The headless smoke test
	// drives a NewObject'd instance via SetBranchWorld (mirrors the other
	// commandlets, which NewObject what they test rather than relying on a
	// subsystem existing in the commandlet world).
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	EBranchState GetState() const { return State; }
	bool IsBranched() const { return State == EBranchState::Branched; }

	// Enter a branch reality: snapshot the declared set. Main -> Branched.
	bool EnterBranch();

	// Commit: keep live, advance the baseline, drop the snapshot.
	// Branched -> Resolving -> Main. Re-resolving is a guarded no-op.
	bool MergeBranch();

	// Throw the branch away: restore the declared set EXACTLY (RAW writes).
	// Branched -> Resolving -> Main.
	bool DiscardBranch();

	// Locked product decision (SIB-28): pickups are suspended while branched so
	// collecting can't mutate state outside the declared set. Guard stubbed now;
	// ABookPickup consults it once wired (later phase).
	UFUNCTION(BlueprintPure, Category = "Branch")
	bool ArePickupsSuspended() const { return IsBranched(); }

	// What the live EnterBranch captured — for the smoke test to inspect.
	const FBranchManifest& GetManifest() const { return Manifest; }

	// Test seam: point capture/restore at a loaded world without PIE. Production
	// resolves GetWorld().
	void SetBranchWorld(UWorld* InWorld) { BranchWorld = InWorld; }

private:
	UWorld* ResolveWorld() const;
	void RebuildRegistry();                                   // collect IBranchable + inventory from the world
	FBranchManifest CaptureDeclaredSet() const;
	void RestoreDeclaredSet(const FBranchManifest& Snapshot); // RAW writes only — never replay verbs

	EBranchState State = EBranchState::Main;
	FBranchManifest Manifest;

	// In-session identity = index into this registry (Ch5 promotes to FGuid).
	TArray<TWeakObjectPtr<UObject>> Branchables;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<UWorld> BranchWorld;
};
