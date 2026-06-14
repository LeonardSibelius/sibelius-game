// SauceSmokeTestCommandlet.cpp — World Three P0 headless gate for ASauceCauldron + ABookRain.
//
// Bar (spec SS1/SS8/SS9):
//   ASSERT 1 — cauldron completion is one-shot + idempotent, and OnSauceComplete fires
//              EXACTLY once (SS9).
//   ASSERT 2 — book-rain pool builds to PoolSize on BeginPlay, starts empty, and the active
//              count NEVER exceeds PoolSize under tick — recycle, never spawn/destroy (SS1).
//   ASSERT 3 — both classes resolve and are spawnable.
//   (Placement in L_AI_Temple + the actual rain/feed visuals are a PIE check — SS8.)
//
// Family shell cloned from UCompileSmokeTestCommandlet (the sibling that hand-initialises a
// world): LoadPackage host map -> InitWorld -> spawn transient actors -> CleanupWorld on exit
// (without the cleanup the engine shuts down holding a live orphaned world -> nonzero exit even
// when every assertion PASSED). NAMED namespace + function-scoped `using` (unity-build safe);
// no variable shadowing (warnings-as-errors). Run with the editor CLOSED (port 3000).

#include "SauceSmokeTestCommandlet.h"
#include "SauceCauldron.h"
#include "BookRain.h"

#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogSauceSmokeTest, Log, All);

namespace SauceSmokeTestNS
{
	// Host world only — the gate spawns its own transient actors and asserts nothing about the
	// map's contents (SS8: placement is PIE-verified, not headless). L_AI_Temple is the world-
	// three map, so coupling the gate to its load is intentional.
	const FString HostMapPackage = TEXT("/Game/Maps/L_AI_Temple");
	const TCHAR* CubeMeshPath    = TEXT("/Engine/BasicShapes/Cube.Cube");

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogSauceSmokeTest, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogSauceSmokeTest, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};
}

USauceSmokeTestCommandlet::USauceSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 USauceSmokeTestCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogSauceSmokeTest, Error, TEXT("SauceSmokeTest requires an editor build. Use UnrealEditor-Cmd.exe."));
	return 1;
