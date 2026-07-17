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
	// The office the O key returns to, and a host map to spawn transient actors in.
	// (The forests are GONE — Walt's cut, 2026-07-16: the Many Worlds deck and its
	// baked forest levels were deleted; this gate now guards the return path only.)
	const FString OfficeMapPackage = TEXT("/Game/L_Office_v02");
	const FString HostMapPackage   = TEXT("/Game/Maps/L_AI_Temple");

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

	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("=== Away-worlds smoke test: back-to-office path ==="));

	FResult R;

	// --- ASSERT 2: the Back-to-Office travel path exists. ------------------------------
	// Headless can't press O, so prove the path structurally: the O-key destination (the
	// office) loads, AND the "away from office" rule that gates the O key is correct — live
	// in every non-office level, a no-op in the office, PIE-prefix-safe either way.
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 2: the back-to-office travel path exists ---"));
	{
		UWorld* OfficeWorld = LoadMapWorld(OfficeMapPackage);
		R.Check(OfficeWorld != nullptr, FString::Printf(TEXT("office level (O-key destination) exists and loads (%s)"), *OfficeMapPackage));

		const ASibeliusGameCharacter* CharCDO = GetDefault<ASibeliusGameCharacter>();
		R.Check(CharCDO != nullptr, TEXT("player character CDO resolves"));
		if (CharCDO)
		{
			// O / the hint are live in EVERY away-from-office level (temple, cathedral, carousel).
			R.Check(CharCDO->IsAwayFromOfficeLevelName(TEXT("L_Carousel")) &&
				CharCDO->IsAwayFromOfficeLevelName(TEXT("L_AI_Temple")) &&
				CharCDO->IsAwayFromOfficeLevelName(TEXT("L_Cathedral")),
				TEXT("O is live in every away-from-office level (carousel / temple / cathedral)"));

			// ...and a no-op in the office itself.
			R.Check(!CharCDO->IsAwayFromOfficeLevelName(TEXT("L_Office_v02")),
				TEXT("O no-ops in the office (L_Office_v02)"));

			// PIE-prefix safe: a streaming prefix must not flip either verdict.
			R.Check(!CharCDO->IsAwayFromOfficeLevelName(TEXT("UEDPIE_0_L_Office_v02")) &&
				CharCDO->IsAwayFromOfficeLevelName(TEXT("UEDPIE_0_L_AI_Temple")),
				TEXT("PIE-prefixed names resolve correctly (office = no-op, temple = away)"));
		}
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
