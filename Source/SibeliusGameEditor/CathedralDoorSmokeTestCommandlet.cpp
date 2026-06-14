// CathedralDoorSmokeTestCommandlet.cpp — SIB-31 headless smoke test for ACathedralDoor.
//
// Bar (docs/sib-31-cathedral-door-notes.md, D1-D6):
// [HARD] L_Office_v02 loads
// [HARD] the ACathedralDoor CLASS CONTRACT holds. SIB-44: the attic door is now an
//        AHiddenDoor (the "Carousel of Fates" world-gate), so a placed ACathedralDoor in
//        L_Office_v02 is no longer required — the class still ships on the cathedral's
//        RETURN doors, so the contract is exercised on any placed instances here and
//        otherwise on a transient spawn (its defaults match the per-door checks below).
// [HARD] the door's TargetLevelName package exists on disk (D2 — OpenLevel on a
//        bad name is a silent no-op)
// [HARD] ACathedralDoor does NOT implement IBranchable (D3 — never in deploy saves)
// [HARD] the branch registry never picks the door up (D3 record-count proxy)
// [HARD] travel guard: allowed at depth 0, REFUSED while branched, allowed again
//        after discard (D1)
// [HARD] focused prompt is the authored text
//
// CP3 lesson #6: NAMED namespace to avoid unity-build redefinition collisions with
// the sibling smoke commandlets.

#include "CathedralDoorSmokeTestCommandlet.h"
#include "CathedralDoor.h"
#include "Branchable.h"
#include "BranchSubsystem.h"
#include "Interactable.h"

#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator
#include "Misc/PackageName.h"       // FPackageName::DoesPackageExist
#include "UObject/Package.h"        // GetTransientPackage

#if WITH_EDITOR
#include "FileHelpers.h"            // UEditorLoadingAndSavingUtils
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCathedralDoorSmoke, Log, All);

namespace CathedralDoorSmokeTestNS
{
	const FString DefaultMapPackage = TEXT("/Game/L_Office_v02");

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogCathedralDoorSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogCathedralDoorSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	// Resolve the door's TargetLevelName to a package path: a short name (the
	// shipping default, "L_Cathedral") lives under /Game/Maps; a value containing
	// '/' is taken verbatim.
	FString LevelNameToPackage(const FName& LevelName)
	{
		const FString NameStr = LevelName.ToString();
		return NameStr.Contains(TEXT("/")) ? NameStr : (TEXT("/Game/Maps/") + NameStr);
	}
}

