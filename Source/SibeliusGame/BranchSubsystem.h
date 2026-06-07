// BranchSubsystem.h
//
// SIB-28 — Ch4 "Test-Drive". Owns the branch FSM and captures the bounded
// declared set (FBranchManifest) over IBranchable + the inventory ledger, on a
// snapshot stack. RestoreDeclaredSet writes RAW state, never replays verbs.
//
// Phase 0 seam · Phase 1 Enter/Discard round-trip · Phase 2 Merge + idempotency
// · Phase 3 nesting (stack of frames) + branch-state signal (OnBranchDepthChanged
// drives the desaturate PP + HUD marker on depth >= 1). Phase 4 = Deploy boundary.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BranchTypes.h"
#include "BranchSubsystem.generated.h"

class UWorld;
class UInventoryComponent;

// Fires whenever the branch nesting depth changes (0 = Main). The desaturate
// post-process + HUD branch marker subscribe to this and toggle on depth >= 1.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBranchDepthChanged, int32 /*Depth*/);

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

	// Branch nesting depth: 0 = Main, N = N branches deep (Phase 3).
	int32 GetDepth() const { return Stack.Num(); }

	// Drives the branch-state signal (desaturate PP + HUD marker): on iff branched.
	bool IsBranchSignalActive() const { return Stack.Num() >= 1; }

	// Broadcast on every depth change so PP/HUD listeners can react.
	FOnBranchDepthChanged OnBranchDepthChanged;

	// Enter a branch reality: snapshot the declared set onto the stack.
	// Allowed from Main (depth 1) OR while Branched (Phase 3 nesting, deeper).
	bool EnterBranch();

	// Commit the top branch: keep live, pop its snapshot. Folds into the enclosing
	// branch's live (or becomes Main truth at depth 0). Guarded one-resolution latch.
	bool MergeBranch();

	// Throw the top branch away: restore EXACTLY to its snapshot (RAW writes) —
	// the enclosing branch's live (or Main) at depth 0. Nested discard can't leak.
	bool DiscardBranch();

	// Locked product decision (SIB-28): pickups are suspended while branched so
	// collecting can't mutate state outside the declared set. Guard stubbed now;
	// ABookPickup consults it once wired (later phase).
	UFUNCTION(BlueprintPure, Category = "Branch")
	bool ArePickupsSuspended() const { return IsBranched(); }

	// The TOP frame's snapshot (the current branch's capture) — for the smoke test
	// to inspect. Empty when at Main.
	const FBranchManifest& GetManifest() const
	{
		static const FBranchManifest Empty;
		return Stack.Num() ? Stack.Last() : Empty;
	}

	// Test seam: point capture/restore at a loaded world without PIE. Production
	// resolves GetWorld().
	void SetBranchWorld(UWorld* InWorld) { BranchWorld = InWorld; }

private:
	UWorld* ResolveWorld() const;
	void RebuildRegistry();                                   // collect IBranchable + inventory from the world
	FBranchManifest CaptureDeclaredSet() const;
	void RestoreDeclaredSet(const FBranchManifest& Snapshot); // RAW writes only — never replay verbs
	void SuspendPickups();                                    // engage on enter (locked product decision)
	void ResumePickups();                                     // release on resolve (discard/merge)

	EBranchState State = EBranchState::Main;

	// Snapshot stack — one frame per nested branch. Top = the current branch's
	// pre-edit capture; discarding pops+restores it, merging pops+keeps live.
	TArray<FBranchManifest> Stack;

	// In-session identity = index into this registry (Ch5 promotes to FGuid).
	TArray<TWeakObjectPtr<UObject>> Branchables;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<UWorld> BranchWorld;
};
