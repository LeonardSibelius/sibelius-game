// BranchSmokeTestCommandlet.cpp
//
// SIB-28 - Ch4 Test-Drive headless smoke test. PHASE 0 (the seam):
//
// [HARD] L_Office_v02 loads
// [HARD] UBranchSubsystem instantiates and starts in EBranchState::Main
// [HARD] EnterBranch(): Main -> Branched, and the manifest captured the declared
//        set (>= 1 IBranchable object from the level) [T1 seam]
// [HARD] pickups are suspended while branched (locked product decision)
// [HARD] EnterBranch() while Branched is rejected (no nesting until Phase 3)
// [HARD] DiscardBranch(): Branched -> Main; a second Discard is a guarded no-op
// [HARD] MergeBranch() from Main is rejected; from Branched: Branched -> Main,
//        and a second Merge is a guarded no-op (idempotency latch)
//
// PHASE 1 (enter/discard for real — full declared-set round-trip):
// [HARD] mutate EVERY declared kind inside a branch (refactor a Refactorable,
//        spend/alter the inventory ledger, Build a BuildSite, unlock a HatchLock)
// [HARD] Discard restores byte-exact: per-entry AND whole-manifest equality vs
//        the pre-branch capture (raw-state restore, never verb replay)
// [HARD] pickups suspended throughout the branch, released on discard
// [HARD] second Discard is a no-op; Merge stays latched (can't half-fire)
//
// PHASE 2 (merge for real + idempotency):
// [HARD] degenerate merge (zero edits) changes nothing
// [HARD] enter -> mutate every kind -> Merge -> live state KEPT (refactored stays
//        refactored, site built, hatch unlocked, ledger keeps spent values)
// [HARD] second Merge no-op; Discard-after-Merge no-op (can't revert a merge)
// [HARD] baseline advanced: re-capture == merged state; discarding a post-merge
//        branch restores to the MERGED baseline, not the original
//
// PHASE 3 (nesting + branch-state signaling):
// [HARD] nesting allowed (enter while Branched pushes a deeper frame); depth
//        tracks 0/1/2 and OnBranchDepthChanged fires each change
// [HARD] branch signal (desaturate PP + HUD) on iff depth >= 1
// [HARD] nested discard restores to the OUTER branch exactly (no leak in);
//        outer discard restores to Main exactly (no leak out)
// [HARD] re-enter after unwind starts clean from Main (no residue)
// [HARD] nested merge folds into the enclosing branch; a later outer discard
//        drops the merged-inner changes too (outer discard wins)
//
// PHASE 4 (Ch5 Deploy boundary guard — guard-only, no persistence):
// [HARD] Deploy allowed at Main (depth 0); CanDeploy() true
// [HARD] Deploy refused while branched (depth 1 and nested depth 2)
// [HARD] partial resolve isn't enough — refused until depth 0; allowed again once
//        merge/discard returns to Main (merge precedes deploy)
//
// PHASE 0 (Ch5) — GUID identity seam (SIB-29, no SaveGame yet):
// [HARD] every registered IBranchable has a valid GUID; no collisions
// [HARD] GetOrCreateBranchId is assign-once (stable, never regenerated)
// [HARD] a re-register (clear + rebuild) keeps identity: each GUID resolves to the
//        SAME logical object, registry size holds, object ids unchanged
// [HARD] inventory ledger entries carry valid, distinct, stable GUIDs
//
// PHASE 1 (Ch5) — SaveGame write (SIB-29, no load/re-apply yet):
// [HARD] RequestDeploy at Main writes a USibeliusSaveGame to the slot via the save
//        chokepoint; the written save's SaveVersion is stamped
// [HARD] the save holds a GUID-keyed delta for each CHANGED branchable and NONE for
//        the unchanged (default) ones; non-zero resource entries only
// [HARD] a branched RequestDeploy writes NOTHING (D4 regression)
// [HARD] runs against a sandbox slot it deletes afterward (no artifact)
//
// CP3 lesson #6: NAMED namespace to avoid unity-build redefinition collisions
// with the sibling commandlets.

#include "BranchSmokeTestCommandlet.h"
#include "BranchSubsystem.h"
#include "BranchTypes.h"

#include "RefactorableComponent.h"
#include "InventoryComponent.h"
#include "BuildSite.h"
#include "HatchLock.h"
#include "CompileTypes.h"           // EResourceType
#include "SibeliusSaveIO.h"         // Ch5 Phase 1: GameInstance-free save I/O
#include "SibeliusSaveGame.h"       // Ch5 Phase 1: persisted manifest

#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator
#include "GameFramework/Actor.h"
#include "UObject/Package.h"        // GetTransientPackage

#if WITH_EDITOR
#include "FileHelpers.h"            // UEditorLoadingAndSavingUtils
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBranchSmoke, Log, All);

