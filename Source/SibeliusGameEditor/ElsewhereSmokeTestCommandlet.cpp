// ElsewhereSmokeTestCommandlet.cpp — see header. Headless ship gate for the Many Worlds
// door now that it travels to a FIXED authored forest (Mode A) and the curio / cabinet /
// builder flow is set aside. Shell cloned from the sibling gates (the Sauce gate): LoadPackage
// host map -> InitWorld -> spawn transient actors -> CleanupWorld on the exit path (without
// the cleanup the engine shuts down holding a live orphaned world -> nonzero exit even when
// every assertion PASSED). NAMED namespace + function-scoped `using` (unity-build safe); no
// variable shadowing (warnings-as-errors). Run with the editor CLOSED (port 3000).

#include "ElsewhereSmokeTestCommandlet.h"

#include "SauceDoor.h"
#include "HiddenDoor.h"
#include "SibeliusGameCharacter.h"

#include "Engine/World.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogElsewhereSmokeTest, Log, All);

namespace ElsewhereSmokeTestNS
{
	// The fixed authored forest the Sauce Door now travels into (Mode A), the office it
	// returns to (the O-key OpenLevel target), and a host map to spawn transient actors in.
	const FString ForestMapPackage = TEXT("/Game/Maps/L_Poplar_Forest");
	const FString OfficeMapPackage = TEXT("/Game/L_Office_v02");
	const FString HostMapPackage   = TEXT("/Game/Maps/L_AI_Temple");
	const FName   ForestLevelName  = TEXT("L_Poplar_Forest");

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogElsewhereSmokeTest, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogElsewhereSmokeTest, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	// LoadPackage + find the contained world (null if either step fails). Used for the
	// "exists and loads" asserts — no InitWorld, so there is nothing to tear down for these.
	UWorld* LoadMapWorld(const FString& PackageName)
	{
		UPackage* Pkg = LoadPackage(nullptr, *PackageName, LOAD_None);
		return Pkg ? UWorld::FindWorldInPackage(Pkg) : nullptr;
	}
}

UElsewhereSmokeTestCommandlet::UElsewhereSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UElsewhereSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogElsewhereSmokeTest, Error, TEXT("ElsewhereSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
	return 1;
#else
	// Function-scoped so the namespace doesn't leak into other TUs under unity build.
	using namespace ElsewhereSmokeTestNS;

	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("=== THE MANY WORLDS door smoke test: forest loads + back-to-office path ==="));

	FResult R;

	// --- ASSERT 1: the fixed authored forest level (Mode A) exists and loads. ----------
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 1: the Poplar forest level loads ---"));
	{
		UWorld* ForestWorld = LoadMapWorld(ForestMapPackage);
		R.Check(ForestWorld != nullptr, FString::Printf(TEXT("forest level exists and loads (%s)"), *ForestMapPackage));
	}

	// --- ASSERT 2: the Back-to-Office travel path exists. ------------------------------
	// Headless can't press O, so prove the path structurally: the O-key destination (the
	// office) loads, AND the wander-world allowlist that gates the O key includes the forest.
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 2: the back-to-office travel path exists ---"));
	{
		UWorld* OfficeWorld = LoadMapWorld(OfficeMapPackage);
		R.Check(OfficeWorld != nullptr, FString::Printf(TEXT("office level (O-key destination) exists and loads (%s)"), *OfficeMapPackage));

		const ASibeliusGameCharacter* CharCDO = GetDefault<ASibeliusGameCharacter>();
		R.Check(CharCDO && CharCDO->IsWanderWorldLevel(ForestLevelName),
			FString::Printf(TEXT("wander-world allowlist contains %s (O key + hint live there)"), *ForestLevelName.ToString()));

		// Regression guard for the PIE-prefix bug: the SAME name carrying a PIE prefix must
		// still match via the shared prefix-safe helper (the live O-key / HUD path).
		R.Check(CharCDO && CharCDO->IsWanderWorldLevelName(TEXT("UEDPIE_0_L_Poplar_Forest")),
			TEXT("PIE-prefixed level name resolves to the wander world (prefix-safe match)"));
	}

	// --- ASSERT 3: ASauceDoor is a plain hidden-door travel door (curio flow bypassed). -
	// Spawn it in a hand-init host world and confirm the inherited Code-Vision reveal still
	// drives collision both ways. It now travels via the inherited TravelTargetLevel (the
	// office-obelisk path) instead of the UElsewhereSubsystem roll, which is set aside.
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 3: the Sauce Door reveals like a hidden door ---"));
	UWorld* HostWorld = LoadMapWorld(HostMapPackage);
	R.Check(HostWorld != nullptr, FString::Printf(TEXT("host map loads (%s)"), *HostMapPackage));
	if (!HostWorld)
	{
		UE_LOG(LogElsewhereSmokeTest, Error, TEXT("=== ELSEWHERE SMOKE TEST FAILED: no host world. ==="));
		return 1;
	}
	HostWorld->WorldType = EWorldType::Editor;
	HostWorld->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false));

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;

	ASauceDoor* Door = HostWorld->SpawnActor<ASauceDoor>(ASauceDoor::StaticClass(), SpawnParams);
	R.Check(Door != nullptr, TEXT("ASauceDoor spawns"));
	R.Check(Door && Door->IsA(AHiddenDoor::StaticClass()),
		TEXT("ASauceDoor IS an AHiddenDoor (keeps the Code Vision reveal/shimmer + plain travel)"));
	R.Check(Door && Door->RunCollisionSelfTest(),
		TEXT("reveal drives collision both ways: hidden+blocking <-> revealed+passable"));

	// Tear down the hand-initialised world (the exit-3 lesson: no implicit counterpart to InitWorld).
	HostWorld->CleanupWorld();

	if (R.Failures == 0)
	{
		UE_LOG(LogElsewhereSmokeTest, Display, TEXT("=== ELSEWHERE SMOKE TEST PASSED (forest travel path holds headless). ==="));
		return 0;
	}
	UE_LOG(LogElsewhereSmokeTest, Error, TEXT("=== ELSEWHERE SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif // WITH_EDITOR
}
