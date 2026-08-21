#include "CompileSmokeTestCommandlet.h"

#if WITH_EDITOR

#include "BookPickup.h"
#include "BuildSite.h"
#include "CompileEndSubsystem.h"
#include "CompileTypes.h"
#include "HatchLock.h"
#include "InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogCompileSmokeTest, Display, All);

// CP3 lesson #6: named namespace, never anonymous, in sibling commandlet .cpps.
namespace CompileSmokeTestNS
{
	// [ASSET PATH] placeholders - Stage 5 of the runbook swaps in the real paths.
	static const TCHAR* DefaultMapPackage = TEXT("/Game/L_Office_v02");
	static const TCHAR* IMCPath = TEXT("/Game/Input/IMC_Default.IMC_Default");          // [ASSET PATH?]
	static const TCHAR* BuildActionPath = TEXT("/Game/Input/IA_Build.IA_Build");        // [ASSET PATH?]
	// C9: the build site replaced the ATTIC LADDER, not the staircase. NB: despite its
	// name, SM_Stairs_D_1-2-Attic is the main floor-1->2 staircase and MUST stay (lesson
	// banked Jun 4). The removed asset is the ladder, SM_AtticLadder_A.
	static const TCHAR* RemovedStairsMeshName = TEXT("SM_AtticLadder_A");

	static const int32 ActorBandMin = 1000;
	static const int32 ActorBandMax = 1150;
	static const int32 SoftLockSurplus = 2; // C4: pickups >= total cost + surplus

	struct FResult
	{
		int32 Failures = 0;

		void Pass(const FString& Msg)  { UE_LOG(LogCompileSmokeTest, Display, TEXT("  [PASS] %s"), *Msg); }
		void Fail(const FString& Msg)  { ++Failures; UE_LOG(LogCompileSmokeTest, Error, TEXT("  [FAIL] %s"), *Msg); }
		void Check(bool bOk, const FString& Msg, const FString& WhyFailed = FString())
		{
			if (bOk) { Pass(Msg); }
			else     { Fail(WhyFailed.IsEmpty() ? Msg : FString::Printf(TEXT("%s - %s"), *Msg, *WhyFailed)); }
		}
	};

	static FString ParseMapArg(const FString& Params)
	{
		FString MapArg;
		if (FParse::Value(*Params, TEXT("map="), MapArg) && !MapArg.IsEmpty())
		{
			return MapArg;
		}
		return DefaultMapPackage;
	}
}