namespace BranchSmokeTestNS
{
	const FString DefaultMapPackage = TEXT("/Game/L_Office_v02");

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogBranchSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogBranchSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	FString ParseMapArg(const FString& Params)
	{
		TArray<FString> Tokens, Switches;
		TMap<FString, FString> SwitchPairs;
		UCommandlet::ParseCommandLine(*Params, Tokens, Switches, SwitchPairs);
		if (const FString* Found = SwitchPairs.Find(TEXT("map")))
		{
			return *Found;
		}
		return DefaultMapPackage;
	}

	// Manifest equality keyed by stable identity (SIB-29): resources by EResourceType,
	// objects by GUID — order-independent, so it no longer leans on the registry
	// emitting captures in a fixed array order.
	bool ManifestsEqual(const FBranchManifest& A, const FBranchManifest& B)
	{
		if (A.Resources.Num() != B.Resources.Num() || A.Objects.Num() != B.Objects.Num())
		{
			return false;
		}
		for (const FResourceEntry& EA : A.Resources)
		{
			const FResourceEntry* EB = B.Resources.FindByPredicate(
				[&EA](const FResourceEntry& X) { return X.Resource == EA.Resource; });
			if (!EB || EB->Count != EA.Count)
			{
				return false;
			}
		}
		for (const FBranchObjectState& OA : A.Objects)
		{
			const FBranchObjectState* OB = B.Objects.FindByPredicate(
				[&OA](const FBranchObjectState& X) { return X.ObjectId == OA.ObjectId; });
			if (!OB || OB->State != OA.State)
			{
				return false;
			}
		}
		return true;
	}
}

