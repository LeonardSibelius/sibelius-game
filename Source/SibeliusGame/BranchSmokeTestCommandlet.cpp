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
// PHASE 2 (Ch5) — load + re-apply (SIB-29):
// [HARD] deploy a known edit, reset live branchables to default, ApplyDeployedSave
//        restores each edited object byte-exact (Ch4 round-trip, via the file)
// [HARD] objects with NO saved delta stay at default (no spurious writes)
// [HARD] applying the same save twice is a no-op the second time (idempotent)
// [HARD] an orphan GUID (resolves to nothing) is skipped, not fatal
// [HARD] a save newer than CurrentSaveVersion is refused (no mutation)
//
// PHASE 3 (Ch5) — versioning + fail-safe (SIB-29):
// [HARD] migration: a v1 save migrates v1->v2 (marker stamped) and applies; a
//        current save is a no-op; a newer save can't downgrade
// [HARD] a newer-than-current save on disk is refused (no fallback, no mutation)
// [HARD] fail-safe (D6): a corrupt primary falls back to the last-good backup and
//        applies it; with neither usable, load fails safe to authored default
//        (no crash, no partial write)
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
#include "Misc/FileHelper.h"        // Ch5 Phase 3: FFileHelper (corrupt a .sav on disk)
#include "Misc/Paths.h"             // Ch5 Phase 3: FPaths (resolve the save-slot file path)

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
	const FString DeploySandboxSlot   = TEXT("SmokeDeploySlot_Temp");
	const FString DeploySandboxBackup = DeploySandboxSlot + TEXT("_Backup"); // matches BackupSlotName()
	Branch->SetDeploySlotName(DeploySandboxSlot);
	FSibeliusSaveIO::Delete(DeploySandboxSlot);   // clear any stale artifacts up front
	FSibeliusSaveIO::Delete(DeploySandboxBackup); // (the apply path promotes good saves here)

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

		// =====================================================================
		//  PHASE 2 (Ch5) — load + re-apply (SIB-29). Deploy a known edit, reset
		//  the live branchables to default (simulate a fresh level load), then
		//  ApplyDeployedSave: edited objects restore byte-exact (the Ch4 round-trip
		//  equality, now through the save file); no-delta objects stay at default;
		//  applying twice is a no-op; an orphan GUID is skipped; a newer-version
		//  save is refused. Sandbox slot, cleaned up.
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 2 (Ch5): load + re-apply ---"));

		FSibeliusSaveIO::Delete(DeploySandboxSlot);

		// Deploy a KNOWN edit at Main: Refactorable refactored + BuildSite built +
		// Book = 5 are CHANGED; HatchLock locked + Key = 0 are DEFAULT (no delta).
		Refac->RestoreBranchState(1);
		Site->RestoreBranchState(1);
		Hatch->RestoreBranchState(1);              // default (locked) -> no delta
		Inv->RestoreCount(EResourceType::Book, 5);
		Inv->RestoreCount(EResourceType::Key, 0);
		R.Check(Branch->RequestDeploy(), TEXT("C5-P2: deploy the known edit at Main"));
		R.Check(FSibeliusSaveIO::Has(DeploySandboxSlot), TEXT("C5-P2: deploy wrote the slot"));

		// Ch4 round-trip baseline = the deployed (pre-reset) states.
		const uint8 RefacWant = Refac->CaptureBranchState(); // 1
		const uint8 SiteWant  = Site->CaptureBranchState();  // 1
		const uint8 HatchWant = Hatch->CaptureBranchState(); // 1 (default)
		const int32 BookWant  = Inv->GetCount(EResourceType::Book); // 5
		const int32 KeyWant   = Inv->GetCount(EResourceType::Key);  // 0

		// Reset the live branchables to authored DEFAULT (a fresh level load).
		Refac->RestoreBranchState(Refac->GetDefaultBranchState()); // 0
		Site->RestoreBranchState(Site->GetDefaultBranchState());   // 0
		Hatch->RestoreBranchState(Hatch->GetDefaultBranchState()); // 1 (locked)
		Inv->RestoreCount(EResourceType::Book, 0);
		Inv->RestoreCount(EResourceType::Key, 0);
		R.Check(Refac->CaptureBranchState() == 0 && Site->CaptureBranchState() == 0
			&& Inv->GetCount(EResourceType::Book) == 0,
			TEXT("C5-P2: live branchables reset to default before apply"));

		// APPLY: load the slot + re-apply the deltas through raw RestoreBranchState.
		R.Check(Branch->ApplyDeployedSave(), TEXT("C5-P2: ApplyDeployedSave loads + re-applies the slot"));

		// Edited objects restored byte-exact (Ch4 round-trip, now via the file).
		R.Check(Refac->CaptureBranchState() == RefacWant, TEXT("C5-P2: Refactorable restored byte-exact from the save"));
		R.Check(Site->CaptureBranchState() == SiteWant,   TEXT("C5-P2: BuildSite restored byte-exact from the save"));
		R.Check(Inv->GetCount(EResourceType::Book) == BookWant, TEXT("C5-P2: Book ledger restored from the save"));
		// Objects: >= our 2 forced deltas (the level may author other default-state
		// branchables, which write no delta). Resources are fully controlled: Book
		// non-zero + Key zero -> exactly 1 resource delta.
		R.Check(Branch->GetLastApplyObjectsForTest() >= 2 && Branch->GetLastApplyResourcesForTest() == 1,
			TEXT("C5-P2: applied the forced object delta(s) + exactly 1 resource delta"));

		// Objects with NO saved delta stay at default (no spurious writes).
		R.Check(Hatch->CaptureBranchState() == Hatch->GetDefaultBranchState(),
			TEXT("C5-P2: unchanged HatchLock stays default (no delta in the save)"));
		R.Check(Inv->GetCount(EResourceType::Key) == 0,
			TEXT("C5-P2: zero-Key stays default (no delta in the save)"));

		// Idempotency: applying the same save again changes nothing.
		R.Check(Branch->ApplyDeployedSave(), TEXT("C5-P2: second ApplyDeployedSave succeeds"));
		R.Check(Refac->CaptureBranchState() == RefacWant && Site->CaptureBranchState() == SiteWant
			&& Hatch->CaptureBranchState() == HatchWant
			&& Inv->GetCount(EResourceType::Book) == BookWant
			&& Inv->GetCount(EResourceType::Key) == KeyWant,
			TEXT("C5-P2: re-applying the same save is a no-op (idempotent)"));

		// Orphan GUID: a delta that resolves to no live object is skipped, not fatal.
		USibeliusSaveGame* OrphanSave = NewObject<USibeliusSaveGame>(GetTransientPackage());
		if (OrphanSave)
		{
			OrphanSave->SaveVersion = USibeliusSaveGame::CurrentSaveVersion;
			FBranchObjectState Orphan; Orphan.ObjectId = FGuid::NewGuid(); Orphan.State = 1; // resolves to nothing
			FBranchObjectState RealOne; RealOne.ObjectId = Refac->GetBranchId(); RealOne.State = 1;
			OrphanSave->ObjectDeltas.Add(Orphan);   // orphan FIRST — must not abort the rest
			OrphanSave->ObjectDeltas.Add(RealOne);
			R.Check(FSibeliusSaveIO::Commit(OrphanSave, DeploySandboxSlot), TEXT("C5-P2: wrote an orphan-bearing save"));

			Refac->RestoreBranchState(0); // reset the real target so its re-apply is observable
			R.Check(Branch->ApplyDeployedSave(), TEXT("C5-P2: apply skips the orphan and continues"));
			R.Check(Refac->CaptureBranchState() == 1, TEXT("C5-P2: the real delta still applied despite the orphan"));
			R.Check(Branch->GetLastApplyOrphansForTest() == 1, TEXT("C5-P2: exactly one orphan GUID was skipped"));
			R.Check(Branch->GetLastApplyObjectsForTest() == 1, TEXT("C5-P2: only the one resolvable delta was applied"));
		}

		// Version guard: a save newer than CurrentSaveVersion is refused (no mutation).
		USibeliusSaveGame* FutureSave = NewObject<USibeliusSaveGame>(GetTransientPackage());
		if (FutureSave)
		{
			FutureSave->SaveVersion = USibeliusSaveGame::CurrentSaveVersion + 1;
			FBranchObjectState Future; Future.ObjectId = Refac->GetBranchId(); Future.State = 0; // would un-refactor if wrongly applied
			FutureSave->ObjectDeltas.Add(Future);
			R.Check(FSibeliusSaveIO::Commit(FutureSave, DeploySandboxSlot), TEXT("C5-P2: wrote a newer-version save"));

			Refac->RestoreBranchState(1); // a wrongly-applied future save would flip this to 0
			R.Check(!Branch->ApplyDeployedSave(), TEXT("C5-P2: newer-version save is skipped (returns false)"));
			R.Check(Refac->CaptureBranchState() == 1, TEXT("C5-P2: skipped future save did NOT mutate the world"));
		}

		// Clean up the sandbox slots (apply promoted a backup too).
		FSibeliusSaveIO::Delete(DeploySandboxSlot);
		FSibeliusSaveIO::Delete(DeploySandboxBackup);
		R.Check(!FSibeliusSaveIO::Has(DeploySandboxSlot) && !FSibeliusSaveIO::Has(DeploySandboxBackup),
			TEXT("C5-P2: sandbox slots cleaned up after the test"));

		// =====================================================================
		//  PHASE 3 (Ch5) — versioning + fail-safe (SIB-29). A v1 save migrates to
		//  v2 and applies; a newer-than-current save is refused; a corrupt primary
		//  falls back to the last-good backup; with neither usable, load fails safe
		//  (world stays default, no crash, no partial write). Sandbox slots, cleaned.
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- Phase 3 (Ch5): versioning + fail-safe ---"));

		FSibeliusSaveIO::Delete(DeploySandboxSlot);
		FSibeliusSaveIO::Delete(DeploySandboxBackup);

		// Helpers: reset the live branchables to authored default, and hand-build a
		// deploy save at a chosen version with Refac/Site/Book deltas.
		auto ResetToDefault = [&]()
		{
			Refac->RestoreBranchState(Refac->GetDefaultBranchState());
			Site->RestoreBranchState(Site->GetDefaultBranchState());
			Hatch->RestoreBranchState(Hatch->GetDefaultBranchState());
			Inv->RestoreCount(EResourceType::Book, 0);
			Inv->RestoreCount(EResourceType::Key, 0);
		};
		auto MakeSave = [&](int32 Version, uint8 RefacState, uint8 SiteState, int32 Book) -> USibeliusSaveGame*
		{
			USibeliusSaveGame* S = NewObject<USibeliusSaveGame>(GetTransientPackage());
			S->SaveVersion = Version;
			{ FBranchObjectState d; d.ObjectId = Refac->GetBranchId(); d.State = RefacState; S->ObjectDeltas.Add(d); }
			{ FBranchObjectState d; d.ObjectId = Site->GetBranchId();  d.State = SiteState;  S->ObjectDeltas.Add(d); }
			if (Book != 0)
			{
				FResourceEntry e; e.Resource = EResourceType::Book;
				e.EntryId = Inv->GetOrCreateResourceId(EResourceType::Book); e.Count = Book;
				S->ResourceDeltas.Add(e);
			}
			return S;
		};
		// True on-disk corruption: overwrite the slot's .sav with a short garbage
		// buffer so LoadGameFromSlot fails the header/deserialize and the load routes
		// to Corrupt — without ever instantiating the abstract USaveGame base. Path
		// mirrors the generic save system (<ProjectSaved>/SaveGames/<Slot>.sav).
		auto CorruptSlotFile = [](const FString& Slot) -> bool
		{
			const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Slot + TEXT(".sav"));
			const TArray<uint8> Garbage = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03 };
			return FFileHelper::SaveArrayToFile(Garbage, *Path);
		};

		// --- (a) Migration step in isolation: v1 -> v2 marks + bumps; current is a
		//         no-op; newer can't downgrade. ---
		{
			USibeliusSaveGame* V1 = MakeSave(1, 1, 1, 9);
			R.Check(V1 && V1->MigrateToCurrent(), TEXT("C5-P3: a v1 save migrates to current"));
			R.Check(V1 && V1->SaveVersion == USibeliusSaveGame::CurrentSaveVersion,
				TEXT("C5-P3: migrated save is stamped at CurrentSaveVersion (v2)"));
			R.Check(V1 && V1->FormatNote == TEXT("migrated:v1->v2"),
				TEXT("C5-P3: v1->v2 migrator ran (FormatNote marker set)"));

			USibeliusSaveGame* V2 = MakeSave(USibeliusSaveGame::CurrentSaveVersion, 1, 1, 9);
			R.Check(V2 && V2->MigrateToCurrent() && V2->SaveVersion == USibeliusSaveGame::CurrentSaveVersion,
				TEXT("C5-P3: a current-version save migrates as a no-op"));

			USibeliusSaveGame* Vn = MakeSave(USibeliusSaveGame::CurrentSaveVersion + 1, 1, 1, 9);
			R.Check(Vn && !Vn->MigrateToCurrent(),
				TEXT("C5-P3: a newer-than-current save can't be migrated (no downgrade)"));
		}

		// --- (b) v1 save on disk migrates AND applies end-to-end. ---
		{
			USibeliusSaveGame* V1 = MakeSave(1, 1, 1, 9);
			R.Check(FSibeliusSaveIO::Commit(V1, DeploySandboxSlot), TEXT("C5-P3: wrote a v1 save to the primary slot"));
			ResetToDefault();
			R.Check(Branch->ApplyDeployedSave(), TEXT("C5-P3: ApplyDeployedSave loads + migrates + applies the v1 save"));
			R.Check(Branch->GetLastLoadSourceForTest() == EDeployLoadSource::Primary,
				TEXT("C5-P3: applied from the primary slot"));
			R.Check(Refac->CaptureBranchState() == 1 && Site->CaptureBranchState() == 1
				&& Inv->GetCount(EResourceType::Book) == 9,
				TEXT("C5-P3: migrated v1 save applied byte-exact"));
		}

		// --- (c) Newer-than-current on disk is refused, no fallback, no mutation. ---
		{
			USibeliusSaveGame* Vn = MakeSave(USibeliusSaveGame::CurrentSaveVersion + 1, 0, 0, 0); // would clear if wrongly applied
			R.Check(FSibeliusSaveIO::Commit(Vn, DeploySandboxSlot), TEXT("C5-P3: wrote a newer-version save to the primary"));
			Refac->RestoreBranchState(1); // a wrong apply would flip this to 0
			Site->RestoreBranchState(1);
			R.Check(!Branch->ApplyDeployedSave(), TEXT("C5-P3: newer-version save refused (returns false)"));
			R.Check(Branch->GetLastLoadSourceForTest() == EDeployLoadSource::None,
				TEXT("C5-P3: refusal applied nothing (no fallback to an older backup)"));
			R.Check(Refac->CaptureBranchState() == 1 && Site->CaptureBranchState() == 1,
				TEXT("C5-P3: newer-version refusal did NOT mutate the world"));
		}

		// --- (d) Corrupt primary falls back to the last-good backup. ---
		{
			// Establish a good backup: deploy a known state, then apply (promotes it).
			ResetToDefault();
			Refac->RestoreBranchState(1);
			Site->RestoreBranchState(1);
			Inv->RestoreCount(EResourceType::Book, 9);
			R.Check(Branch->RequestDeploy(), TEXT("C5-P3: deploy a known-good state to the primary"));
			R.Check(Branch->ApplyDeployedSave() && Branch->GetLastLoadSourceForTest() == EDeployLoadSource::Primary,
				TEXT("C5-P3: apply promotes the good primary to the last-good backup"));
			R.Check(FSibeliusSaveIO::Has(DeploySandboxBackup), TEXT("C5-P3: last-good backup now exists"));

			// Corrupt the primary on disk: garbage bytes -> LoadGameFromSlot fails to
			// deserialize -> routes to the Corrupt path (real truncation, no ensures).
			R.Check(CorruptSlotFile(DeploySandboxSlot), TEXT("C5-P3: corrupted the primary slot (garbage bytes on disk)"));

			ResetToDefault();
			R.Check(Branch->ApplyDeployedSave(), TEXT("C5-P3: corrupt primary falls back to backup and applies"));
			R.Check(Branch->GetLastLoadSourceForTest() == EDeployLoadSource::Backup,
				TEXT("C5-P3: the fallback sourced from the backup slot"));
			R.Check(Refac->CaptureBranchState() == 1 && Site->CaptureBranchState() == 1
				&& Inv->GetCount(EResourceType::Book) == 9,
				TEXT("C5-P3: backup applied byte-exact after primary corruption"));
		}

		// --- (e) Both bad -> fail safe to authored default (no crash, no partial write). ---
		{
			R.Check(CorruptSlotFile(DeploySandboxSlot), TEXT("C5-P3: corrupted the primary again (garbage bytes on disk)"));
			R.Check(FSibeliusSaveIO::Delete(DeploySandboxBackup), TEXT("C5-P3: removed the backup (both unusable)"));
			ResetToDefault();
			R.Check(!Branch->ApplyDeployedSave(), TEXT("C5-P3: with no usable save, apply fails safe (returns false)"));
			R.Check(Branch->GetLastLoadSourceForTest() == EDeployLoadSource::None,
				TEXT("C5-P3: nothing applied on the fail-safe path"));
			R.Check(Refac->CaptureBranchState() == Refac->GetDefaultBranchState()
				&& Site->CaptureBranchState() == Site->GetDefaultBranchState()
				&& Inv->GetCount(EResourceType::Book) == 0,
				TEXT("C5-P3: world stays at authored default (no partial apply)"));
		}

		// Clean up the sandbox slots.
		FSibeliusSaveIO::Delete(DeploySandboxSlot);
		FSibeliusSaveIO::Delete(DeploySandboxBackup);
		R.Check(!FSibeliusSaveIO::Has(DeploySandboxSlot) && !FSibeliusSaveIO::Has(DeploySandboxBackup),
			TEXT("C5-P3: sandbox slots cleaned up after the test"));

		// =====================================================================
		//  SIB-38 — GUID baking: assign-once across reload + duplication safety.
		//  (Cross-session .umap baking itself is verified in the PIE playtest, not
		//  headless — here we prove the invariants that baking relies on.)
		// =====================================================================
		UE_LOG(LogBranchSmoke, Display, TEXT("--- SIB-38: GUID baking (assign-once + duplication) ---"));

		// A set BranchId is never regenerated by the runtime fallback — this is what
		// protects a baked id from being clobbered on load (a "simulated reload").
		const FGuid SiteIdBaked = Site->GetOrCreateBranchId();
		R.Check(SiteIdBaked.IsValid(), TEXT("SIB-38: BuildSite carries a valid BranchId after ensure"));
		R.Check(Site->GetOrCreateBranchId() == SiteIdBaked && Site->GetBranchId() == SiteIdBaked,
			TEXT("SIB-38: a set BranchId is never regenerated (assign-once across a simulated reload)"));

		const FGuid HatchIdBaked = Hatch->GetOrCreateBranchId();
		const FGuid RefacIdBaked = Refac->GetOrCreateBranchId();

		// Duplication safety: a copied branchable must NOT share its source's id
		// (PostDuplicate regenerates it). Without that, copy-paste in-editor would
		// alias two actors to the same GUID and a deploy would resolve the wrong one.
		ABuildSite* DupSite = DuplicateObject<ABuildSite>(Site, GetTransientPackage());
		R.Check(DupSite && DupSite->GetBranchId().IsValid() && DupSite->GetBranchId() != SiteIdBaked,
			TEXT("SIB-38: duplicated BuildSite gets a distinct, valid id (no shared id)"));

		AHatchLock* DupHatch = DuplicateObject<AHatchLock>(Hatch, GetTransientPackage());
		R.Check(DupHatch && DupHatch->GetBranchId().IsValid() && DupHatch->GetBranchId() != HatchIdBaked,
			TEXT("SIB-38: duplicated HatchLock gets a distinct, valid id"));

		URefactorableComponent* DupRefac = DuplicateObject<URefactorableComponent>(Refac, GetTransientPackage());
		R.Check(DupRefac && DupRefac->GetBranchId().IsValid() && DupRefac->GetBranchId() != RefacIdBaked,
			TEXT("SIB-38: duplicated RefactorableComponent gets a distinct, valid id"));
	}

	if (R.Failures == 0)
	{
		UE_LOG(LogBranchSmoke, Display, TEXT("=== BRANCH SMOKE TEST PASSED (Ch4 Phases 0-4 + Ch5 Phases 0-3 green — GUID seam + deploy write/load/migrate/fail-safe). ==="));
		return 0;
	}
	UE_LOG(LogBranchSmoke, Error, TEXT("=== BRANCH SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif
}
