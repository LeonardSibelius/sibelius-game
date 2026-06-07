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
// Phases 2-4 append their own blocks here. CP3 lesson #6: NAMED namespace to
// avoid unity-build redefinition collisions with the sibling commandlets.

#include "BranchSmokeTestCommandlet.h"
#include "BranchSubsystem.h"
#include "BranchTypes.h"

#include "RefactorableComponent.h"
#include "InventoryComponent.h"
#include "BuildSite.h"
#include "HatchLock.h"
#include "CompileTypes.h"           // EResourceType

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

	// Byte-exact manifest comparison (the registry order is deterministic between
	// two captures with no intervening spawn/destroy).
	bool ManifestsEqual(const FBranchManifest& A, const FBranchManifest& B)
	{
		if (A.Resources.Num() != B.Resources.Num() || A.Objects.Num() != B.Objects.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Resources.Num(); ++i)
		{
			if (A.Resources[i].Resource != B.Resources[i].Resource || A.Resources[i].Count != B.Resources[i].Count)
			{
				return false;
			}
		}
		for (int32 i = 0; i < A.Objects.Num(); ++i)
		{
			if (A.Objects[i].RegistryIndex != B.Objects[i].RegistryIndex || A.Objects[i].State != B.Objects[i].State)
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

	// [HARD] No nesting yet: re-enter from Branched is rejected.
	R.Check(!Branch->EnterBranch(), TEXT("EnterBranch() rejected while Branched (no nesting until Phase 3)"));

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
	}

	if (R.Failures == 0)
	{
		UE_LOG(LogBranchSmoke, Display, TEXT("=== BRANCH SMOKE TEST PASSED (Ch4 Phase 0 + Phase 1 green). ==="));
		return 0;
	}
	UE_LOG(LogBranchSmoke, Error, TEXT("=== BRANCH SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif
}