UBranchSmokeTestCommandlet::UBranchSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UBranchSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogBranchSmoke, Error, TEXT("BranchSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
	return 1;
#else
	using namespace BranchSmokeTestNS;

	const FString MapPackage = ParseMapArg(Params);
	UE_LOG(LogBranchSmoke, Display, TEXT("=== SIB-28 Branch smoke test (Phase 0): %s ==="), *MapPackage);

	FResult R;

	// [HARD] Load the world.
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPackage);
	R.Check(World != nullptr, FString::Printf(TEXT("Map loads (%s)"), *MapPackage));
	if (!World)
	{
		UE_LOG(LogBranchSmoke, Error, TEXT("=== BRANCH SMOKE TEST FAILED: could not load map. ==="));
		return 1;
	}

	// [HARD] Subsystem instantiates. The production subsystem doesn't live in the
	// commandlet world (DoesSupportWorldType excludes it, like the siblings), so
	// drive a NewObject'd instance pointed at the loaded world.
	UBranchSubsystem* Branch = NewObject<UBranchSubsystem>(GetTransientPackage(), TEXT("SmokeBranch"));
	R.Check(Branch != nullptr, TEXT("UBranchSubsystem instantiates"));
	if (!Branch)
	{
		UE_LOG(LogBranchSmoke, Error, TEXT("=== BRANCH SMOKE TEST FAILED: no subsystem. ==="));
		return 1;
	}
	Branch->SetBranchWorld(World);

	// Ch5 Phase 1 routes Deploy writes through the GameInstance-free save helper.
	// Point Deploy at a sandbox slot for the WHOLE run so Phase 4's allowed deploys
	// don't litter the real "DeploySlot"; the Ch5 Phase-1 block asserts on it and
	// deletes it at the end.
	const FString DeploySandboxSlot = TEXT("SmokeDeploySlot_Temp");
	Branch->SetDeploySlotName(DeploySandboxSlot);
	FSibeliusSaveIO::Delete(DeploySandboxSlot); // clear any stale artifact up front

	// [HARD] Starts in Main.
	R.Check(Branch->GetState() == EBranchState::Main, TEXT("Initial state is Main"));
	R.Check(!Branch->ArePickupsSuspended(), TEXT("Pickups not suspended in Main"));

	// [HARD] Enter: Main -> Branched, declared set captured.
	const bool bEntered = Branch->EnterBranch();
	R.Check(bEntered, TEXT("EnterBranch() succeeds from Main"));
	R.Check(Branch->GetState() == EBranchState::Branched, TEXT("State is Branched after enter"));
	const int32 Captured = Branch->GetManifest().Objects.Num();
	UE_LOG(LogBranchSmoke, Display, TEXT("  (manifest: %d object(s), %d ledger entr(ies))"),
		Captured, Branch->GetManifest().Resources.Num());
	R.Check(Captured >= 1, TEXT("EnterBranch captured the declared set (>= 1 IBranchable from the level) [T1]"));

	// [HARD] Pickups suspended while branched.
	R.Check(Branch->ArePickupsSuspended(), TEXT("Pickups suspended while branched (product decision)"));

	// (Nesting is enabled in Phase 3 and exercised in its own block below.)

	// [HARD] Discard: Branched -> Main; second discard is a guarded no-op.
	R.Check(Branch->DiscardBranch(), TEXT("DiscardBranch() succeeds from Branched"));
	R.Check(Branch->GetState() == EBranchState::Main, TEXT("State is Main after discard"));
	R.Check(!Branch->DiscardBranch(), TEXT("Second DiscardBranch() is a guarded no-op"));

	// [HARD] Merge: rejected from Main; works from Branched; idempotent.
	R.Check(!Branch->MergeBranch(), TEXT("MergeBranch() rejected from Main (guard)"));
	R.Check(Branch->EnterBranch(), TEXT("EnterBranch() again from Main"));
	R.Check(Branch->MergeBranch(), TEXT("MergeBranch() succeeds from Branched"));
	R.Check(Branch->GetState() == EBranchState::Main, TEXT("State is Main after merge"));
	R.Check(!Branch->MergeBranch(), TEXT("Second MergeBranch() is a guarded no-op (idempotency latch)"));

	// =========================================================================
	//  PHASE 1 — Enter/Discard for real: mutate every declared kind in a branch,
	//  Discard, and assert byte-exact round-trip against the pre-branch capture.
	// =========================================================================
	UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 1: declared-set round-trip ---"));

	// A level refactorable is functional headless (its mesh resolves on demand);
	// the other kinds + a seeded inventory we spawn, so the test is a controlled,
	// deterministic mini-scene independent of what the map happens to contain.
	URefactorableComponent* Refac = nullptr;
	for (TActorIterator<AActor> It(World); It && !Refac; ++It)
	{
		Refac = It->FindComponentByClass<URefactorableComponent>();
	}

	AActor* InvActor = World->SpawnActor<AActor>();
	UInventoryComponent* Inv = InvActor ? NewObject<UInventoryComponent>(InvActor) : nullptr;
	if (Inv)
	{
		Inv->RegisterComponent();
	}
	ABuildSite* Site = World->SpawnActor<ABuildSite>();
	AHatchLock* Hatch = World->SpawnActor<AHatchLock>();

	R.Check(Refac != nullptr, TEXT("P1: found a level URefactorableComponent to refactor"));
	R.Check(Inv != nullptr,   TEXT("P1: spawned a seeded UInventoryComponent"));
	R.Check(Site != nullptr,  TEXT("P1: spawned an ABuildSite"));
	R.Check(Hatch != nullptr, TEXT("P1: spawned an AHatchLock"));

	if (Refac && Inv && Site && Hatch)
	{
		// Seed known pre-branch state (site default: unbuilt, Cost 8 Book; hatch: locked).
		Inv->Add(EResourceType::Book, 20);
		Inv->Add(EResourceType::Key, 3);
		const bool RefacWas = Refac->IsRefactored();

		// Enter -> capture the full declared set.
		R.Check(Branch->EnterBranch(), TEXT("P1: EnterBranch captures the declared set"));
		const FBranchManifest Before = Branch->GetManifest();
		R.Check(Before.Resources.Num() == 2, TEXT("P1: ledger captured whole (Book + Key)"));
		R.Check(Branch->ArePickupsSuspended(), TEXT("P1: pickups suspended on enter"));

		// Mutate EVERY kind of declared state inside the branch, via real verbs.
		Refac->ToggleRefactor();
		Inv->Spend(EResourceType::Book, 5);
		Inv->Add(EResourceType::Key, 4);
		const bool bBuilt = Site->Build(Inv);        // spends 8 Book
		const bool bUnlocked = Hatch->TryUnlock(Inv); // spends 1 Key

		R.Check(Refac->IsRefactored() != RefacWas, TEXT("P1: refactor mutated the Refactorable in-branch"));
		R.Check(bBuilt && Site->IsBuilt(),         TEXT("P1: Build() built the site in-branch (spent Books)"));
		R.Check(bUnlocked && !Hatch->IsLocked(),   TEXT("P1: TryUnlock() unlocked the hatch in-branch (spent Key)"));
		R.Check(Inv->GetCount(EResourceType::Book) != 20 || Inv->GetCount(EResourceType::Key) != 3,
			TEXT("P1: inventory ledger changed in-branch"));
		R.Check(Branch->ArePickupsSuspended(), TEXT("P1: pickups still suspended after in-branch edits"));

		// Discard -> restore EXACTLY (raw writes, no verb replay), release suspension.
		R.Check(Branch->DiscardBranch(), TEXT("P1: DiscardBranch resolves Branched -> Main"));
		R.Check(Branch->GetState() == EBranchState::Main, TEXT("P1: state Main after discard"));
		R.Check(!Branch->ArePickupsSuspended(), TEXT("P1: pickups released after discard"));

		// Byte-exact round-trip, per declared entry.
		R.Check(Refac->IsRefactored() == RefacWas, TEXT("P1 round-trip: Refactorable restored exactly"));
		R.Check(Site->IsBuilt() == false,          TEXT("P1 round-trip: BuildSite restored to unbuilt"));
		R.Check(Hatch->IsLocked() == true,         TEXT("P1 round-trip: HatchLock restored to locked"));
		R.Check(Inv->GetCount(EResourceType::Book) == 20 && Inv->GetCount(EResourceType::Key) == 3,
			TEXT("P1 round-trip: inventory ledger restored exactly (20 Book / 3 Key)"));

		// Aggregate: re-capture and compare the WHOLE manifest to the pre-branch one.
		R.Check(Branch->EnterBranch(), TEXT("P1: re-enter to re-capture post-discard state"));
		R.Check(ManifestsEqual(Before, Branch->GetManifest()),
			TEXT("P1 round-trip: full manifest byte-exact vs pre-branch capture (every entry)"));
		R.Check(Branch->DiscardBranch(), TEXT("P1: discard the verification branch"));

		// Discard is a no-op from Main; Merge is latched (can't half-fire).
		R.Check(!Branch->DiscardBranch(), TEXT("P1: second DiscardBranch is a no-op"));
		R.Check(!Branch->MergeBranch(),   TEXT("P1: MergeBranch latched — rejected outside Branched (can't half-fire)"));

		// =====================================================================
		//  PHASE 2 — Merge for real + idempotency. Reuses the Phase-1 scene; the
		//  world is back at its original state here (Phase 1 discarded its branch:
		//  Refactorable not refactored, site unbuilt, hatch locked, ledger 20/3).
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 2: merge keeps live + idempotency ---"));

		// Degenerate case: merge with ZERO in-branch edits changes nothing.
		R.Check(Branch->EnterBranch(), TEXT("P2: enter (degenerate, no edits)"));
		const FBranchManifest CleanCapture = Branch->GetManifest();
		R.Check(Branch->MergeBranch(), TEXT("P2: merge with zero in-branch edits succeeds"));
		R.Check(Branch->GetState() == EBranchState::Main, TEXT("P2: Main after degenerate merge"));
		R.Check(Branch->EnterBranch(), TEXT("P2: re-enter after degenerate merge"));
		R.Check(ManifestsEqual(CleanCapture, Branch->GetManifest()), TEXT("P2: degenerate merge changed nothing"));
		R.Check(Branch->DiscardBranch(), TEXT("P2: discard the degenerate check branch"));

		// Pre-merge originals (to contrast with the merged baseline).
		const bool  RefacOrig = Refac->IsRefactored();              // false
		const int32 BookOrig  = Inv->GetCount(EResourceType::Book); // 20
		const int32 KeyOrig   = Inv->GetCount(EResourceType::Key);  // 3

		// Enter -> mutate EVERY kind via real verbs -> Merge -> assert live KEPT.
		R.Check(Branch->EnterBranch(), TEXT("P2: enter the merge round"));
		Refac->ToggleRefactor();
		Inv->Spend(EResourceType::Book, 6);
		Inv->Add(EResourceType::Key, 2);
		const bool bBuilt2 = Site->Build(Inv);         // spends 8 Book
		const bool bUnlocked2 = Hatch->TryUnlock(Inv); // spends 1 Key
		R.Check(bBuilt2 && bUnlocked2, TEXT("P2: in-branch verbs applied (build + unlock)"));

		// Snapshot the LIVE (about-to-merge) state for the kept-state asserts.
		const bool  RefacLive = Refac->IsRefactored();              // true
		const bool  SiteLive  = Site->IsBuilt();                    // true
		const int32 BookLive  = Inv->GetCount(EResourceType::Book);
		const int32 KeyLive   = Inv->GetCount(EResourceType::Key);

		R.Check(Branch->MergeBranch(), TEXT("P2: MergeBranch keeps live + advances baseline"));
		R.Check(Branch->GetState() == EBranchState::Main, TEXT("P2: Main after merge"));
		R.Check(!Branch->ArePickupsSuspended(), TEXT("P2: pickups resumed after merge"));

		// Live state KEPT (NOT reverted): every declared kind holds its in-branch value.
		R.Check(Refac->IsRefactored() == RefacLive && RefacLive != RefacOrig, TEXT("P2 merged: Refactorable stays refactored"));
		R.Check(Site->IsBuilt() == SiteLive && SiteLive,                       TEXT("P2 merged: site stays built"));
		R.Check(!Hatch->IsLocked(),                                            TEXT("P2 merged: hatch stays unlocked"));
		R.Check(Inv->GetCount(EResourceType::Book) == BookLive
			&& Inv->GetCount(EResourceType::Key) == KeyLive
			&& (BookLive != BookOrig || KeyLive != KeyOrig),                   TEXT("P2 merged: ledger keeps spent values"));

		// Idempotency: second Merge no-op; Discard-after-Merge no-op (can't revert).
		R.Check(!Branch->MergeBranch(),   TEXT("P2: second MergeBranch is a no-op"));
		R.Check(!Branch->DiscardBranch(), TEXT("P2: Discard after Merge is a no-op (can't revert a merged branch)"));
		R.Check(Site->IsBuilt() && !Hatch->IsLocked(), TEXT("P2: merged state intact after the no-op resolves"));

		// Baseline advanced: a fresh branch off the merged world captures the MERGED
		// state, and discarding it restores to the MERGED baseline — NOT the original.
		R.Check(Branch->EnterBranch(), TEXT("P2: re-enter off the merged baseline"));
		int32 CapturedBook = -1;
		for (const FResourceEntry& E : Branch->GetManifest().Resources)
		{
			if (E.Resource == EResourceType::Book) { CapturedBook = E.Count; }
		}
		R.Check(CapturedBook == BookLive, TEXT("P2: re-capture == merged ledger (baseline advanced, not original)"));

		Refac->ToggleRefactor();                  // mutate again inside this branch
		Inv->Add(EResourceType::Book, 50);
		R.Check(Branch->DiscardBranch(), TEXT("P2: discard the post-merge branch"));
		R.Check(Inv->GetCount(EResourceType::Book) == BookLive && BookLive != BookOrig,
			TEXT("P2: discard restores to the MERGED baseline ledger, not the original"));
		R.Check(Refac->IsRefactored() == RefacLive, TEXT("P2: discard restores Refactorable to the merged baseline"));
		R.Check(Site->IsBuilt() && !Hatch->IsLocked(), TEXT("P2: merged build/unlock survive the post-merge discard"));

		// Clean single-level resolve (nesting is Phase 3's block, below).
		R.Check(Branch->EnterBranch(),  TEXT("P2: enter once more"));
		R.Check(Branch->MergeBranch(),  TEXT("P2: clean up the final branch via merge"));

		// =====================================================================
		//  PHASE 3 — nesting + branch-state signaling. World here is the Phase-2
		//  MERGED baseline (refactored / built / unlocked / ledger 6 Book, 4 Key).
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 3: nesting + signaling ---"));

		// Signal driver: PP desaturate + HUD marker subscribe to OnBranchDepthChanged.
		int32 LastSignalDepth = -1;
		FDelegateHandle SignalH = Branch->OnBranchDepthChanged.AddLambda(
			[&LastSignalDepth](int32 D) { LastSignalDepth = D; });

		R.Check(Branch->GetDepth() == 0, TEXT("P3: depth 0 at Main"));
		R.Check(!Branch->IsBranchSignalActive(), TEXT("P3: branch signal OFF at Main (no desaturate/HUD)"));

		// Level-0 baseline = whatever the world is now.
		const bool  L0Refac = Refac->IsRefactored();
		const int32 L0Book  = Inv->GetCount(EResourceType::Book);

		// Enter OUTER (depth 1) -> mutate.
		R.Check(Branch->EnterBranch(), TEXT("P3: enter outer (depth 1)"));
		R.Check(Branch->GetDepth() == 1 && LastSignalDepth == 1, TEXT("P3: depth 1, signal fired (1)"));
		R.Check(Branch->IsBranchSignalActive(), TEXT("P3: branch signal ON while branched"));
		Refac->ToggleRefactor();
		Inv->Spend(EResourceType::Book, 2);
		const bool  OuterRefac = Refac->IsRefactored();
		const int32 OuterBook  = Inv->GetCount(EResourceType::Book);

		// Enter NESTED (depth 2) — nesting now allowed -> mutate.
		R.Check(Branch->EnterBranch(), TEXT("P3: enter nested (depth 2) — nesting allowed"));
		R.Check(Branch->GetDepth() == 2 && LastSignalDepth == 2, TEXT("P3: depth 2, signal fired (2)"));
		Refac->ToggleRefactor();
		Inv->Add(EResourceType::Book, 100);

		// Discard NESTED -> restores to the OUTER branch's live (no leak in).
		R.Check(Branch->DiscardBranch(), TEXT("P3: discard nested"));
		R.Check(Branch->GetDepth() == 1 && LastSignalDepth == 1, TEXT("P3: back to depth 1, signal (1)"));
		R.Check(Branch->IsBranchSignalActive(), TEXT("P3: signal still ON at depth 1"));
		R.Check(Refac->IsRefactored() == OuterRefac && Inv->GetCount(EResourceType::Book) == OuterBook,
			TEXT("P3: nested discard restored to the OUTER branch exactly (no leak in)"));

		// Discard OUTER -> restores to Main exactly (no leak out).
		R.Check(Branch->DiscardBranch(), TEXT("P3: discard outer"));
		R.Check(Branch->GetDepth() == 0 && LastSignalDepth == 0, TEXT("P3: depth 0, signal fired (0)"));
		R.Check(!Branch->IsBranchSignalActive(), TEXT("P3: branch signal OFF back at Main"));
		R.Check(!Branch->ArePickupsSuspended(), TEXT("P3: pickups released at depth 0"));
		R.Check(Refac->IsRefactored() == L0Refac && Inv->GetCount(EResourceType::Book) == L0Book,
			TEXT("P3: outer discard restored to Main exactly (no leak out)"));

		// Re-enter clean: the new capture starts from Main, no residue.
		R.Check(Branch->EnterBranch(), TEXT("P3: re-enter after full unwind"));
		int32 ReenterBook = -1;
		for (const FResourceEntry& E : Branch->GetManifest().Resources)
		{
			if (E.Resource == EResourceType::Book) { ReenterBook = E.Count; }
		}
		R.Check(ReenterBook == L0Book, TEXT("P3: re-entered branch starts clean from Main (no residue)"));
		R.Check(Branch->DiscardBranch(), TEXT("P3: discard the clean re-enter"));

		// Nested MERGE folds into the enclosing branch; outer discard then drops it.
		R.Check(Branch->EnterBranch(), TEXT("P3: enter outer again (depth 1)"));
		Inv->Spend(EResourceType::Book, 1);
		R.Check(Branch->EnterBranch(), TEXT("P3: enter nested again (depth 2)"));
		Inv->Spend(EResourceType::Book, 1);
		const int32 NestedBook = Inv->GetCount(EResourceType::Book);
		R.Check(Branch->MergeBranch(), TEXT("P3: merge nested -> folds into the outer branch"));
		R.Check(Branch->GetDepth() == 1, TEXT("P3: depth 1 after nested merge (still inside outer)"));
		R.Check(Inv->GetCount(EResourceType::Book) == NestedBook, TEXT("P3: nested merge KEEPS its live in the outer branch"));
		R.Check(Branch->DiscardBranch(), TEXT("P3: discard outer (drops the nested-merged changes too)"));
		R.Check(Branch->GetDepth() == 0, TEXT("P3: depth 0"));
		R.Check(Inv->GetCount(EResourceType::Book) == L0Book,
			TEXT("P3: outer discard wins — nested-merged changes dropped to the Main baseline"));

		Branch->OnBranchDepthChanged.Remove(SignalH);

		// =====================================================================
		//  PHASE 4 — Ch5 Deploy boundary guard (guard-only, no persistence). A
		//  save reads Main ONLY and must never capture an uncommitted branch.
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 4: deploy boundary guard ---"));

		// At Main: Deploy is allowed.
		R.Check(Branch->GetDepth() == 0, TEXT("P4: at Main before the deploy checks"));
		R.Check(Branch->CanDeploy(), TEXT("P4: CanDeploy() true at Main"));
		R.Check(Branch->RequestDeploy(), TEXT("P4: Deploy allowed at depth 0 (Main)"));

		// While branched: Deploy is refused.
		R.Check(Branch->EnterBranch(), TEXT("P4: enter a branch (depth 1)"));
		R.Check(!Branch->CanDeploy(), TEXT("P4: CanDeploy() false while branched"));
		R.Check(!Branch->RequestDeploy(), TEXT("P4: Deploy refused at depth 1 (uncommitted branch)"));

		// Refused at deeper nesting too.
		R.Check(Branch->EnterBranch(), TEXT("P4: nest (depth 2)"));
		R.Check(!Branch->RequestDeploy(), TEXT("P4: Deploy refused at depth 2"));

		// Resolving one level is not enough — still refused at depth 1.
		R.Check(Branch->DiscardBranch(), TEXT("P4: discard nested -> depth 1"));
		R.Check(!Branch->RequestDeploy(), TEXT("P4: Deploy still refused at depth 1"));

		// Merge/discard must take it all the way to Main before Deploy is allowed.
		R.Check(Branch->MergeBranch(), TEXT("P4: merge outer -> back to Main"));
		R.Check(Branch->GetDepth() == 0, TEXT("P4: at Main after fully resolving"));
		R.Check(Branch->CanDeploy() && Branch->RequestDeploy(),
			TEXT("P4: Deploy allowed again once Main (merge/discard precedes deploy)"));

		// =====================================================================
		//  PHASE 0 (Ch5) — GUID identity seam (SIB-29). Identity is a stable
		//  per-object FGuid, not array position: every registered IBranchable has
		//  a valid, unique GUID; the registry survives a clear+rebuild with each
		//  GUID resolving to the SAME logical object; no GUID collisions. Seam
		//  only — no SaveGame yet (that's Ch5 Phase 1).
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 0 (Ch5): GUID identity seam ---"));

		// Build (or rebuild) the GUID registry over the test mini-scene.
		Branch->RebuildRegistryForTest();
		const TMap<FGuid, TWeakObjectPtr<UObject>>& Reg1 = Branch->GetRegistryForTest();

		// At least our spawned Site + Hatch + a level Refactorable are indexed.
		R.Check(Reg1.Num() >= 3,
			TEXT("C5-P0: registry indexes >= 3 IBranchables (Refactorable + BuildSite + HatchLock)"));
		R.Check(Branch->GetRegistryCollisionsForTest() == 0,
			TEXT("C5-P0: no GUID collisions on first registration"));

		// Every registered GUID is valid; snapshot id -> object for the re-register
		// resolve check. (Map keys are inherently unique; the collision counter above
		// is the real two-objects-one-GUID guard.)
		TMap<FGuid, UObject*> Expected;
		bool bAllValid = true;
		for (const TPair<FGuid, TWeakObjectPtr<UObject>>& P : Reg1)
		{
			if (!P.Key.IsValid())
			{
				bAllValid = false;
			}
			Expected.Add(P.Key, P.Value.Get());
		}
		R.Check(bAllValid, TEXT("C5-P0: every registered IBranchable has a VALID GUID"));

		// Each implementer exposes a valid id via the interface, and distinct objects
		// have distinct ids.
		R.Check(Site->GetBranchId().IsValid() && Hatch->GetBranchId().IsValid() && Refac->GetBranchId().IsValid(),
			TEXT("C5-P0: BuildSite/HatchLock/Refactorable each carry a valid persisted GUID"));
		R.Check(Site->GetBranchId() != Hatch->GetBranchId() && Site->GetBranchId() != Refac->GetBranchId(),
			TEXT("C5-P0: distinct objects have distinct GUIDs"));

		// Assign-once: GetOrCreateBranchId is stable across calls, never regenerated.
		const FGuid SiteId = Site->GetOrCreateBranchId();
		R.Check(SiteId == Site->GetOrCreateBranchId() && SiteId == Site->GetBranchId(),
			TEXT("C5-P0: GetOrCreateBranchId is assign-once (stable, never regenerated)"));

		// Re-register: clear + rebuild the registry; identity must survive — each
		// GUID resolves to the SAME logical object (not a shifted array slot).
		Branch->RebuildRegistryForTest();
		const TMap<FGuid, TWeakObjectPtr<UObject>>& Reg2 = Branch->GetRegistryForTest();
		R.Check(Branch->GetRegistryCollisionsForTest() == 0, TEXT("C5-P0: no GUID collisions on re-register"));
		R.Check(Reg2.Num() == Expected.Num(), TEXT("C5-P0: re-register yields the same registry size"));

		bool bResolveStable = true;
		for (const TPair<FGuid, UObject*>& E : Expected)
		{
			if (Branch->ResolveBranchable(E.Key) != E.Value)
			{
				bResolveStable = false;
				break;
			}
		}
		R.Check(bResolveStable,
			TEXT("C5-P0: after clear+rebuild every GUID resolves to the SAME object (identity, not index)"));
		R.Check(Site->GetBranchId() == SiteId,
			TEXT("C5-P0: re-register did NOT regenerate the object's GUID"));

		// Inventory tracked entries also carry stable, distinct, assign-once GUIDs
		// (so Ch5's save payload is uniformly GUID-keyed).
		const FGuid BookId = Inv->GetOrCreateResourceId(EResourceType::Book);
		const FGuid KeyId  = Inv->GetOrCreateResourceId(EResourceType::Key);
		R.Check(BookId.IsValid() && KeyId.IsValid() && BookId != KeyId,
			TEXT("C5-P0: inventory entries carry valid, distinct GUIDs"));
		R.Check(Inv->GetOrCreateResourceId(EResourceType::Book) == BookId
			&& Inv->GetResourceId(EResourceType::Book) == BookId,
			TEXT("C5-P0: inventory entry GUID is assign-once (stable)"));

		// =====================================================================
		//  PHASE 1 (Ch5) — SaveGame write (SIB-29). RequestDeploy at Main writes
		//  the deployed manifest (GUID-keyed deltas vs authored default) to a slot
		//  through the save chokepoint; SaveVersion is stamped; unchanged objects
		//  get NO delta; a branched RequestDeploy writes NOTHING (D4). No load /
		//  re-apply yet (Phase 2). Uses a sandbox slot, deleted at the end.
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 1 (Ch5): SaveGame write ---"));

		// Deploy routes through FSibeliusSaveIO directly (no GameInstance) — the
		// test exercises that same helper path. Start from a clean sandbox slot
		// (Phase 4's allowed deploys wrote it; clear it so the assert is meaningful).
		FSibeliusSaveIO::Delete(DeploySandboxSlot);
		R.Check(!FSibeliusSaveIO::Has(DeploySandboxSlot), TEXT("C5-P1: sandbox slot empty before deploy"));

		// Force a KNOWN deployed state at Main via RAW restores (no verbs / ledger
		// coupling): Refactorable refactored + BuildSite built = CHANGED; HatchLock
		// locked = authored default = UNCHANGED (must produce no delta).
		Refac->RestoreBranchState(1);   // refactored (delta)
		Site->RestoreBranchState(1);    // built (delta)
		Hatch->RestoreBranchState(1);   // locked == default (NO delta)
		Inv->RestoreCount(EResourceType::Book, 7); // non-zero (delta)
		Inv->RestoreCount(EResourceType::Key, 0);  // zero == default (NO delta)

		const FGuid RefacId = Refac->GetBranchId();
		const FGuid SiteId2 = Site->GetBranchId();
		const FGuid HatchId = Hatch->GetBranchId();

		// Deploy at Main: allowed + writes the save through the helper.
		R.Check(Branch->RequestDeploy(), TEXT("C5-P1: RequestDeploy allowed at Main"));
		R.Check(FSibeliusSaveIO::Has(DeploySandboxSlot), TEXT("C5-P1: deploy wrote a save to the slot"));

		// Read the WRITTEN save back (I/O-only; no world re-apply — that's Phase 2).
		USibeliusSaveGame* Loaded = Cast<USibeliusSaveGame>(FSibeliusSaveIO::Load(DeploySandboxSlot));
		R.Check(Loaded != nullptr, TEXT("C5-P1: written save loads back as USibeliusSaveGame"));
		if (Loaded)
		{
			R.Check(Loaded->SaveVersion == USibeliusSaveGame::CurrentSaveVersion,
				TEXT("C5-P1: written save's SaveVersion is stamped (== CurrentSaveVersion)"));

			auto FindObj = [&Loaded](const FGuid& Id) -> const FBranchObjectState*
			{
				return Loaded->ObjectDeltas.FindByPredicate(
					[&Id](const FBranchObjectState& X) { return X.ObjectId == Id; });
			};
			auto FindRes = [&Loaded](EResourceType R) -> const FResourceEntry*
			{
				return Loaded->ResourceDeltas.FindByPredicate(
					[R](const FResourceEntry& X) { return X.Resource == R; });
			};

			const FBranchObjectState* RefacDelta = FindObj(RefacId);
			const FBranchObjectState* SiteDelta  = FindObj(SiteId2);
			R.Check(RefacDelta && RefacDelta->State == 1,
				TEXT("C5-P1: GUID-keyed delta written for the refactored Refactorable"));
			R.Check(SiteDelta && SiteDelta->State == 1,
				TEXT("C5-P1: GUID-keyed delta written for the built BuildSite"));
			R.Check(FindObj(HatchId) == nullptr,
				TEXT("C5-P1: NO delta for the unchanged (default-locked) HatchLock"));

			const FResourceEntry* BookDelta = FindRes(EResourceType::Book);
			R.Check(BookDelta && BookDelta->Count == 7 && BookDelta->EntryId == BookId,
				TEXT("C5-P1: resource delta written for the non-zero Book ledger entry"));
			R.Check(FindRes(EResourceType::Key) == nullptr,
				TEXT("C5-P1: NO resource delta for the zero-count Key entry"));
		}

		// D4 regression: a branched RequestDeploy must write NOTHING.
		R.Check(FSibeliusSaveIO::Delete(DeploySandboxSlot), TEXT("C5-P1: cleared the slot before the D4 check"));
		R.Check(Branch->EnterBranch(), TEXT("C5-P1: enter a branch for the D4 check"));
		R.Check(!Branch->RequestDeploy(), TEXT("C5-P1: RequestDeploy refused while branched (guard)"));
		R.Check(!FSibeliusSaveIO::Has(DeploySandboxSlot), TEXT("C5-P1: branched deploy wrote NOTHING to the slot (D4)"));
		R.Check(Branch->DiscardBranch(), TEXT("C5-P1: discard the D4 branch"));

		// Clean up the sandbox slot so the test leaves no artifact behind.
		FSibeliusSaveIO::Delete(DeploySandboxSlot);
		R.Check(!FSibeliusSaveIO::Has(DeploySandboxSlot), TEXT("C5-P1: sandbox slot cleaned up after the test"));
	}

	if (R.Failures == 0)
	{
		UE_LOG(LogBranchSmoke, Display, TEXT("=== BRANCH SMOKE TEST PASSED (Ch4 Phases 0-4 + Ch5 Phases 0-1 green — GUID seam + SaveGame write). ==="));
		return 0;
	}
	UE_LOG(LogBranchSmoke, Error, TEXT("=== BRANCH SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif
}
