// GenerateSmokeTestCommandlet.cpp
//
// SIB-30 — Ch6 Generate SPIKE gate. One assert block per G-code:
//
// G1 — scope trap: only a catalog EntryId can ever resolve; gibberish never does.
// G2 — varied phrasing: ~3 phrasings per item resolve to the right entry; gibberish
//      -> RefusedNoMatch; a genuine keyword tie -> RefusedAmbiguous (DECISION DB).
// G3 — content safety: out-of-catalog / "unsafe" phrasings never resolve.
// G4 — offline/deterministic: same (input, catalog, budget) -> identical result; the
//      matcher uses no network / RNG / clock (pure function).
// G5 — budget economy: a resolve costs its Cost; budget < Cost -> RefusedOverBudget
//      (distinct from RefusedNoMatch).
// G6 — persistence via the REAL pipeline: a resolved entry, spawned as an ABuildSite,
//      built with the Ch3 Build() verb, gets a stable FGuid and round-trips a deploy
//      save -> reset -> ApplyDeployedSave (the Ch5 path) back to its built state.
//
// CP3 lesson #6: NAMED namespace to avoid unity-build redefinition collisions.

#include "GenerateSmokeTestCommandlet.h"
#include "GenerateMatcher.h"
#include "GenerateTypes.h"
#include "GenerateCatalog.h"        // load the real CSV-backed catalog
#include "MrsHallLines.h"           // P2: DC blocklist + data-driven refusal lines

#include "BuildSite.h"
#include "InventoryComponent.h"
#include "BranchSubsystem.h"
#include "SibeliusSaveIO.h"
#include "SibeliusSaveGame.h"       // P3: inspect the written save (EntryId/transform/budget)
#include "GenerateComponent.h"      // P3: AuthorGeneratedSite + UGenerateComponent budget
#include "BranchTypes.h"            // P3: FBranchObjectState
#include "CompileTypes.h"           // EResourceType

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"        // GetTransientPackage

#if WITH_EDITOR
#include "FileHelpers.h"            // UEditorLoadingAndSavingUtils::LoadMap
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGenerateSmoke, Log, All);

namespace GenerateSmokeTestNS
{
	const FString DefaultMapPackage = TEXT("/Game/L_Office_v02");

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGenerateSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGenerateSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	const TCHAR* OutcomeStr(EGenerateOutcome O)
	{
		switch (O)
		{
		case EGenerateOutcome::Resolved:          return TEXT("Resolved");
		case EGenerateOutcome::RefusedNoMatch:    return TEXT("RefusedNoMatch");
		case EGenerateOutcome::RefusedAmbiguous:  return TEXT("RefusedAmbiguous");
		case EGenerateOutcome::RefusedOverBudget: return TEXT("RefusedOverBudget");
		case EGenerateOutcome::RefusedUnsafe:     return TEXT("RefusedUnsafe");
		default:                                  return TEXT("?");
		}
	}

	bool CatalogHas(const TArray<FGenerateCatalogEntry>& Catalog, const FName& Id)
	{
		return Catalog.ContainsByPredicate([&Id](const FGenerateCatalogEntry& E) { return E.EntryId == Id; });
	}

	// P3 helpers: locate/count live ABuildSites by their stable GUID (re-spawn assertions).
	int32 CountBuildSitesByGuid(UWorld* World, const FGuid& Id)
	{
		int32 N = 0;
		for (TActorIterator<ABuildSite> It(World); It; ++It)
		{
			if (It->GetBranchId() == Id) { ++N; }
		}
		return N;
	}

	ABuildSite* FindBuildSiteByGuid(UWorld* World, const FGuid& Id)
	{
		for (TActorIterator<ABuildSite> It(World); It; ++It)
		{
			if (It->GetBranchId() == Id) { return *It; }
		}
		return nullptr;
	}
}

