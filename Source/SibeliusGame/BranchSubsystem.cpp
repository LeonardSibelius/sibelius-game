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

bool UBranchSubsystem::EnterBranch()
{
	if (State != EBranchState::Main)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Branch] EnterBranch ignored: state != Main (no nesting until Phase 3)."));
		return false;
	}

	RebuildRegistry();
	Manifest = CaptureDeclaredSet();
	State = EBranchState::Branched;
	SuspendPickups(); // engage while branched so collecting can't escape the declared set

	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Entered: captured %d object(s) + %d ledger entr(ies)."),
		Manifest.Objects.Num(), Manifest.Resources.Num());
	return true;
}

bool UBranchSubsystem::DiscardBranch()
{
	if (State != EBranchState::Branched)
	{
		return false; // guard: exactly one resolution, only from Branched
	}

	State = EBranchState::Resolving;
	RestoreDeclaredSet(Manifest); // RAW restore — never replay verbs
	Manifest.Reset();
	ResumePickups();
	State = EBranchState::Main;

	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Discarded: declared set restored."));
	return true;
}

bool UBranchSubsystem::MergeBranch()
{
	// One-resolution latch: merge/discard resolve ONLY from Branched, exactly
	// once. After either, the other (and any repeat / UI double-fire) is a no-op
	// until the next EnterBranch flips back to Branched. The transient Resolving
	// state also blocks re-entrant resolves fired mid-resolve.
	if (State != EBranchState::Branched)
	{
		return false;
	}

	State = EBranchState::Resolving;
	// Phase 2 — full single-level merge: KEEP the live (in-branch) world and drop
	// the snapshot, so the current state becomes the new "main" truth (restore
	// nothing). The next EnterBranch re-captures this kept world, so the baseline
	// has advanced. (Phase 3 nesting will fold into the enclosing branch, not Main.)
	Manifest.Reset();
	ResumePickups();
	State = EBranchState::Main;

	UE_LOG(LogSibeliusGame, Display, TEXT("[Branch] Merged: live kept, baseline advanced."));
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