UCompileSmokeTestCommandlet::UCompileSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCompileSmokeTestCommandlet::Main(const FString& Params)
{
	using namespace CompileSmokeTestNS;

	FResult R;
	const FString MapPackage = ParseMapArg(Params);
	UE_LOG(LogCompileSmokeTest, Display, TEXT("=== Ch3 Compile smoke test: %s ==="), *MapPackage);

	// 1. Map loads.
	UPackage* Package = LoadPackage(nullptr, *MapPackage, LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	R.Check(World != nullptr, FString::Printf(TEXT("Map loads (%s)"), *MapPackage));
	if (!World)
	{
		UE_LOG(LogCompileSmokeTest, Error, TEXT("=== Ch3 SMOKE TEST FAILED (no world) ==="));
		return 1;
	}
	World->WorldType = EWorldType::Editor;
	World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false));

	// 2. Input assets exist (IA_Build + mapped in IMC_Default).
	UObject* BuildAction = FSoftObjectPath(BuildActionPath).TryLoad();
	R.Check(BuildAction != nullptr, TEXT("IA_Build asset loads"), TEXT("check BuildActionPath placeholder"));
	UObject* IMC = FSoftObjectPath(IMCPath).TryLoad();
	R.Check(IMC != nullptr, TEXT("IMC_Default asset loads"), TEXT("check IMCPath placeholder"));
	// (Mapping presence is verified structurally in PIE; headless we assert both assets exist.)

	// 3. Inventory self-test (C3) - transient component, no pawn needed.
	{
		UInventoryComponent* Inv = NewObject<UInventoryComponent>(GetTransientPackage(), TEXT("SmokeInv"));
		FString Err;
		R.Check(Inv->RunInventorySelfTest(Err), TEXT("Inventory round-trip (C3)"), Err);
	}

	// 4. Sites present; per-site self-tests (build/dismantle/nav-link state, C2/C4/C8).
	int32 StairSites = 0, KeySites = 0, TotalCost = 0;
	for (TActorIterator<ABuildSite> It(World); It; ++It)
	{
		ABuildSite* Site = *It;
		TotalCost += Site->Cost;
		if (Site->Output == EBuildOutput::Structure) { ++StairSites; }
		else { ++KeySites; }

		// SIB-27: a consumable site (KeyBuildSite) can't survive RunBuildSelfTest's
		// dismantle leg — it's intentionally never dismantlable — so route it to the
		// consume-on-build ledger instead.
		FString Err;
		const bool bSelfTestOk = (Site->bConsumeOnBuild || Site->Output == EBuildOutput::KeyItem)
			? Site->RunConsumeOnBuildSelfTest(Err)
			: Site->RunBuildSelfTest(Err);
		R.Check(bSelfTestOk,
			FString::Printf(TEXT("BuildSite self-test (%s)"), *Site->GetName()), Err);
		if (Site->Output == EBuildOutput::Structure)
		{
			R.Check(Site->NavLink != nullptr,
				FString::Printf(TEXT("Structure site %s has a NavLink assigned (C2)"), *Site->GetName()));
		}
	}
	R.Check(StairSites >= 1, TEXT("At least one Structure build site present"));
	R.Check(KeySites >= 1, TEXT("At least one KeyItem build site present"));

	// 4b. Consume-on-build ledger (SIB-27 K1–K5) on controlled, spawned instances, so
	// the asserts hold regardless of how the level's KeyBuildSite instance is authored
	// (Walt flips bConsumeOnBuild by hand). Headless: the reveal consumes synchronously,
	// so the terminal Consumed state is observable inline. Reveal *feel* is PIE-only.
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;

		// K1: a default site (bConsumeOnBuild=false) keeps build→dismantle→refund intact.
		if (ABuildSite* StairLike = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SpawnParams))
		{
			R.Check(!StairLike->bConsumeOnBuild, TEXT("bConsumeOnBuild defaults false (K1)"));
			FString Err;
			R.Check(StairLike->RunBuildSelfTest(Err),
				TEXT("Default site build/dismantle/refund intact (K1)"), Err);
			StairLike->Destroy();
		}
		else
		{
			R.Fail(TEXT("Could not spawn a default test BuildSite (K1)"));
		}

		// K2–K6: a consumable KeyItem site reveals→consumes, never dismantles, no
		// double-grant, and a reload presents Consumed without minting a second Key.
		if (ABuildSite* KeyLike = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SpawnParams))
		{
			KeyLike->bConsumeOnBuild = true;
			KeyLike->Output = EBuildOutput::KeyItem;
			KeyLike->CostResource = EResourceType::Book;
			FString Err;
			R.Check(KeyLike->RunConsumeOnBuildSelfTest(Err),
				TEXT("Consume-on-build ledger (K2–K6)"), Err);
			KeyLike->Destroy();
		}
		else
		{
			R.Fail(TEXT("Could not spawn a consumable test BuildSite (K2–K6)"));
		}

		// Walk-up / E: the alcove orb grants the Key without COMPILE.
		if (ABuildSite* WalkUp = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SpawnParams))
		{
			WalkUp->Output = EBuildOutput::KeyItem;
			WalkUp->bConsumeOnBuild = true;
			WalkUp->CostResource = EResourceType::Book;
			WalkUp->Cost = 8;
			UInventoryComponent* Inv = NewObject<UInventoryComponent>(WalkUp, TEXT("WalkUpInv"));
			WalkUp->AddInstanceComponent(Inv);
			Inv->RegisterComponent();
			Inv->Add(EResourceType::Book, 12);
			WalkUp->TryTakeAtticKey(WalkUp);
			R.Check(WalkUp->IsBuilt() || WalkUp->IsConsumed(),
				TEXT("Walk-up attic key consumes the site without COMPILE"));
			R.Check(Inv->GetCount(EResourceType::Key) == 1,
				TEXT("Walk-up attic key grants exactly one Key"));
			R.Check(Inv->GetCount(EResourceType::Book) == 4,
				TEXT("Walk-up attic key spends 8 books (12-8=4)"));
			WalkUp->Destroy();
		}
		else
		{
			R.Fail(TEXT("Could not spawn a walk-up KeyItem BuildSite"));
		}
	}

	// 5. Old attic stairs are gone (C9); actor count in band.
	int32 ActorCount = 0, BookPickups = 0;
	bool bStairsMeshFound = false;
	for (FActorIterator It(World); It; ++It)
	{
		++ActorCount;
		if (const AStaticMeshActor* SMA = Cast<AStaticMeshActor>(*It))
		{
			const UStaticMeshComponent* SMC = SMA->GetStaticMeshComponent();
			if (SMC && SMC->GetStaticMesh() && SMC->GetStaticMesh()->GetName() == RemovedStairsMeshName)
			{
				bStairsMeshFound = true;
			}
		}
		if (Cast<ABookPickup>(*It)) { ++BookPickups; }
	}
	R.Check(!bStairsMeshFound, FString::Printf(TEXT("%s removed from level (C9)"), RemovedStairsMeshName));
	R.Check(ActorCount >= ActorBandMin && ActorCount <= ActorBandMax,
		FString::Printf(TEXT("Actor count in [%d, %d] (found %d)"), ActorBandMin, ActorBandMax, ActorCount));

	// 6. Soft-lock arithmetic (C4): pickups cover every sink + surplus.
	R.Check(BookPickups >= TotalCost + SoftLockSurplus,
		FString::Printf(TEXT("Book pickups (%d) >= total cost (%d) + %d surplus (C4)"),
			BookPickups, TotalCost, SoftLockSurplus));

	// 7. Hatch lock self-test.
	int32 Hatches = 0;
	for (TActorIterator<AHatchLock> It(World); It; ++It)
	{
		++Hatches;
		FString Err;
		R.Check((*It)->RunLockSelfTest(Err),
			FString::Printf(TEXT("HatchLock self-test (%s)"), *(*It)->GetName()), Err);
	}
	R.Check(Hatches >= 1, TEXT("At least one HatchLock present"));

	// 8. Compile end-trigger present (Phase 4): overlapping it fires Ch3 completion
	// via UCompileEndSubsystem. Headless we assert the tagged volume exists, exactly
	// as the CodeVision bar asserts its CodeVisionEndTrigger.
	int32 EndTriggers = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(UCompileEndSubsystem::EndTriggerTag)) { ++EndTriggers; }
	}
	R.Check(EndTriggers >= 1,
		FString::Printf(TEXT("End-trigger actor tagged '%s' present"),
			*UCompileEndSubsystem::EndTriggerTag.ToString()));

	// Tear down the world we hand-initialized at the top. InitWorld() has no implicit
	// counterpart here (the sibling commandlets get theirs free via LoadMap), so without
	// this the engine shuts down holding a still-live orphaned world -> handled ensure
	// -> critical error -> nonzero exit even when every assertion PASSED.
	World->CleanupWorld();

	if (R.Failures == 0)
	{
		UE_LOG(LogCompileSmokeTest, Display, TEXT("=== Ch3 COMPILE SMOKE TEST PASSED ==="));
		return 0;
	}
	UE_LOG(LogCompileSmokeTest, Error, TEXT("=== Ch3 COMPILE SMOKE TEST FAILED (%d) ==="), R.Failures);
	return 1;
}

#endif // WITH_EDITOR