#else
	// Function-scoped so the namespace doesn't leak into other TUs under unity build.
	using namespace SauceSmokeTestNS;

	UE_LOG(LogSauceSmokeTest, Display, TEXT("=== World3 P0 Sauce smoke test: cauldron + book-rain ==="));

	FResult R;

	// --- Host world (clone of the Compile gate's hand-init pattern). -------------------
	UPackage* Package = LoadPackage(nullptr, *HostMapPackage, LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	R.Check(World != nullptr, FString::Printf(TEXT("Host map loads (%s)"), *HostMapPackage));
	if (!World)
	{
		UE_LOG(LogSauceSmokeTest, Error, TEXT("=== SAUCE SMOKE TEST FAILED: no world. ==="));
		return 1;
	}
	World->WorldType = EWorldType::Editor;
	World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false));

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;

	// =====================================================================
	//  ASSERT 1 — cauldron completion: one-shot, idempotent, fires once (SS9).
	// =====================================================================
	UE_LOG(LogSauceSmokeTest, Display, TEXT("--- ASSERT 1: cauldron one-shot completion (SS9) ---"));
	ASauceCauldron* Cauldron = World->SpawnActor<ASauceCauldron>(ASauceCauldron::StaticClass(), SpawnParams);
	R.Check(Cauldron != nullptr, TEXT("ASauceCauldron spawns"));
	if (Cauldron)
	{
		Cauldron->CompleteThreshold = 1.f;

		USauceCompleteCounter* Counter = NewObject<USauceCompleteCounter>(GetTransientPackage());
		Cauldron->OnSauceComplete.AddDynamic(Counter, &USauceCompleteCounter::HandleSauceComplete);

		const bool bR1 = Cauldron->FeedSauce(0.5f);
		R.Check(!bR1 && !Cauldron->IsComplete(), TEXT("FeedSauce(0.5) below threshold -> false, not complete"));

		const bool bR2 = Cauldron->FeedSauce(0.6f);
		R.Check(bR2 && Cauldron->IsComplete(), TEXT("FeedSauce crossing threshold -> true, IsComplete()"));
		R.Check(Counter->Count == 1, TEXT("OnSauceComplete broadcast exactly once at completion"));

		const bool bR3 = Cauldron->FeedSauce(0.6f);
		R.Check(!bR3, TEXT("FeedSauce after completion -> false (idempotent)"));
		R.Check(Counter->Count == 1, TEXT("no second broadcast after completion (one-shot latch)"));
		R.Check(Cauldron->BlendProgress <= 1.f + KINDA_SMALL_NUMBER, TEXT("BlendProgress clamped to <= 1"));
	}

	// =====================================================================
	//  ASSERT 2 — book-rain pool builds + caps; recycle, never destroy (SS1).
	// =====================================================================
	UE_LOG(LogSauceSmokeTest, Display, TEXT("--- ASSERT 2: book-rain pool + cap (SS1) ---"));
	ABookRain* Rain = World->SpawnActor<ABookRain>(ABookRain::StaticClass(), SpawnParams);
	R.Check(Rain != nullptr, TEXT("ABookRain spawns"));
	if (Rain)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubeMeshPath);
		R.Check(Cube != nullptr, FString::Printf(TEXT("engine cube mesh resolves as a book fixture (%s)"), CubeMeshPath));

		const int32 TestPool = 8;
		TArray<FVector> Sources;
		Sources.Add(FVector(0.f, 0.f, 500.f));
		TArray<TObjectPtr<UStaticMesh>> Meshes;
		if (Cube)
		{
			Meshes.Add(Cube);
		}
		Rain->ConfigureForTest(TestPool, Sources, FVector::ZeroVector, Meshes);

		// BeginPlay builds the pool once (SS1).
		Rain->DispatchBeginPlay();
		R.Check(Rain->GetPoolNumForTest() == TestPool,
			FString::Printf(TEXT("pool built to PoolSize after BeginPlay (%d/%d)"), Rain->GetPoolNumForTest(), TestPool));
		R.Check(Rain->GetActiveBookCountForTest() == 0, TEXT("no book active at t0"));

		// Tick well past FallDuration; active count must NEVER exceed PoolSize and the pool
		// must never grow (recycle, never spawn/destroy).
		bool bCapHeld = true;
		bool bPoolStable = true;
		int32 MaxActive = 0;
		for (int32 i = 0; i < 40; ++i)
		{
			Rain->Tick(0.2f);
			const int32 Active = Rain->GetActiveBookCountForTest();
			MaxActive = FMath::Max(MaxActive, Active);
			if (Active > TestPool)                  { bCapHeld = false; }
			if (Rain->GetPoolNumForTest() != TestPool) { bPoolStable = false; }
		}
		R.Check(bCapHeld,
			FString::Printf(TEXT("active book count never exceeds PoolSize under tick (max %d/%d) — SS1"), MaxActive, TestPool));
		R.Check(bPoolStable, TEXT("pool size constant under tick (recycle, never spawn/destroy) — SS1"));
		R.Check(MaxActive > 0, TEXT("at least one book became active under tick (rain actually runs)"));
	}

	// =====================================================================
	//  ASSERT 3 — classes resolve + spawnable.
	// =====================================================================
	UE_LOG(LogSauceSmokeTest, Display, TEXT("--- ASSERT 3: classes resolve ---"));
	R.Check(ASauceCauldron::StaticClass() != nullptr && ABookRain::StaticClass() != nullptr,
		TEXT("ASauceCauldron + ABookRain StaticClass() resolve"));

	// Tear down the hand-initialised world (the exit-3 lesson: no implicit counterpart to InitWorld).
	World->CleanupWorld();

	if (R.Failures == 0)
	{
		UE_LOG(LogSauceSmokeTest, Display, TEXT("=== SAUCE SMOKE TEST PASSED (World3 P0 — cauldron + book-rain stubs green). ==="));
		return 0;
	}

	UE_LOG(LogSauceSmokeTest, Error, TEXT("=== SAUCE SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif // WITH_EDITOR
}
