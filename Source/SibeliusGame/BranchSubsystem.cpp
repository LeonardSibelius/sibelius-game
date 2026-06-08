// BranchSubsystem.cpp — SIB-28 Ch4 Test-Drive, Phase 0 (the seam).

#include "BranchSubsystem.h"
#include "Branchable.h"
#include "InventoryComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator
#include "GameFramework/Actor.h"
#include "SibeliusGame.h"           // LogSibeliusGame

bool UBranchSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Gameplay worlds only - never the editor preview or the commandlet world.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UWorld* UBranchSubsystem::ResolveWorld() const
{
	if (UWorld* W = BranchWorld.Get())
	{
		return W;
	}
	return GetWorld();
}

void UBranchSubsystem::RebuildRegistry()
{
	Branchables.Reset();
	Inventory.Reset();

	UWorld* World = ResolveWorld();
	if (!World)
	{
		return;
	}

	// Collect every IBranchable in the level (actor-level: ABuildSite/AHatchLock;
	// component-level: URefactorableComponent) plus the inventory ledger. Phase 0
	// builds the registry on enter; a later phase moves to BeginPlay registration.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Cast<IBranchable>(Actor))
		{
			Branchables.Add(Actor);
		}
		TInlineComponentArray<UActorComponent*> Comps(Actor);
		for (UActorComponent* Comp : Comps)
		{
			if (!Comp)
			{
				continue;
			}
			if (Cast<IBranchable>(Comp))
			{
				Branchables.Add(Comp);
			}
			if (!Inventory.IsValid())
			{
				if (UInventoryComponent* Inv = Cast<UInventoryComponent>(Comp))
				{
					Inventory = Inv;
				}
			}
		}
	}
}

FBranchManifest UBranchSubsystem::CaptureDeclaredSet() const
{
	FBranchManifest Snap;

	for (int32 i = 0; i < Branchables.Num(); ++i)
	{
		UObject* Obj = Branchables[i].Get();
		if (IBranchable* B = (Obj ? Cast<IBranchable>(Obj) : nullptr))
		{
			FBranchObjectState S;
			S.RegistryIndex = i;
			S.State = B->CaptureBranchState();
			Snap.Objects.Add(S);
		}
	}

	if (UInventoryComponent* Inv = Inventory.Get())
	{
		FResourceEntry Book;
		Book.Resource = EResourceType::Book;
		Book.Count = Inv->GetCount(EResourceType::Book);
		Snap.Resources.Add(Book);

		FResourceEntry Key;
		Key.Resource = EResourceType::Key;
		Key.Count = Inv->GetCount(EResourceType::Key);
		Snap.Resources.Add(Key);
	}

	return Snap;
}

void UBranchSubsystem::RestoreDeclaredSet(const FBranchManifest& Snapshot)
{
	// RAW state only — never replay Build()/TryUnlock()/Collect(). Each object
	// restores its own exact visuals; the ledger is overwritten whole so the
	// spend/grant couplings restore consistently.
	for (const FBranchObjectState& S : Snapshot.Objects)
	{
		if (!Branchables.IsValidIndex(S.RegistryIndex))
		{
			continue;
		}
		UObject* Obj = Branchables[S.RegistryIndex].Get();
		if (IBranchable* B = (Obj ? Cast<IBranchable>(Obj) : nullptr))
		{
			B->RestoreBranchState(S.State);
		}
	}

	if (UInventoryComponent* Inv = Inventory.Get())
	{
		for (const FResourceEntry& E : Snapshot.Resources)
		{
			Inv->RestoreCount(E.Resource, E.Count);
		}
	}
}

// Max nesting depth — a defensive cap; real "test-drive within test-drive" use
// never approaches it.
static constexpr int32 MAX_BRANCH_DEPTH = 8;

bool UBranchSubsystem::EnterBranch()
{
	// Allowed from Main (depth 1) or while Branched (Phase 3 nesting) — but never
	// mid-resolve.
	if (State == EBranchState::Resolving)
	{
		return false;
	}
	if (Stack.Num() >= MAX_BRANCH_DEPTH)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Branch] EnterBranch ignored: max nesting depth %d reached."), MAX_BRANCH_DEPTH);
		return false;
	}

	if (Stack.Num() == 0)
	{
		RebuildRegistry();   // build the registry once, on the outermost enter
		SuspendPickups();    // engage suspension while any branch is open
	}
	Stack.Push(CaptureDeclaredSet()); // snapshot the CURRENT world (enclosing live)
	State = EBranchState::Branched;

	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Entered: depth %d, captured %d object(s) + %d ledger entr(ies)."),
		Stack.Num(), Stack.Last().Objects.Num(), Stack.Last().Resources.Num());
	OnBranchDepthChanged.Broadcast(Stack.Num());
	return true;
}

bool UBranchSubsystem::DiscardBranch()
{
	if (State != EBranchState::Branched)
	{
		return false; // latch: resolve only from Branched, one frame at a time
	}

	State = EBranchState::Resolving;
	const FBranchManifest Top = Stack.Pop();
	RestoreDeclaredSet(Top); // RAW restore to where this branch began (enclosing live / Main)
	if (Stack.Num() == 0)
	{
		ResumePickups();
		State = EBranchState::Main;
	}
	else
	{
		State = EBranchState::Branched; // still inside the enclosing branch
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Discarded: restored to depth %d."), Stack.Num());
	OnBranchDepthChanged.Broadcast(Stack.Num());
	return true;
}

bool UBranchSubsystem::MergeBranch()
{
	// One-resolution latch: merge/discard resolve ONLY from Branched, one frame
	// per call. After resolving a frame, a repeat / UI double-fire at the same
	// depth is a no-op until the next EnterBranch. Resolving blocks re-entrancy.
	if (State != EBranchState::Branched)
	{
		return false;
	}

	State = EBranchState::Resolving;
	// Keep the live (in-branch) world; drop this frame's snapshot. The kept state
	// folds into the enclosing branch's live (depth > 0) or becomes the new Main
	// truth (depth 0). The enclosing frame's snapshot is untouched, so discarding
	// the enclosing branch still rewinds to where IT began.
	Stack.Pop();
	if (Stack.Num() == 0)
	{
		ResumePickups();
		State = EBranchState::Main;
	}
	else
	{
		State = EBranchState::Branched;
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Merged: live kept, depth %d."), Stack.Num());
	OnBranchDepthChanged.Broadcast(Stack.Num());
	return true;
}

void UBranchSubsystem::SuspendPickups()
{
	// Locked product decision (SIB-28): pickups are suspended while branched.
	// Queryable now via ArePickupsSuspended() (state-derived); ABookPickup will
	// consult it / subscribe here when wired in a later phase.
	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Pickups suspended."));
}

void UBranchSubsystem::ResumePickups()
{
	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Pickups resumed."));
}

bool UBranchSubsystem::RequestDeploy()
{
	// Ch5 Deploy boundary: refuse while any branch is open so a save can't capture
	// uncommitted, ephemeral branch state. The player must merge or discard first.
	if (!CanDeploy())
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Branch] Deploy refused: a branch is open (depth %d) — merge or discard first."), GetDepth());
		return false;
	}

	// Allowed (at Main). Ch5 persists the committed declared set to disk HERE.
	// Phase 4 is guard-only, so this is just the gate for now.
	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Deploy allowed (at Main, depth 0)."));
	return true;
}
