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
// Phases 1-4 append their own blocks here. CP3 lesson #6: NAMED namespace to
// avoid unity-build redefinition collisions with the sibling commandlets.

#include "BranchSmokeTestCommandlet.h"
#include "BranchSubsystem.h"
#include "BranchTypes.h"

#include "Engine/World.h"
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

	if (R.Failures == 0)
	{
		UE_LOG(LogBranchSmoke, Display, TEXT("=== BRANCH SMOKE TEST PASSED (Ch4 Phase 0 seam green). ==="));
		return 0;
	}
	UE_LOG(LogBranchSmoke, Error, TEXT("=== BRANCH SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif
}
