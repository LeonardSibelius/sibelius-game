// BranchSubsystem.cpp — SIB-28 Ch4 Test-Drive, Phase 0 (the seam).

#include "BranchSubsystem.h"
#include "Branchable.h"
#include "InventoryComponent.h"
#include "SaveSubsystem.h"
#include "SibeliusSaveGame.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"            // TActorIterator
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h" // CreateSaveGameObject
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
	BranchablesById.Reset();
	Inventory.Reset();
	LastRegistryCollisions = 0;

	UWorld* World = ResolveWorld();
	if (!World)
	{
		return;
	}

	// Collect every IBranchable in the level (actor-level: ABuildSite/AHatchLock;
	// component-level: URefactorableComponent) plus the inventory ledger, indexing
	// each by its stable GUID (SIB-29). Phase 0 builds the registry on enter; a
	// later phase moves to BeginPlay registration.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		RegisterBranchable(Actor);
		TInlineComponentArray<UActorComponent*> Comps(Actor);
		for (UActorComponent* Comp : Comps)
		{
			if (!Comp)
			{
				continue;
			}
			RegisterBranchable(Comp);
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

void UBranchSubsystem::RegisterBranchable(UObject* Obj)
{
	IBranchable* B = Cast<IBranchable>(Obj);
	if (!B)
	{
		return;
	}

	// Assign-once if invalid, then index by the GUID (identity, not array position).
	const FGuid Id = B->GetOrCreateBranchId();
	if (TWeakObjectPtr<UObject>* Existing = BranchablesById.Find(Id))
	{
		if (Existing->Get() != Obj)
		{
			// Two DISTINCT objects share a GUID — a real collision (e.g. an actor
			// duplicated in-editor copied its baked BranchId). Keep the first
			// registrant; count it so the smoke test can assert zero.
			++LastRegistryCollisions;
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Branch] GUID collision on %s: %s already maps to a different object; ignoring the duplicate."),
				*GetNameSafe(Obj), *Id.ToString());
		}
		return; // same object re-seen (or a collision): nothing to add
	}
	BranchablesById.Add(Id, Obj);
}

UObject* UBranchSubsystem::ResolveBranchable(const FGuid& Id) const
{
	const TWeakObjectPtr<UObject>* Found = BranchablesById.Find(Id);
	return Found ? Found->Get() : nullptr;
}

FBranchManifest UBranchSubsystem::CaptureDeclaredSet() const
{
	FBranchManifest Snap;

	// Key each captured object by its stable GUID (the registry map key). Identity
	// is position-independent now, so restore matches on GUID, not array slot.
	for (const TPair<FGuid, TWeakObjectPtr<UObject>>& Pair : BranchablesById)
	{
		UObject* Obj = Pair.Value.Get();
		if (IBranchable* B = (Obj ? Cast<IBranchable>(Obj) : nullptr))
		{
			FBranchObjectState S;
			S.ObjectId = Pair.Key;
			S.State = B->CaptureBranchState();
			Snap.Objects.Add(S);
		}
	}

	if (UInventoryComponent* Inv = Inventory.Get())
	{
		FResourceEntry Book;
		Book.Resource = EResourceType::Book;
		Book.EntryId = Inv->GetOrCreateResourceId(EResourceType::Book);
		Book.Count = Inv->GetCount(EResourceType::Book);
		Snap.Resources.Add(Book);

		FResourceEntry Key;
		Key.Resource = EResourceType::Key;
		Key.EntryId = Inv->GetOrCreateResourceId(EResourceType::Key);
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
		UObject* Obj = ResolveBranchable(S.ObjectId); // GUID -> object (SIB-29), not array slot
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

USaveSubsystem* UBranchSubsystem::ResolveSaveSubsystem() const
{
	if (SaveSubsystemOverride)
	{
		return SaveSubsystemOverride; // injected (headless tests)
	}
	if (UWorld* World = ResolveWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<USaveSubsystem>();
		}
	}
	return nullptr;
}

USibeliusSaveGame* UBranchSubsystem::BuildDeploySave() const
{
	USibeliusSaveGame* Save = Cast<USibeliusSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USibeliusSaveGame::StaticClass()));
	if (!Save)
	{
		return nullptr;
	}
	Save->SaveVersion = USibeliusSaveGame::CurrentSaveVersion; // stamp on write

	// Deltas only: an object contributes an entry iff its current declared state
	// differs from its authored default. An untouched world writes an empty save.
	for (const TPair<FGuid, TWeakObjectPtr<UObject>>& Pair : BranchablesById)
	{
		UObject* Obj = Pair.Value.Get();
		IBranchable* B = (Obj ? Cast<IBranchable>(Obj) : nullptr);
		if (!B)
		{
			continue;
		}
		const uint8 Cur = B->CaptureBranchState();
		if (Cur == B->GetDefaultBranchState())
		{
			continue; // no deployed change -> no delta
		}
		FBranchObjectState S;
		S.ObjectId = Pair.Key;
		S.State = Cur;
		Save->ObjectDeltas.Add(S);
	}

	// Resource entries: persist only non-zero counts (empty == authored default).
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		const EResourceType Tracked[] = { EResourceType::Book, EResourceType::Key };
		for (EResourceType R : Tracked)
		{
			const int32 Count = Inv->GetCount(R);
			if (Count == 0)
			{
				continue;
			}
			FResourceEntry E;
			E.Resource = R;
			E.EntryId = Inv->GetOrCreateResourceId(R);
			E.Count = Count;
			Save->ResourceDeltas.Add(E);
		}
	}

	return Save;
}

bool UBranchSubsystem::RequestDeploy()
{
	// Ch5 Deploy boundary: refuse while any branch is open so a save can't capture
	// uncommitted, ephemeral branch state. The player must merge or discard first.
	if (!CanDeploy())
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Branch] Deploy refused: a branch is open (depth %d) — merge or discard first."), GetDepth());
		return false; // D4: a branched deploy writes NOTHING
	}

	// Allowed (at Main). Rebuild over the live Main world, then persist the deployed
	// declared set (GUID-keyed deltas) through the single save chokepoint.
	RebuildRegistry();
	if (USaveSubsystem* Saver = ResolveSaveSubsystem())
	{
		USibeliusSaveGame* Save = BuildDeploySave();
		const bool bWrote = Save && Saver->CommitSave(Save, DeploySlotName);
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Branch] Deploy at Main: %s slot '%s' (v%d) — %d object + %d resource delta(s)."),
			bWrote ? TEXT("wrote") : TEXT("FAILED to write"),
			*DeploySlotName,
			Save ? Save->SaveVersion : 0,
			Save ? Save->ObjectDeltas.Num() : 0,
			Save ? Save->ResourceDeltas.Num() : 0);
	}
	else
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Branch] Deploy allowed (Main) but no save subsystem resolved — nothing persisted."));
	}

	// Return reflects the GUARD (allowed), unchanged from Phase 4; persistence above
	// is the side effect.
	return true;
}
