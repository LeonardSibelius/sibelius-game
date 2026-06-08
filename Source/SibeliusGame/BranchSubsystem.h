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
class USaveSubsystem;
class USibeliusSaveGame;

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

	// Ch5 Deploy boundary. Deploy persists Main to disk and reads Main ONLY — a
	// save must never capture an uncommitted branch. CanDeploy() is the gate (true
	// iff at Main); RequestDeploy() is the guarded entry point that refuses while
	// branched (merge or discard must happen first).
	//
	// Phase 1 (SIB-29): when allowed, RequestDeploy gathers the deployed declared
	// set into a USibeliusSaveGame (GUID-keyed deltas) and writes it to DeploySlot
	// through the save chokepoint. The return value is the GUARD result (allowed),
	// unchanged from Phase 4; persistence is the side effect. No load yet (Phase 2).
	bool CanDeploy() const { return GetDepth() == 0; }
	bool RequestDeploy();

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

	// Resolve a branchable by its stable GUID (SIB-29). Null if not registered /
	// the object has gone away. The manifest restore keys off this.
	UObject* ResolveBranchable(const FGuid& Id) const;

	// Test seams (SIB-29): drive + inspect the GUID registry without PIE.
	void RebuildRegistryForTest() { RebuildRegistry(); }
	const TMap<FGuid, TWeakObjectPtr<UObject>>& GetRegistryForTest() const { return BranchablesById; }
	int32 GetRegistryCollisionsForTest() const { return LastRegistryCollisions; }

	// Ch5 Phase 1 save wiring. Production resolves USaveSubsystem off the game
	// instance; SetSaveSubsystem injects one headlessly. SetDeploySlotName lets the
	// smoke test target a sandbox slot it cleans up.
	void SetSaveSubsystem(USaveSubsystem* InSaver) { SaveSubsystemOverride = InSaver; }
	void SetDeploySlotName(const FString& InSlot) { DeploySlotName = InSlot; }
	const FString& GetDeploySlotName() const { return DeploySlotName; }

private:
	UWorld* ResolveWorld() const;
	void RebuildRegistry();                                   // collect IBranchable + inventory from the world
	void RegisterBranchable(UObject* Obj);                    // GUID-index one candidate (assign-once + collision guard)
	FBranchManifest CaptureDeclaredSet() const;
	void RestoreDeclaredSet(const FBranchManifest& Snapshot); // RAW writes only — never replay verbs
	void SuspendPickups();                                    // engage on enter (locked product decision)
	void ResumePickups();                                     // release on resolve (discard/merge)

	USaveSubsystem* ResolveSaveSubsystem() const;             // injected override, else off the game instance
	USibeliusSaveGame* BuildDeploySave() const;               // gather current deltas (vs authored default) into a save

	EBranchState State = EBranchState::Main;

	// Snapshot stack — one frame per nested branch. Top = the current branch's
	// pre-edit capture; discarding pops+restores it, merging pops+keeps live.
	TArray<FBranchManifest> Stack;

	// SIB-29: identity is a stable per-object FGuid, not array position. The registry
	// is a GUID -> object lookup; the manifest keys by GUID and resolves through here.
	TMap<FGuid, TWeakObjectPtr<UObject>> BranchablesById;
	int32 LastRegistryCollisions = 0; // distinct objects that hashed to an already-claimed GUID on the last rebuild
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<UWorld> BranchWorld;

	// Ch5 Phase 1: the deploy save target + an optional injected chokepoint (tests).
	UPROPERTY()
	TObjectPtr<USaveSubsystem> SaveSubsystemOverride;
	FString DeploySlotName = TEXT("DeploySlot");
};
