// ElsewhereSmokeTestCommandlet.cpp — see header. Shell cloned from the Sauce gate
// (LoadPackage host map -> InitWorld -> spawn transient actors -> CleanupWorld).

#include "ElsewhereSmokeTestCommandlet.h"

#include "ElsewhereTypes.h"
#include "ElsewhereGen.h"
#include "ElsewhereSubsystem.h"
#include "ElsewhereSaveGame.h"
#include "ElsewhereBuilder.h"
#include "Curio.h"
#include "ReturnDoor.h"
#include "CabinetOfCuriosities.h"
#include "SauceDoor.h"
#include "SibeliusSaveIO.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogElsewhereSmokeTest, Log, All);

namespace ElsewhereSmokeTestNS
{
	const FString HostMapPackage = TEXT("/Game/Maps/L_AI_Temple");
	const FString SandboxSlot    = TEXT("ElsewhereSmokeSlot");

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
	using namespace ElsewhereSmokeTestNS;

	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("=== THE SAUCE DOOR smoke test: generate -> collect -> cabinet -> return ==="));

	FResult R;

	// --- Content registry (DataTable-or-default, same path the runtime uses). --------
	TArray<FPlaceTypeDef> Places;
	TArray<FCurioDef> Curios;
	UElsewhereSubsystem::LoadRegistry(Places, Curios);

	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 1: content registry (MVP wants >=3 place-types) ---"));
	R.Check(Places.Num() >= 3, FString::Printf(TEXT("at least 3 place-types (%d)"), Places.Num()));
	R.Check(Curios.Num() >= 3, FString::Printf(TEXT("at least 3 curios (%d)"), Curios.Num()));
	{
		bool bAllPoolsResolve = true;
		for (const FPlaceTypeDef& P : Places)
		{
			if (P.CurioPool.Num() == 0) { bAllPoolsResolve = false; break; }
			for (const FName& Id : P.CurioPool)
			{
				if (!FElsewhereGen::FindCurio(Curios, Id)) { bAllPoolsResolve = false; break; }
			}
		}
		R.Check(bAllPoolsResolve, TEXT("every place has a non-empty pool of resolvable curios"));

		bool bHasCommon = false, bHasRare = false, bHasLegendary = false;
		for (const FCurioDef& C : Curios)
		{
			bHasCommon    |= (C.Rarity == EElsewhereRarity::Common);
			bHasRare      |= (C.Rarity == EElsewhereRarity::Rare);
			bHasLegendary |= (C.Rarity == EElsewhereRarity::Legendary);
		}
		R.Check(bHasCommon && bHasRare && bHasLegendary, TEXT("all three rarity tiers exist (wonder bonus, not loss)"));
	}

	// --- Determinism + variety + fit (the generation guarantee). ---------------------
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 2: deterministic, varied, curio-fits-place ---"));
	{
		bool bDeterministic = true;
		bool bAllFit = true;
		TSet<FName> SeenPlaces;
		TSet<FName> SeenCurios;
		for (int32 Seed = 1; Seed <= 200; ++Seed)
		{
			const FElsewherePlan A = FElsewhereGen::RollPlan(Seed, Places, Curios);
			const FElsewherePlan B = FElsewhereGen::RollPlan(Seed, Places, Curios);
			if (A.PlaceTypeId != B.PlaceTypeId || A.CurioId != B.CurioId ||
				A.LayoutSeed != B.LayoutSeed || A.MoodSeed != B.MoodSeed)
			{
				bDeterministic = false;
			}
			// the rolled curio must belong to the rolled place's pool (§6 fit)
			if (const FPlaceTypeDef* P = FElsewhereGen::FindPlace(Places, A.PlaceTypeId))
			{
				if (!P->CurioPool.Contains(A.CurioId)) { bAllFit = false; }
			}
			else { bAllFit = false; }
			SeenPlaces.Add(A.PlaceTypeId);
			SeenCurios.Add(A.CurioId);
		}
		R.Check(bDeterministic, TEXT("RollPlan(seed) is deterministic (same seed -> same plan) over 200 seeds"));
		R.Check(bAllFit, TEXT("every rolled curio fits its place-type's pool"));
		R.Check(SeenPlaces.Num() >= 3, FString::Printf(TEXT("variety: >=3 distinct place-types appear over 200 seeds (%d)"), SeenPlaces.Num()));
		R.Check(SeenCurios.Num() >= 6, FString::Printf(TEXT("variety: many distinct curios appear (%d)"), SeenCurios.Num()));
	}

	// --- Collection logic: always score, dupes don't refill a slot (§6). -------------
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 3: collection scoring + dedupe ---"));
	{
		FCurioCollection Coll;
		const FCurioDef* Common = Curios.FindByPredicate([](const FCurioDef& C){ return C.Rarity == EElsewhereRarity::Common; });
		const FCurioDef* Legend = Curios.FindByPredicate([](const FCurioDef& C){ return C.Rarity == EElsewhereRarity::Legendary; });
		R.Check(Common && Legend, TEXT("found a Common + a Legendary curio to test scoring"));
		if (Common && Legend)
		{
			const bool bNew1 = Coll.Add(*Common, TEXT("P"));
			R.Check(bNew1 && Coll.Owned.Num() == 1 && Coll.TotalCollected == 1, TEXT("first collect: new slot, total=1"));
			const int32 ScoreAfterCommon = Coll.Score;
			R.Check(ScoreAfterCommon == FCurioCollection::RarityScore(EElsewhereRarity::Common), TEXT("Common scored by its rarity"));

			const bool bNew2 = Coll.Add(*Common, TEXT("P"));
			R.Check(!bNew2 && Coll.Owned.Num() == 1 && Coll.TotalCollected == 2, TEXT("duplicate: no new slot, total climbs"));
			R.Check(Coll.Score > ScoreAfterCommon, TEXT("duplicate still scores (go-again stays rewarding)"));

			Coll.Add(*Legend, TEXT("P"));
			R.Check(Coll.Owned.Num() == 2, TEXT("Legendary fills a new slot"));
			R.Check(FCurioCollection::RarityScore(EElsewhereRarity::Legendary) > FCurioCollection::RarityScore(EElsewhereRarity::Common),
				TEXT("Legendary worth more than Common (rarity = bonus)"));
		}
	}

	// --- Persistence round-trip + the DISCARD rule (§3/§4). --------------------------
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 4: save/load curios+score; room is NOT saved ---"));
	{
		FSibeliusSaveIO::Delete(SandboxSlot);   // clean start

		// Build a collection, "stage" a room plan alongside it.
		FCurioCollection Coll;
		for (const FCurioDef& C : Curios) { Coll.Add(C, TEXT("P")); }   // own everything
		const int32 ExpectScore = Coll.Score;
		const int32 ExpectUnique = Coll.Owned.Num();
		const int32 ExpectTotal = Coll.TotalCollected;
		const FElsewherePlan StagedRoom = FElsewhereGen::RollPlan(12345, Places, Curios);
		R.Check(StagedRoom.IsValid(), TEXT("a room is staged at save time"));

		UElsewhereSaveGame* Save = NewObject<UElsewhereSaveGame>();
		Save->SaveVersion = UElsewhereSaveGame::CurrentSaveVersion;
		Save->Collection = Coll;
		R.Check(FSibeliusSaveIO::Commit(Save, SandboxSlot), TEXT("CommitSave succeeds"));

		UElsewhereSaveGame* Loaded = Cast<UElsewhereSaveGame>(FSibeliusSaveIO::Load(SandboxSlot));
		R.Check(Loaded != nullptr, TEXT("LoadSave returns a UElsewhereSaveGame"));
		if (Loaded)
		{
			R.Check(Loaded->Collection.Score == ExpectScore, TEXT("score round-trips"));
			R.Check(Loaded->Collection.Owned.Num() == ExpectUnique, TEXT("owned curios round-trip"));
			R.Check(Loaded->Collection.TotalCollected == ExpectTotal, TEXT("total collected round-trips"));
			// The discard rule, structurally: the save type carries Collection + version
			// ONLY — there is no field that can hold the staged room. Even though a room
			// was staged at save time, nothing about it persists.
			R.Check(Loaded->SaveVersion == UElsewhereSaveGame::CurrentSaveVersion, TEXT("save version stamped"));
		}
		FSibeliusSaveIO::Delete(SandboxSlot);
		R.Check(!FSibeliusSaveIO::Has(SandboxSlot), TEXT("sandbox slot cleaned up"));
	}

	// --- World-dependent asserts: spawn the actors in a hand-init world. -------------
	UPackage* Package = LoadPackage(nullptr, *HostMapPackage, LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	R.Check(World != nullptr, FString::Printf(TEXT("host map loads (%s)"), *HostMapPackage));
	if (!World)
	{
		UE_LOG(LogElsewhereSmokeTest, Error, TEXT("=== ELSEWHERE SMOKE TEST FAILED: no world. ==="));
		return 1;
	}
	World->WorldType = EWorldType::Editor;
	World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false));

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;

	// --- Builder: assembles a deterministic room, the one curio, a return door. ------
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 5: builder spawns curio + return door, deterministic ---"));
	{
		const FElsewherePlan Plan = FElsewhereGen::RollPlan(777, Places, Curios);
		AElsewhereBuilder* B1 = World->SpawnActor<AElsewhereBuilder>(AElsewhereBuilder::StaticClass(), SpawnParams);
		AElsewhereBuilder* B2 = World->SpawnActor<AElsewhereBuilder>(AElsewhereBuilder::StaticClass(), SpawnParams);
		R.Check(B1 && B2, TEXT("AElsewhereBuilder spawns"));
		if (B1 && B2)
		{
			const int32 Props1 = B1->BuildFromPlan(Plan, Places, Curios);
			const int32 Props2 = B2->BuildFromPlan(Plan, Places, Curios);
			R.Check(Props1 > 0, FString::Printf(TEXT("builder scatters props (%d)"), Props1));
			R.Check(Props1 == Props2, TEXT("same plan -> same prop count (room reproducible from seed)"));
			R.Check(B1->GetSpawnedCurio() != nullptr, TEXT("exactly one curio spawned"));
			R.Check(B1->GetSpawnedCurio() && B1->GetSpawnedCurio()->CurioId == Plan.CurioId,
				TEXT("spawned curio matches the plan's curio id"));
			R.Check(B1->GetSpawnedReturnDoor() != nullptr, TEXT("a return door is spawned (always a way home)"));
		}

		// The dressed place-type specifically (Server Cathedral / Crebotoly palette):
		// it must build a real room deterministically even though the kit bytes aren't
		// installed here (palette soft-refs fail to load -> engine-shape fallback).
		R.Check(FElsewhereGen::FindPlace(Places, TEXT("ServerCathedral")) != nullptr,
			TEXT("Server Cathedral place-type exists (the first dressed place)"));
		{
			FElsewherePlan Cath;
			Cath.Seed = 4242;
			Cath.PlaceTypeId = TEXT("ServerCathedral");
			Cath.CurioId = TEXT("KernelRelic");
			Cath.LayoutSeed = 4242;
			Cath.MoodSeed = 99;
			AElsewhereBuilder* C1 = World->SpawnActor<AElsewhereBuilder>(AElsewhereBuilder::StaticClass(), SpawnParams);
			AElsewhereBuilder* C2 = World->SpawnActor<AElsewhereBuilder>(AElsewhereBuilder::StaticClass(), SpawnParams);
			if (C1 && C2)
			{
				const int32 CathProps1 = C1->BuildFromPlan(Cath, Places, Curios);
				const int32 CathProps2 = C2->BuildFromPlan(Cath, Places, Curios);
				R.Check(CathProps1 > 0 && CathProps1 == CathProps2,
					FString::Printf(TEXT("Server Cathedral builds deterministically (kit meshes when installed, else fallback) (%d props)"), CathProps1));
				R.Check(C1->GetSpawnedCurio() && C1->GetSpawnedCurio()->CurioId == TEXT("KernelRelic"),
					TEXT("Server Cathedral spawns its curio"));
				R.Check(C1->GetSpawnedReturnDoor() != nullptr, TEXT("Server Cathedral spawns a return door"));
			}
		}
	}

	// --- Cabinet: fills as the owned set grows. --------------------------------------
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 6: cabinet fill reflects the collection ---"));
	{
		ACabinetOfCuriosities* Cabinet = World->SpawnActor<ACabinetOfCuriosities>(ACabinetOfCuriosities::StaticClass(), SpawnParams);
		R.Check(Cabinet != nullptr, TEXT("ACabinetOfCuriosities spawns"));
		if (Cabinet)
		{
			TArray<FName> AllIds;
			for (const FCurioDef& C : Curios) { AllIds.Add(C.Id); }

			const FCabinetState Empty = Cabinet->RefreshFrom(AllIds, {}, 0);
			R.Check(Empty.Filled == 0 && Empty.Total == AllIds.Num(), TEXT("empty cabinet: 0 filled, all slots present"));

			TArray<FName> OwnedHalf;
			for (int32 i = 0; i < AllIds.Num(); i += 2) { OwnedHalf.Add(AllIds[i]); }
			const FCabinetState Half = Cabinet->RefreshFrom(AllIds, OwnedHalf, 42);
			R.Check(Half.Filled == OwnedHalf.Num() && Half.Total == AllIds.Num(),
				FString::Printf(TEXT("partial cabinet: %d/%d filled"), Half.Filled, Half.Total));

			const FCabinetState Full = Cabinet->RefreshFrom(AllIds, AllIds, 999);
			R.Check(Full.Filled == Full.Total, TEXT("full cabinet: every slot filled"));
		}
	}

	// --- Sauce Door arm gate + class resolution. -------------------------------------
	UE_LOG(LogElsewhereSmokeTest, Display, TEXT("--- ASSERT 7: sauce door arm gate + classes resolve ---"));
	{
		ASauceDoor* Door = World->SpawnActor<ASauceDoor>(ASauceDoor::StaticClass(), SpawnParams);
		R.Check(Door != nullptr, TEXT("ASauceDoor spawns"));
		R.Check(Door && Door->RunArmGateSelfTest(), TEXT("unarmed = no prompt + blocks; armed = prompt + passable + visible"));
		R.Check(
			ACurio::StaticClass() && AReturnDoor::StaticClass() && AElsewhereBuilder::StaticClass() &&
			ACabinetOfCuriosities::StaticClass() && ASauceDoor::StaticClass(),
			TEXT("all Sauce Door classes resolve"));
	}

	World->CleanupWorld();

	if (R.Failures == 0)
	{
		UE_LOG(LogElsewhereSmokeTest, Display, TEXT("=== ELSEWHERE SMOKE TEST PASSED (the wonder loop holds headless). ==="));
		return 0;
	}
	UE_LOG(LogElsewhereSmokeTest, Error, TEXT("=== ELSEWHERE SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
#endif // WITH_EDITOR
}