UCathedralDoorSmokeTestCommandlet::UCathedralDoorSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCathedralDoorSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogCathedralDoorSmoke, Error, TEXT("CathedralDoorSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
	return 1;
#else
	// Function-scoped so the namespace doesn't leak into other TUs under unity build.
	using namespace CathedralDoorSmokeTestNS;

	UE_LOG(LogCathedralDoorSmoke, Display, TEXT("=== SIB-31 cathedral door smoke test: %s ==="), *DefaultMapPackage);

	FResult R;

	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(DefaultMapPackage);
	R.Check(World != nullptr, FString::Printf(TEXT("Map loads (%s)"), *DefaultMapPackage));
	if (!World)
	{
		UE_LOG(LogCathedralDoorSmoke, Error, TEXT("=== CATHEDRAL DOOR SMOKE TEST FAILED: could not load map. ==="));
		return 1;
	}

	// --- 1) ACathedralDoor class contract.
	// SIB-44: the attic door is now an AHiddenDoor (the "Carousel of Fates" world-gate), so a
	// placed ACathedralDoor in L_Office_v02 is NO LONGER REQUIRED. The class still ships on the
	// cathedral's RETURN doors, so its contract still matters — exercise it on whatever placed
	// instances exist here (the office should now have none) and otherwise on a transient spawn.
	// The transient's class defaults (TargetLevelName=L_Cathedral, prompt "Enter the cathedral
	// [E]") are exactly what the per-door checks below assert, so the contract stays gated.
	TArray<ACathedralDoor*> Doors;
	for (TActorIterator<ACathedralDoor> It(World); It; ++It)
	{
		Doors.Add(*It);
	}

	UE_LOG(LogCathedralDoorSmoke, Display,
		TEXT("  [INFO] placed ACathedralDoor in %s: %d (attic door is now AHiddenDoor — SIB-44 carousel swap)"),
		*DefaultMapPackage, Doors.Num());

	if (Doors.Num() == 0)
	{
		ACathedralDoor* TransientDoor = World->SpawnActor<ACathedralDoor>();
		R.Check(TransientDoor != nullptr,
			TEXT("ACathedralDoor class still spawnable — contract checked on a transient (return doors still ship the class)"));
		if (TransientDoor)
		{
			Doors.Add(TransientDoor);
		}
	}

	// --- 2) D3 class invariant: never IBranchable, so it can never enter a deploy save.
	R.Check(!ACathedralDoor::StaticClass()->ImplementsInterface(UBranchable::StaticClass()),
		TEXT("ACathedralDoor does NOT implement IBranchable (D3: stays out of deploy saves)"));

	// --- 3) Per-door checks: D2 target package on disk + the authored prompt.
	for (ACathedralDoor* Door : Doors)
	{
		R.Check(!Door->TargetLevelName.IsNone(),
			FString::Printf(TEXT("%s: TargetLevelName is set"), *Door->GetName()));

		const FString LevelPackage = LevelNameToPackage(Door->TargetLevelName);
		R.Check(FPackageName::DoesPackageExist(LevelPackage),
			FString::Printf(TEXT("%s: TargetLevelName package exists on disk (%s) — D2"), *Door->GetName(), *LevelPackage));

		// AActor::ProcessEvent silently no-ops in a non-game world, so the
		// BlueprintNativeEvent thunk returns an EMPTY FText without this guard.
		FEditorScriptExecutionGuard ScriptGuard;
		const FText Prompt = IInteractable::Execute_GetInteractionPrompt(Door);
		R.Check(Prompt.ToString() == TEXT("Enter the cathedral [E]"),
			FString::Printf(TEXT("%s: prompt is the authored text (got \"%s\")"), *Door->GetName(), *Prompt.ToString()));
	}

	// --- 4) D1 travel guard, driven against a NewObject'd subsystem (BranchSmoke pattern).
	UBranchSubsystem* Branch = NewObject<UBranchSubsystem>(GetTransientPackage());
	Branch->SetBranchWorld(World);

	R.Check(ACathedralDoor::IsTravelAllowed(nullptr), TEXT("Travel allowed with no branch subsystem (headless world)"));
	R.Check(ACathedralDoor::IsTravelAllowed(Branch), TEXT("Travel allowed at Main (depth 0)"));
	R.Check(Branch->EnterBranch(), TEXT("EnterBranch for the guard probe"));
	R.Check(!ACathedralDoor::IsTravelAllowed(Branch), TEXT("Travel REFUSED while branched (depth 1) — D1"));
	R.Check(Branch->DiscardBranch(), TEXT("DiscardBranch unwinds the probe"));
	R.Check(ACathedralDoor::IsTravelAllowed(Branch), TEXT("Travel allowed again after discard"));

	// --- 5) D3 record-count proxy: the registry (the deploy save's source) never
	// picks the door up, even with one live in the world.
	Branch->RebuildRegistryForTest();
	bool bDoorInRegistry = false;
	for (const TPair<FGuid, TWeakObjectPtr<UObject>>& Pair : Branch->GetRegistryForTest())
	{
		if (Cast<ACathedralDoor>(Pair.Value.Get()))
		{
			bDoorInRegistry = true;
			break;
		}
	}
	R.Check(!bDoorInRegistry, TEXT("Door absent from the branch registry (D3: zero deploy-save records from the door)"));

	if (R.Failures == 0)
	{
		UE_LOG(LogCathedralDoorSmoke, Display, TEXT("=== CATHEDRAL DOOR SMOKE TEST PASSED (SIB-31 green). ==="));
		return 0;
	}

	UE_LOG(LogCathedralDoorSmoke, Error, TEXT("=== CATHEDRAL DOOR SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif // WITH_EDITOR
}