UGenerateSmokeTestCommandlet::UGenerateSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UGenerateSmokeTestCommandlet::Main(const FString& Params)
{
	using namespace GenerateSmokeTestNS;

	UE_LOG(LogGenerateSmoke, Display, TEXT("=== SIB-30 Generate smoke test (Ch6 P2): G1-G7 against the REAL catalog + Mrs. Hall data ==="));

	FResult R;

	// Load the curated catalog from the committed CSV (the real thing, not a fixture).
	TArray<FGenerateCatalogEntry> Catalog;
	FString LoadErr;
	const bool bLoaded = LoadGenerateCatalog(Catalog, LoadErr);
	R.Check(bLoaded, FString::Printf(TEXT("P0: catalog loads from %s"), *GetGenerateCatalogCsvPath()));
	R.Check(Catalog.Num() >= 4, FString::Printf(TEXT("P0: catalog has >= 4 entries (%d loaded)"), Catalog.Num()));
	if (!bLoaded)
	{
		UE_LOG(LogGenerateSmoke, Error, TEXT("  catalog load error: %s"), *LoadErr);
	}
	else
	{
		FString Ids;
		for (const FGenerateCatalogEntry& E : Catalog) { Ids += E.EntryId.ToString() + TEXT(" "); }
		UE_LOG(LogGenerateSmoke, Display, TEXT("  catalog entries: %s"), *Ids);
	}

	// =====================================================================
	//  G2 — varied phrasing (the core risk). ~3 phrasings per item resolve to
	//  the right entry; gibberish -> NoMatch; a real keyword tie -> Ambiguous.
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G2: varied phrasing ---"));
	struct FPhrasing { const TCHAR* Input; const TCHAR* Expected; };
	const FPhrasing PhraseTable[] = {
		{ TEXT("a tree"),             TEXT("tree") },
		{ TEXT("tall oak"),           TEXT("tree") },
		{ TEXT("leaves and trunk"),   TEXT("tree") },
		{ TEXT("a potted plant"),     TEXT("plant") },
		{ TEXT("small shrub"),        TEXT("plant") },
		{ TEXT("a fern"),             TEXT("plant") },
		{ TEXT("a lamp"),             TEXT("lamp") },
		{ TEXT("some light"),         TEXT("lamp") },
		{ TEXT("an old lantern"),     TEXT("lamp") },
		{ TEXT("a crate"),            TEXT("crate") },
		{ TEXT("a wooden box"),       TEXT("crate") },
		{ TEXT("storage container"),  TEXT("crate") },
		{ TEXT("a chair"),            TEXT("chair") },
		{ TEXT("a seat"),             TEXT("chair") },
		{ TEXT("a wooden stool"),     TEXT("chair") },
		{ TEXT("a key"),              TEXT("key") },
		{ TEXT("a brass key"),        TEXT("key") },
		{ TEXT("unlock the door"),    TEXT("key") },
		// P4: each new keyword in the grown vocabulary resolves (never refuses) to its entry.
		{ TEXT("a pine"),             TEXT("tree") },
		{ TEXT("a potted flower"),    TEXT("plant") },
		{ TEXT("a light bulb"),       TEXT("lamp") },
		{ TEXT("a barrel"),           TEXT("crate") },
		{ TEXT("a sofa"),             TEXT("chair") },
		{ TEXT("a keycard"),          TEXT("key") },
	};

	int32 PhraseHits = 0;
	const int32 PhraseTotal = UE_ARRAY_COUNT(PhraseTable);
	for (const FPhrasing& P : PhraseTable)
	{
		const FGenerateResolution Got = ClassifyGenerateRequest(P.Input, Catalog, /*budget*/ 99);
		const bool bOk = (Got.Outcome == EGenerateOutcome::Resolved) && (Got.EntryId == FName(P.Expected));
		if (bOk) { ++PhraseHits; }
		else
		{
			UE_LOG(LogGenerateSmoke, Warning, TEXT("  G2 MISS: \"%s\" -> %s(%s), expected Resolved(%s)"),
				P.Input, OutcomeStr(Got.Outcome), *Got.EntryId.ToString(), P.Expected);
		}
	}
	R.Check(PhraseHits == PhraseTotal,
		FString::Printf(TEXT("G2: all %d phrasings resolved to the expected entry (%d/%d)"), PhraseTotal, PhraseHits, PhraseTotal));

	// Gibberish -> RefusedNoMatch.
	const TCHAR* Gibberish[] = { TEXT("xyzzy"), TEXT("a unicorn"), TEXT("blorp the frobnitz"), TEXT("") };
	bool bAllGibberishRefused = true;
	for (const TCHAR* G : Gibberish)
	{
		if (ClassifyGenerateRequest(G, Catalog, 99).Outcome != EGenerateOutcome::RefusedNoMatch)
		{
			bAllGibberishRefused = false;
		}
	}
	R.Check(bAllGibberishRefused, TEXT("G2: gibberish/empty -> RefusedNoMatch (never a wrong spawn)"));

	// Genuine keyword tie -> RefusedAmbiguous (DECISION DB).
	{
		const FGenerateResolution Tie = ClassifyGenerateRequest(TEXT("light box"), Catalog, 99); // light(lamp)+box(crate)
		R.Check(Tie.Outcome == EGenerateOutcome::RefusedAmbiguous, TEXT("G2/DB: keyword tie ('light box') -> RefusedAmbiguous"));
	}

	// =====================================================================
	//  G1 — scope trap: every Resolved result is a real catalog EntryId; nothing
	//  outside the catalog can ever resolve. (The only spawn path is a resolved id.)
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G1: closed-catalog scope ---"));
	const TCHAR* AllInputs[] = {
		TEXT("a lamp"), TEXT("a tree"), TEXT("a brass key"), TEXT("light box"),
		TEXT("xyzzy"), TEXT("a unicorn"), TEXT("a gun"), TEXT(""), TEXT("conjure a dragon")
	};
	bool bScopeHolds = true;
	for (const TCHAR* In : AllInputs)
	{
		const FGenerateResolution Got = ClassifyGenerateRequest(In, Catalog, 99);
		if (Got.Outcome == EGenerateOutcome::Resolved && !CatalogHas(Catalog, Got.EntryId))
		{
			bScopeHolds = false; // resolved to something NOT in the catalog — impossible by design
			UE_LOG(LogGenerateSmoke, Error, TEXT("  G1 BREACH: \"%s\" resolved to non-catalog id %s"), In, *Got.EntryId.ToString());
		}
	}
	R.Check(bScopeHolds, TEXT("G1: no input ever resolves to a non-catalog EntryId (closed by construction)"));

	// =====================================================================
	//  G3 — content safety: out-of-catalog / "unsafe" phrasings never resolve.
	//  (DECISION DC: the tone blocklist -> RefusedUnsafe is now implemented in P2 and
	//  exercised in G7 below; here, with no blocklist passed, these still never resolve.)
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G3: content safety ---"));
	const TCHAR* UnsafeIsh[] = { TEXT("a gun"), TEXT("set it on fire"), TEXT("poison"), TEXT("conjure a dragon") };
	bool bNoUnsafeSpawn = true;
	for (const TCHAR* U : UnsafeIsh)
	{
		if (ClassifyGenerateRequest(U, Catalog, 99).Outcome == EGenerateOutcome::Resolved)
		{
			bNoUnsafeSpawn = false;
		}
	}
	R.Check(bNoUnsafeSpawn, TEXT("G3: out-of-catalog / unsafe phrasings never resolve to a spawn"));

	// =====================================================================
	//  G4 — offline / deterministic: same inputs -> identical results.
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G4: deterministic ---"));
	const FGenerateResolution A1 = ClassifyGenerateRequest(TEXT("a brass key"), Catalog, 99);
	const FGenerateResolution A2 = ClassifyGenerateRequest(TEXT("a brass key"), Catalog, 99);
	R.Check(A1.Outcome == A2.Outcome && A1.EntryId == A2.EntryId && A1.Cost == A2.Cost,
		TEXT("G4: ClassifyGenerateRequest is deterministic (identical result on repeat)"));
	R.Check(TokenizeGenerateInput(TEXT("A Brass KEY!")) == TokenizeGenerateInput(TEXT("a brass key")),
		TEXT("G4: tokenizer is case/punctuation-stable (no RNG/clock)"));

	// =====================================================================
	//  G5 — budget economy: resolve costs Cost; budget < Cost -> RefusedOverBudget
	//  (distinct from RefusedNoMatch).
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G5: budget economy ---"));
	int32 Budget = 5;
	const FGenerateResolution L = ClassifyGenerateRequest(TEXT("a lamp"), Catalog, Budget); // cost 1
	R.Check(L.Outcome == EGenerateOutcome::Resolved && L.Cost == 1, TEXT("G5: 'a lamp' resolves with cost 1"));
	Budget -= L.Cost; // 5 -> 4 (the per-area economy decrements on spawn)
	R.Check(Budget == 4, TEXT("G5: budget decremented by the entry's cost"));

	const FGenerateResolution KeyOk = ClassifyGenerateRequest(TEXT("brass key"), Catalog, 3); // cost 3, budget 3
	R.Check(KeyOk.Outcome == EGenerateOutcome::Resolved, TEXT("G5: cost == remaining budget still resolves"));
	const FGenerateResolution KeyBroke = ClassifyGenerateRequest(TEXT("brass key"), Catalog, 2); // cost 3 > budget 2
	R.Check(KeyBroke.Outcome == EGenerateOutcome::RefusedOverBudget,
		TEXT("G5: cost > remaining budget -> RefusedOverBudget (distinct from NoMatch)"));
	R.Check(KeyBroke.Outcome != EGenerateOutcome::RefusedNoMatch, TEXT("G5: over-budget is NOT conflated with no-match"));

	// =====================================================================
	//  G7 — P2: DC unsafe-word blocklist + data-driven Mrs. Hall refusal lines.
	//  A blocklisted phrase -> RefusedUnsafe (never a spawn); every refusal reason
	//  yields a non-empty line from data; line selection stays deterministic.
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G7: DC blocklist + Mrs. Hall lines (P2) ---"));

	TArray<FString> Blocklist;
	FString BlockErr;
	const bool bBlock = LoadGenerateBlocklist(Blocklist, BlockErr);
	R.Check(bBlock && Blocklist.Num() > 0, FString::Printf(TEXT("G7: blocklist loads from data (%d words)"), Blocklist.Num()));
	if (!bBlock)
	{
		UE_LOG(LogGenerateSmoke, Error, TEXT("  blocklist load error: %s"), *BlockErr);
	}

	// A blocklisted phrase -> RefusedUnsafe, and NEVER a spawn.
	const FGenerateResolution Unsafe = ClassifyGenerateRequest(TEXT("a gun"), Catalog, 99, Blocklist);
	R.Check(Unsafe.Outcome == EGenerateOutcome::RefusedUnsafe, TEXT("G7: blocklisted 'a gun' -> RefusedUnsafe"));
	R.Check(Unsafe.Outcome != EGenerateOutcome::Resolved, TEXT("G7: a blocklisted phrase never resolves to a spawn"));

	// The unsafe veto wins even when a real catalog keyword is also present.
	const FGenerateResolution UnsafeMix = ClassifyGenerateRequest(TEXT("a lamp and a gun"), Catalog, 99, Blocklist);
	R.Check(UnsafeMix.Outcome == EGenerateOutcome::RefusedUnsafe, TEXT("G7: an unsafe word vetoes an otherwise-matchable request"));

	// Proof the veto is the blocklist, not the catalog: same input, no blocklist -> not unsafe,
	// and still never a spawn (closed catalog).
	const FGenerateResolution NoList = ClassifyGenerateRequest(TEXT("a gun"), Catalog, 99);
	R.Check(NoList.Outcome != EGenerateOutcome::RefusedUnsafe, TEXT("G7: without the blocklist 'a gun' is NOT flagged unsafe"));
	R.Check(NoList.Outcome != EGenerateOutcome::Resolved, TEXT("G7: 'a gun' never spawns, with or without the blocklist"));

	// Every refusal reason yields a non-empty line from the data table.
	TMap<EGenerateOutcome, TArray<FMrsHallLine>> Lines;
	FString LinesErr;
	const bool bLines = LoadMrsHallLines(Lines, LinesErr);
	R.Check(bLines, TEXT("G7: Mrs. Hall lines load from data"));
	if (!bLines)
	{
		UE_LOG(LogGenerateSmoke, Error, TEXT("  Mrs. Hall lines load error: %s"), *LinesErr);
	}
	const EGenerateOutcome Reasons[] = {
		EGenerateOutcome::RefusedNoMatch, EGenerateOutcome::RefusedAmbiguous,
		EGenerateOutcome::RefusedOverBudget, EGenerateOutcome::RefusedUnsafe
	};
	bool bAllReasonsHaveLines = true;
	bool bAllReasonsHaveAudioKey = true; // P2.5: the AudioKey column wired through to the pick
	for (const EGenerateOutcome Reason : Reasons)
	{
		const FMrsHallLine Picked = PickMrsHallLine(Lines, Reason, 0);
		if (Picked.Line.IsEmpty())
		{
			bAllReasonsHaveLines = false;
			UE_LOG(LogGenerateSmoke, Warning, TEXT("  G7: no line for reason %s"), OutcomeStr(Reason));
		}
		if (Picked.AudioKey.IsEmpty())
		{
			bAllReasonsHaveAudioKey = false;
			UE_LOG(LogGenerateSmoke, Warning, TEXT("  G7: no AudioKey for reason %s"), OutcomeStr(Reason));
		}
	}
	R.Check(bAllReasonsHaveLines, TEXT("G7: every refusal reason has at least one non-empty line"));
	R.Check(bAllReasonsHaveAudioKey, TEXT("G7/P2.5: every refusal reason's line carries a non-empty AudioKey"));

	/* ---------- THE STORY LINES (docs/SPINE.md Move 2) ----------
	   Everything Mrs. Hall says that is NOT a Generate refusal lives in its own table, so
	   a story Reason can never leak into the refusal pool: MrsHallReasonToOutcome maps
	   anything it does not recognise to RefusedNoMatch, so a "Ticket" row in the refusal
	   CSV could be handed to a player as the answer to typing "a pine tree".

	   These checks exist because the failure mode here is SILENCE. A missing table or a
	   misspelled Reason makes her say nothing at all — no error, no crash, nothing in the
	   log — the same quiet failure class that hid every interaction prompt for eight
	   releases. */
	{
		TMap<FName, TArray<FMrsHallLine>> Story;
		FString StoryErr;
		const bool bStory = LoadMrsHallStoryLines(Story, StoryErr);
		R.Check(bStory, TEXT("SPINE: Mrs. Hall story lines load from data"));
		if (!bStory)
		{
			UE_LOG(LogGenerateSmoke, Error, TEXT("  story lines load error: %s"), *StoryErr);
		}

		/* Reasons that are WRITTEN and therefore must keep working. This list is the
		   contract: add a beat's line to the CSV, add its Reason here, and the gate will
		   hold it from then on. */
		static const TCHAR* RequiredReasons[] = { TEXT("Ticket"), TEXT("Power.CodeVision"), TEXT("Final") };
		bool bAllStoryReasons = true;
		for (const TCHAR* ReasonStr : RequiredReasons)
		{
			if (PickMrsHallStoryLine(Story, FName(ReasonStr), 0).Line.IsEmpty())
			{
				bAllStoryReasons = false;
				UE_LOG(LogGenerateSmoke, Warning, TEXT("  SPINE: no story line for Reason '%s'"), ReasonStr);
			}
		}
		R.Check(bAllStoryReasons, TEXT("SPINE: every story Reason the code asks for has a line"));

		/* The REMAINING beats, reported and not failed. The character now asks for
		   Power.<Verb> on the first use of every power (one subscription to the gated
		   OnPowerVerbUsed chokepoint), so the code is wired for all six — but the writing
		   is Walt's, and an unwritten line is a deliberate silence, not a defect. Printing
		   the outstanding ones here means the remaining work is visible in the gate rather
		   than discovered by walking around the game wondering why she said nothing. */
		static const TCHAR* AllPowerReasons[] = {
			TEXT("Power.CodeVision"), TEXT("Power.Refactor"), TEXT("Power.Compile"),
			TEXT("Power.TestDrive"),  TEXT("Power.Deploy"),   TEXT("Power.Generate")
		};
		int32 Written = 0;
		FString Missing;
		for (const TCHAR* ReasonStr : AllPowerReasons)
		{
			if (PickMrsHallStoryLine(Story, FName(ReasonStr), 0).Line.IsEmpty())
			{
				Missing += FString::Printf(TEXT(" %s"), ReasonStr);
			}
			else
			{
				++Written;
			}
		}
		UE_LOG(LogGenerateSmoke, Display,
			TEXT("  SPINE: power beats written %d/6.%s"),
			Written, Missing.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Still to write:%s"), *Missing));

		// And no story line may also be serving as a Generate refusal.
		bool bNoLeak = true;
		for (const TPair<FName, TArray<FMrsHallLine>>& StoryGroup : Story)
		{
			for (const FMrsHallLine& S : StoryGroup.Value)
			{
				for (const TPair<EGenerateOutcome, TArray<FMrsHallLine>>& RefusalGroup : Lines)
				{
					for (const FMrsHallLine& Refusal : RefusalGroup.Value)
					{
						if (Refusal.Line.Equals(S.Line)) { bNoLeak = false; }
					}
				}
			}
		}
		R.Check(bNoLeak, TEXT("SPINE: no story line has leaked into the Generate refusal pool"));
	}

	// Selection is deterministic for a given selector (no RNG/clock).
	R.Check(PickMrsHallLine(Lines, EGenerateOutcome::RefusedNoMatch, 0).Line == PickMrsHallLine(Lines, EGenerateOutcome::RefusedNoMatch, 0).Line,
		TEXT("G7: line selection is deterministic for a given selector"));

	// P2.5: print the Line -> AudioKey manifest in the gate output (the record list for Walt).
	if (bLines)
	{
		LogMrsHallAudioManifest(Lines);
	}

	// =====================================================================
	//  G6 — persistence via the REAL Ch3 build + Ch5 persist pipeline.
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G6: persistence through the real pipeline ---"));
#if WITH_EDITOR
	// A resolved entry must spawn something. Use the catalog's "key"-shaped entry as the
	// thing-to-spawn and drive it through ABuildSite (Ch3) + UBranchSubsystem (Ch5).
	const FGenerateResolution Spawned = ClassifyGenerateRequest(TEXT("a brass key"), Catalog, 99);
	R.Check(Spawned.Outcome == EGenerateOutcome::Resolved, TEXT("G6: request resolves to a catalog entry to spawn"));

	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(DefaultMapPackage);
	R.Check(World != nullptr, FString::Printf(TEXT("G6: map loads (%s)"), *DefaultMapPackage));
	if (World)
	{
		const FString SandboxSlot = TEXT("GenerateSpikeSlot_Temp");
		FSibeliusSaveIO::Delete(SandboxSlot);

		// Spawn the resolved object as an ABuildSite (the Ch3 buildable) + a seeded
		// inventory, exactly as the real generate->build pipeline would.
		ABuildSite* Site = World->SpawnActor<ABuildSite>();
		AActor* InvActor = World->SpawnActor<AActor>();
		UInventoryComponent* Inv = InvActor ? NewObject<UInventoryComponent>(InvActor) : nullptr;
		if (Inv) { Inv->RegisterComponent(); }
		R.Check(Site != nullptr, TEXT("G6: spawned an ABuildSite for the resolved entry"));
		R.Check(Inv != nullptr,  TEXT("G6: spawned a seeded inventory"));

		if (Site && Inv)
		{
			// G6: the spawned object receives a stable FGuid (SIB-38 bakes it at edit-time
			// construction; here the editor-world spawn assigns one). Capture it.
			const FGuid Id = Site->GetOrCreateBranchId();
			R.Check(Id.IsValid(), TEXT("G6: the spawned buildable received a valid, stable FGuid"));

			// Build it with the REAL Ch3 verb (afford the default cost).
			Inv->Add(EResourceType::Book, Site->Cost + 2);
			R.Check(Site->Build(Inv) && Site->IsBuilt(), TEXT("G6: Ch3 Build() built the spawned object"));

			// Persist via the REAL Ch5 path: deploy (writes a GUID-keyed delta) at Main.
			UBranchSubsystem* Branch = NewObject<UBranchSubsystem>(GetTransientPackage(), TEXT("GenSpikeBranch"));
			R.Check(Branch != nullptr, TEXT("G6: branch subsystem instantiates"));
			if (Branch)
			{
				Branch->SetBranchWorld(World);
				Branch->SetDeploySlotName(SandboxSlot);

				R.Check(Branch->RequestDeploy(), TEXT("G6: RequestDeploy persisted the deployed state at Main"));
				R.Check(FSibeliusSaveIO::Has(SandboxSlot), TEXT("G6: deploy wrote a save to the slot"));

				// Knock the built state back down, then re-apply from disk (reload).
				Site->RestoreBranchState(0);
				R.Check(!Site->IsBuilt(), TEXT("G6: reset the spawned object to unbuilt"));

				R.Check(Branch->ApplyDeployedSave(), TEXT("G6: ApplyDeployedSave loaded + re-applied the save"));
				R.Check(Site->IsBuilt(), TEXT("G6: the spawned object round-tripped save->reload back to BUILT"));
				R.Check(Site->GetBranchId() == Id, TEXT("G6: its FGuid was stable across the round-trip"));
			}
		}

		FSibeliusSaveIO::Delete(SandboxSlot);
		R.Check(!FSibeliusSaveIO::Has(SandboxSlot), TEXT("G6: sandbox slot cleaned up"));
	}
#else
	R.Check(false, TEXT("G6: requires an editor build (skipped) — re-run via UnrealEditor-Cmd"));
#endif

	// =====================================================================
	//  G8 — SIB-30 P3: a runtime-GENERATED object survives deploy -> reload.
	//  A generated ABuildSite has no placed actor, so on reload it must be
	//  RE-CREATED from the catalog (EntryId + saved transform + saved GUID),
	//  and the generation budget must persist (it used to reset). Headless we
	//  simulate "reload" by DESTROYING the runtime actor + resetting the budget.
	// =====================================================================
	UE_LOG(LogGenerateSmoke, Display, TEXT("--- G8: P3 generated-object persistence (re-spawn on reload) ---"));
#if WITH_EDITOR
	{
		const FGenerateResolution G8Res = ClassifyGenerateRequest(TEXT("a brass key"), Catalog, 99);
		const FGenerateCatalogEntry* G8Entry = Catalog.FindByPredicate(
			[&G8Res](const FGenerateCatalogEntry& E) { return E.EntryId == G8Res.EntryId; });
		R.Check(G8Res.Outcome == EGenerateOutcome::Resolved && G8Entry != nullptr,
			TEXT("G8: request resolves to a catalog entry to generate"));

		// REUSE the map G6 already loaded (its `World` is still in function scope) — the
		// destroy-actor/reset-budget reload simulation needs a world, not a fresh one.
		// Re-declaring `World` here shadowed G6's (C4456, warnings-as-errors); a second
		// LoadMap would also owe its own CleanupWorld on every exit path (the exit-3 lesson).
		R.Check(World != nullptr, TEXT("G8: reuses the map loaded for G6"));
		if (World && G8Entry)
		{
			const FString P3Slot = TEXT("GenerateP3Slot_Temp");
			FSibeliusSaveIO::Delete(P3Slot);

			// Spawn a GENERATED site through the SHARED author path (exactly what the live
			// UGenerateComponent::SpawnEntry now calls), at a known, off-origin transform.
			const FTransform SavedXform(FRotator(0.f, 90.f, 0.f), FVector(123.f, 456.f, 78.f), FVector::OneVector);
			ABuildSite* Gen = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SavedXform);
			R.Check(Gen != nullptr, TEXT("G8: spawned a generated ABuildSite"));
			if (Gen)
			{
				AuthorGeneratedSite(Gen, *G8Entry);
			}
			const FGuid GenId = Gen ? Gen->GetOrCreateBranchId() : FGuid();
			R.Check(Gen && Gen->IsGenerated() && Gen->IsBuilt(), TEXT("G8: generated site is tagged + BUILT"));
			R.Check(Gen && Gen->GetGenerateEntryId() == G8Res.EntryId, TEXT("P3-2: site carries its catalog EntryId"));

			// A generator component holding a known, non-default remaining budget (7).
			AActor* GenOwner = World->SpawnActor<AActor>();
			UGenerateComponent* GenComp = GenOwner ? NewObject<UGenerateComponent>(GenOwner) : nullptr;
			if (GenComp) { GenComp->RegisterComponent(); GenComp->SetRemainingBudget(7); }
			R.Check(GenComp != nullptr, TEXT("G8: generator component present in the world"));

			UBranchSubsystem* Branch = NewObject<UBranchSubsystem>(GetTransientPackage(), TEXT("GenP3Branch"));
			if (Branch && Gen && GenComp)
			{
				Branch->SetBranchWorld(World);
				Branch->SetDeploySlotName(P3Slot);

				// Deploy at Main.
				R.Check(Branch->RequestDeploy(), TEXT("G8: RequestDeploy persisted the deployed state"));

				// P3-2 / P3-6: the WRITTEN save carries the generated provenance + budget.
				USibeliusSaveGame* Saved = Cast<USibeliusSaveGame>(FSibeliusSaveIO::Load(P3Slot));
				const FBranchObjectState* Rec = Saved ? Saved->ObjectDeltas.FindByPredicate(
					[&GenId](const FBranchObjectState& O) { return O.ObjectId == GenId; }) : nullptr;
				R.Check(Rec != nullptr && Rec->bGenerated && Rec->GenerateEntryId == G8Res.EntryId,
					TEXT("P3-2: save record carries the generated EntryId"));
				R.Check(Rec != nullptr && Rec->GenerateTransform.Equals(SavedXform),
					TEXT("P3-2/P3-3: save record carries the spawn transform"));
				R.Check(Saved != nullptr && Saved->GenerateBudget == 7,
					TEXT("P3-6: save carries the remaining generation budget (not the default)"));

				// Simulate RELOAD: the runtime actor is gone, and the budget has reset.
				Gen->Destroy();
				GenComp->SetRemainingBudget(10); // back to the authored default
				R.Check(CountBuildSitesByGuid(World, GenId) == 0, TEXT("G8: generated actor gone after simulated reload"));

				// Apply — the fix re-creates it.
				R.Check(Branch->ApplyDeployedSave(), TEXT("G8: ApplyDeployedSave re-applied the save"));
				R.Check(Branch->GetLastApplyGeneratedForTest() == 1, TEXT("P3-1: exactly one generated object re-spawned"));

				ABuildSite* Re = FindBuildSiteByGuid(World, GenId);
				R.Check(Re != nullptr, TEXT("P3-1: the generated object was re-spawned"));
				R.Check(Re != nullptr && Re->IsBuilt(), TEXT("P3-1: the re-spawned object is BUILT"));
				R.Check(Re != nullptr && Re->GetBranchId() == GenId, TEXT("P3-4: its GUID is stable across the re-spawn"));
				R.Check(Re != nullptr && Re->IsGenerated() && Re->GetGenerateEntryId() == G8Res.EntryId,
					TEXT("P3-1: re-spawn re-tagged it generated with the same EntryId (so a 2nd deploy won't orphan it)"));
				R.Check(Re != nullptr && Re->GetActorTransform().Equals(SavedXform),
					TEXT("P3-3: re-spawn used the SAVED transform verbatim (no placement re-trace)"));
				R.Check(GenComp->GetRemainingBudget() == 7,
					TEXT("P3-6: budget restored to the deploy-time value (7), not the post-reload default (10)"));

				// P3-5: idempotent — applying AGAIN must not double-spawn.
				R.Check(Branch->ApplyDeployedSave(), TEXT("P3-5: a second ApplyDeployedSave succeeds"));
				R.Check(Branch->GetLastApplyGeneratedForTest() == 0, TEXT("P3-5: the second apply re-spawns nothing (object already present)"));
				R.Check(CountBuildSitesByGuid(World, GenId) == 1, TEXT("P3-5: exactly ONE generated object after two applies (no duplicate)"));
			}

			FSibeliusSaveIO::Delete(P3Slot);
			FSibeliusSaveIO::Delete(P3Slot + TEXT("_Backup")); // apply mirrors a last-good backup
			R.Check(!FSibeliusSaveIO::Has(P3Slot), TEXT("G8: sandbox slot cleaned up"));
		}
	}
#else
	R.Check(false, TEXT("G8: requires an editor build (skipped) — re-run via UnrealEditor-Cmd"));
#endif

	if (R.Failures == 0)
	{
		UE_LOG(LogGenerateSmoke, Display, TEXT("=== GENERATE SMOKE TEST PASSED (Ch6 P2 — catalog + blocklist + Mrs. Hall lines green). ==="));
		return 0;
	}
	UE_LOG(LogGenerateSmoke, Error, TEXT("=== GENERATE SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
}
