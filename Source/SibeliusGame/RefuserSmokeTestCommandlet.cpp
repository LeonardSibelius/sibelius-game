// RefuserSmokeTestCommandlet.cpp - CP3 headless smoke test for L_RefuserTest.

#include "RefuserSmokeTestCommandlet.h"

#include "RefuserSpawner.h"
#include "RefuserController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"

#if WITH_EDITOR
#include "FileHelpers.h" // UEditorLoadingAndSavingUtils (editor-only)
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRefuserSmokeTest, Log, All);

// Named (not anonymous) namespace so these helpers don't collide with the
// identically-named ones in SibeliusSmokeTestCommandlet.cpp under a unity build.
namespace RefuserSmokeTestNS
{
	const FString DefaultMapPackage = TEXT("/Game/L_RefuserTest");

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogRefuserSmokeTest, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogRefuserSmokeTest, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	FString ParseMapArg(const FString& Params)
	{
		FString MapOverride;
		if (FParse::Value(*Params, TEXT("map="), MapOverride) && !MapOverride.IsEmpty())
		{
			return MapOverride;
		}
		return DefaultMapPackage;
	}
}

using namespace RefuserSmokeTestNS;

URefuserSmokeTestCommandlet::URefuserSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 URefuserSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogRefuserSmokeTest, Error, TEXT("RefuserSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
	return 1;
#else
	const FString MapPackage = ParseMapArg(Params);
	UE_LOG(LogRefuserSmokeTest, Display, TEXT("=== CP3 smoke test: %s ==="), *MapPackage);

	FResult R;

	// 1. The map loads.
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPackage);
	R.Check(World != nullptr, FString::Printf(TEXT("Map loads (%s)"), *MapPackage));
	if (!World)
	{
		UE_LOG(LogRefuserSmokeTest, Error, TEXT("=== CP3 SMOKE TEST FAILED: could not load map. ==="));
		return 1;
	}

	// 2. At least one ARefuserSpawner is present, and its RefuserClass is set.
	int32 SpawnerCount = 0;
	ARefuserSpawner* FirstSpawnerWithClass = nullptr;
	for (TActorIterator<ARefuserSpawner> It(World); It; ++It)
	{
		++SpawnerCount;
		if (It->RefuserClass != nullptr && FirstSpawnerWithClass == nullptr)
		{
			FirstSpawnerWithClass = *It;
		}
	}
	R.Check(SpawnerCount >= 1,
		FString::Printf(TEXT("At least one ARefuserSpawner present (found %d)"), SpawnerCount));
	R.Check(FirstSpawnerWithClass != nullptr,
		TEXT("A spawner has RefuserClass set (not null)"));

	// 3. At least one NavMeshBoundsVolume is present.
	int32 NavVolumeCount = 0;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		++NavVolumeCount;
	}
	R.Check(NavVolumeCount >= 1,
		FString::Printf(TEXT("At least one NavMeshBoundsVolume present (found %d)"), NavVolumeCount));

	// 4. The spawner's RefuserClass has an AIControllerClass that is
	//    ARefuserController (or a subclass) — i.e. the Refuser is wired to chase.
	if (FirstSpawnerWithClass)
	{
		const APawn* PawnCDO = FirstSpawnerWithClass->RefuserClass
			? FirstSpawnerWithClass->RefuserClass->GetDefaultObject<APawn>()
			: nullptr;
		UClass* AIControllerClass = PawnCDO ? PawnCDO->AIControllerClass.Get() : nullptr;
		const bool bChaseWired = AIControllerClass != nullptr
			&& AIControllerClass->IsChildOf(ARefuserController::StaticClass());
		R.Check(bChaseWired,
			FString::Printf(TEXT("RefuserClass AIControllerClass is ARefuserController or subclass (got %s)"),
				AIControllerClass ? *AIControllerClass->GetName() : TEXT("null")));
	}
	else
	{
		// No spawner with a RefuserClass — cannot verify chase wiring; fail it.
		R.Check(false, TEXT("RefuserClass AIControllerClass is ARefuserController or subclass (no RefuserClass to inspect)"));
	}

	if (R.Failures == 0)
	{
		UE_LOG(LogRefuserSmokeTest, Display, TEXT("=== CP3 SMOKE TEST PASSED ==="));
		return 0;
	}

	UE_LOG(LogRefuserSmokeTest, Error, TEXT("=== CP3 SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif // WITH_EDITOR
}
