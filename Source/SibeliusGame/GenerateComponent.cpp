// GenerateComponent.cpp — SIB-30 Ch6 P1. Resolve a typed request -> spawn / refuse.

#include "GenerateComponent.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
#include "GenerateMatcher.h"
#include "GenerateCatalog.h"
#include "MrsHallLines.h"           // P2: refusal lines + DC blocklist (data-driven)
#include "MrsHallMessageWidget.h"   // P2: the styled refusal memo
#include "MrsHallSubsystem.h"       // SPINE Move 2: the one channel she speaks through

#include "BuildSite.h"
#include "GistSite.h"                // THE GIST: what no-match and a tie build now
#include "GistRules.h"
#include "FilmGenerator.h"           // "a film about..." makes a film
#include "FilmDirectorSubsystem.h"
#include "Spaceport.h"               // the one catalog entry with a prerequisite
#include "ProteinMachine.h"          // IsEnhanced - that prerequisite
#include "HintVolume.h"              // the lawn: where a spaceport may be built
#include "Branchable.h"              // GetBranchId - the id a record is keyed on
#include "ProgressionSubsystem.h"    // where generated objects are remembered
#include "SibeliusGame.h"            // LogSibeliusGame
#include "EngineUtils.h"             // TActorIterator - what is already here
#include "GameFramework/PlayerStart.h" // the lawn marker the spaceport is built from
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"   // CreateWidget
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"          // GEngine toast
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h" // P2.5: PlaySound2D
#include "Sound/SoundBase.h"        // P2.5: USoundBase
#include "Misc/App.h"               // P2.5: FApp::CanEverRenderAudio (headless guard)
#include "UObject/UObjectGlobals.h" // P2.5: LoadObject (soft-load the clip by path)
#include "HAL/IConsoleManager.h"    // sib.Generate.Request

/* WHICH CLASS THIS ROW SPAWNS — one answer, used by BOTH spawn paths.

   SpawnEntry (live) and RespawnGeneratedSite (reload) each used to name
   ABuildSite::StaticClass() themselves. That was two copies of one fact, and the day a
   row spawns a subclass is the day they can disagree: generate a spaceport, save, reload,
   and get a bare BuildSite with the spaceport's EntryId on it. This function is the only
   place the question is answered.

   A class that fails to load falls back to a plain ABuildSite rather than refusing. The
   row still resolved, the budget was still charged, and a player who typed a valid word
   should get SOMETHING - the log says which class went missing. */
static TSubclassOf<ABuildSite> ResolveSiteClass(const FGenerateCatalogEntry& Entry)
{
	if (!Entry.ActorClass.IsNull())
	{
		if (UClass* Loaded = Entry.ActorClass.LoadSynchronous())
		{
			return Loaded;
		}
		UE_LOG(LogTemp, Warning,
			TEXT("[Generate] '%s' names ActorClass '%s' which failed to load - "
			     "falling back to a plain BuildSite."),
			*Entry.EntryId.ToString(), *Entry.ActorClass.ToString());
	}
	return ABuildSite::StaticClass();
}

// SIB-30 P3 — shared generated-site authoring. ONE place that turns a catalog entry into a
// built, tagged ABuildSite, so live generation and re-spawn-on-load can't diverge.
void AuthorGeneratedSite(ABuildSite* Site, const FGenerateCatalogEntry& Entry,
	bool bFreshlyGenerated)
{
	if (!Site)
	{
		return;
	}

	// Author the catalog mesh + per-entry transform onto the buildable's FinalMesh
	// (SIB-40-in-data — e.g. the key's Scale 0.25 / Yaw 90). Soft-load is cached/cheap.
	//
	// A CLASS-BACKED ROW BRINGS ITS OWN LOOK. ASpaceport builds itself out of dozens of
	// meshes; stamping a single catalog mesh onto FinalMesh would be meaningless at best
	// and, with the empty Mesh such a row will have, would OVERWRITE the authored scale
	// and rotation its own construction depends on. Mesh rows are untouched by this.
	UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
	const bool bClassBacked = !Entry.ActorClass.IsNull();
	if (Site->FinalMesh && !bClassBacked)
	{
		if (Mesh)
		{
			Site->FinalMesh->SetStaticMesh(Mesh);
		}
		Site->FinalMesh->SetRelativeScale3D(Entry.SpawnScale);
		Site->FinalMesh->SetRelativeRotation(Entry.SpawnRotation);
	}

	// Tag the provenance (so Deploy can persist it) and present it BUILT. Generation pays
	// the BUDGET, not Books, so it uses the RAW RestoreBranchState(1) path, NOT Build().
	Site->MarkGenerated(Entry.EntryId);
	Site->RestoreBranchState(1);

	/* LAST, and only when the player is watching. RestoreBranchState has already put the
	   site in its finished state, so a class that performs an arrival replays it FROM
	   finished — nothing can be left half-built if the hook does nothing or throws. */
	if (bFreshlyGenerated)
	{
		Site->OnGeneratedFresh();
	}
}

ABuildSite* RespawnGeneratedSite(UWorld* World, const FGenerateCatalogEntry& Entry,
	const FTransform& SavedTransform, const FGuid& SavedId)
{
	if (!World)
	{
		return nullptr;
	}

	// AlwaysSpawn (NOT AdjustIfPossible) so the saved transform is honoured VERBATIM —
	// the original P1 placement trace already resolved the floor/wall; re-running it on
	// reload would be wrong (P3-3). The actor pose is the saved pose, full stop.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildSite* Site = World->SpawnActor<ABuildSite>(ResolveSiteClass(Entry), SavedTransform, SpawnParams);
	if (!Site)
	{
		return nullptr;
	}

	Site->RestoreBranchId(SavedId);   // stable identity across the re-spawn (P3-4)
	AuthorGeneratedSite(Site, Entry); // mesh + tag + BUILT (no budget charge — pure recreate)
	return Site;
}

UGenerateComponent::UGenerateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGenerateComponent::BeginPlay()
{
	Super::BeginPlay();

	FString Err;
	if (LoadGenerateCatalog(Catalog, Err))
	{
		UE_LOG(LogTemp, Display, TEXT("[Generate] catalog loaded: %d entries, %d sauce per unit of cost."), Catalog.Num(), SaucePerCost);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Generate] catalog load FAILED: %s"), *Err);
	}

	// P2: Mrs. Hall's refusal lines + the DC unsafe-word blocklist (data, editor-CSV or
	// asset). Non-fatal if missing — refusals fall back to a terse hard-coded line / no veto.
	FString LinesErr;
	if (LoadMrsHallLines(RefusalLines, LinesErr))
	{
		UE_LOG(LogTemp, Display, TEXT("[Generate] Mrs. Hall lines loaded: %d reason group(s)."), RefusalLines.Num());
		// P2.5: print the Line -> AudioKey manifest so Walt knows which clips to record.
		LogMrsHallAudioManifest(RefusalLines);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Generate] Mrs. Hall lines load FAILED: %s"), *LinesErr);
	}

	FString BlockErr;
	if (LoadGenerateBlocklist(Blocklist, BlockErr))
	{
		UE_LOG(LogTemp, Display, TEXT("[Generate] unsafe blocklist loaded: %d word(s)."), Blocklist.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Generate] blocklist load FAILED: %s"), *BlockErr);
	}

	/* LAST, AND AFTER THE CATALOG IS LOADED. Restoring a saved site needs its catalog row
	   to author it from, so this cannot move above the load without silently restoring
	   nothing. Order is the whole safety here. */
	RestoreGeneratedSites();

	/* AND THE SPACEPORT, IF HE IS OWED ONE. It builds itself the moment he is enhanced
	   (AProteinMachine), but a save from before 2026-11-09 can be enhanced with no spaceport
	   and, now that G no longer builds one, no way to get it. After the restore above, so a
	   spaceport he already has is found standing and nothing is built twice. A level with
	   no lawn returns false at once. */
	if (AProteinMachine::IsEnhanced(this))
	{
		BuildSpaceportOnLawn();
	}
}

const FGenerateCatalogEntry* UGenerateComponent::FindEntry(const FName& Id) const
{
	return Catalog.FindByPredicate([&Id](const FGenerateCatalogEntry& E) { return E.EntryId == Id; });
}

EGenerateOutcome UGenerateComponent::SubmitRequest(const FString& RawText)
{
	/* [G] MAKES FILMS, AND ONLY FILMS (Walt, 2026-11-09: "the G option still makes the silly
	   objects out of cones, cubes etc. let us get rid of that feature. let us make it clear
	   that G is only for films").

	   Whatever he types is what the film is about: "my old boss" and "a film about my old
	   boss" are the same wish. The film generator (FilmGenRules.h) writes a short film
	   around his words and the agents in the level perform it. Ten sauce, charged once it
	   is actually playing.

	   WHAT THIS REPLACED. G used to resolve the words against a closed catalog (a tree, a
	   lamp, a crate, a chair, a key, a plant, the spaceport) and, when nothing matched,
	   build THE GIST of them out of cones and cubes. Neither is reachable from G any more.
	   The spaceport now builds itself when he is enhanced (BuildSpaceportOnLawn) and the
	   Sibelius Hotel when he lands on Grok (UGrokEndingSubsystem). The catalog, the gist
	   rules and the restore path all stay, because things he generated in an older save are
	   still standing in his levels and still have to come back when he returns to them.

	   A word on Mrs. Hall's blocklist is still refused, by her, in her voice.

	   IT STARTS A MOMENT LATER, NOT NOW. This runs inside the G panel's Enter, and the
	   character closes the panel straight afterwards (HandleGenerateSubmit), handing the
	   keyboard back to the game. A film started here would have its screen take the
	   keyboard and then lose it to that close, leaving a film he cannot skip. */
	UProgressionSubsystem* Wallet = UProgressionSubsystem::Get(this);
	const int32 Sauce = Wallet ? Wallet->GetSauce() : MAX_int32;

	FFilm Film;
	const uint32 Take = static_cast<uint32>(FMath::Rand());
	if (!FilmGenerator::Make(RawText, Take, Blocklist, Film))
	{
		UE_LOG(LogTemp, Display, TEXT("[Generate] request \"%s\" -> refused (a blocked word)."), *RawText);
		const FMrsHallLine Picked = PickMrsHallLine(RefusalLines, EGenerateOutcome::RefusedUnsafe, RefusalCount++);
		const FString Line = Picked.Line.IsEmpty() ? FString(TEXT("Absolutely not.")) : Picked.Line;
		if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
		{
			Hall->SayLine(Line, Picked.AudioKey);
		}
		else
		{
			ShowMrsHall(Line);   // no world subsystem (headless): the local fallback still works
		}
		return EGenerateOutcome::RefusedUnsafe;
	}

	UE_LOG(LogTemp, Display, TEXT("[Generate] request \"%s\" -> a film, \"%s\" (take %u), sauce %d."),
		*RawText, *Film.Title, Take, Sauce);
	if (Sauce < SaucePerCost)
	{
		ASibeliusHUD::Toast(this, FString::Printf(TEXT("A FILM NEEDS %d SAUCE — YOU HAVE %d"), SaucePerCost, Sauce),
			3.5f, SibeliusToast::Warn);
		return EGenerateOutcome::RefusedOverBudget;
	}

	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UGenerateComponent> WeakThis(this);
		FTimerHandle Unused;
		World->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakThis, Film]()
		{
			UGenerateComponent* Self = WeakThis.Get();
			UWorld* Now = Self ? Self->GetWorld() : nullptr;
			UFilmDirectorSubsystem* Director = Now ? Now->GetSubsystem<UFilmDirectorSubsystem>() : nullptr;
			if (Director && Director->PlayFilm(Film))
			{
				// Paid once it is playing, as a generated object was paid for once it stood.
				if (UProgressionSubsystem* Purse = UProgressionSubsystem::Get(Self))
				{
					Purse->TrySpendSauce(SaucePerCost);
				}
				Self->bHasSpawnedThisSession = true;   // SIB-31: the cathedral door's generate gate
			}
			else if (Self)
			{
				ASibeliusHUD::Toast(Self, TEXT("A FILM NEEDS AN AGENT TO BE IN IT — FIND ONE AND PRESS G AGAIN"),
					4.0f, SibeliusToast::Warn);
			}
		}), 0.15f, /*bLoop=*/false);
	}
	return EGenerateOutcome::Resolved;
}

/* THE SPACEPORT BUILDS ITSELF (Walt, 2026-11-09: "let us have the spaceport happen instantly
   in the lawn after you get your proteins").

   It was the one catalog row the story needed, and it had gathered three refusals of its
   own: not before the protein treatment, not from anywhere but the lawn, not without the
   sauce. All three existed because he had to stand in the right place, in the right state,
   and type the right word. Now the game does it for him at the moment the last condition
   comes true, and none of the three can be got wrong.

   WHERE. The catalog row plants it 160 m ahead of the player, and the place that was
   designed and playtested from is the edge of the lawn, looking across it. That is exactly
   what this reproduces without him: from the SpaceportLawn marker (the PlayerStart Nyra
   waits beside at stage 2, which sits on the lawn's near edge), through the centre of the
   lawn (the AHintVolume whose hint a spaceport satisfies), the row's own distance on. Both
   are found in the level, so moving either in the editor moves the spaceport with it.

   False, and nothing built, when this level has no lawn, when a spaceport already stands
   (or stood: a launched or discarded one is still an actor), or the catalog has no row. */
bool UGenerateComponent::BuildSpaceportOnLawn()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ASpaceport> It(World); It; ++It)
	{
		return false;
	}

	const AHintVolume* Lawn = nullptr;
	for (TActorIterator<AHintVolume> It(World); It; ++It)
	{
		if (It->SatisfiedWhenPresent && It->SatisfiedWhenPresent->IsChildOf(ASpaceport::StaticClass()))
		{
			Lawn = *It;
			break;
		}
	}
	const FGenerateCatalogEntry* Row = Catalog.FindByPredicate([](const FGenerateCatalogEntry& E)
	{
		const UClass* SiteClass = ResolveSiteClass(E).Get();
		return SiteClass && SiteClass->IsChildOf(ASpaceport::StaticClass());
	});
	if (!Lawn || !Row)
	{
		return false;
	}

	// The near edge of the lawn. Without the marker, the side of the lawn he is on.
	static const FName LawnMarkerTag(TEXT("SpaceportLawn"));   // UDancerAgentComponent::GuideStage2StartTag
	const FVector LawnCentre = Lawn->GetActorLocation();
	FVector Approach = LawnCentre;
	bool bHaveApproach = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->PlayerStartTag == LawnMarkerTag)
		{
			Approach = It->GetActorLocation();
			bHaveApproach = true;
			break;
		}
	}
	if (!bHaveApproach)
	{
		if (const AActor* Owner = GetOwner())
		{
			Approach = Owner->GetActorLocation();
		}
	}
	const FVector Across = (LawnCentre - Approach).GetSafeNormal2D();
	if (Across.IsNearlyZero())
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Generate] spaceport not built: no way to tell which side of the lawn is the far one."));
		return false;
	}

	const float Ahead = Row->SpawnAhead > 0.0f ? Row->SpawnAhead : 16000.0f;
	const FVector Spot = Approach + Across * Ahead;

	// The ground there, by the same long-throw trace FindSpawnPoint uses: start high, because
	// the land beyond the lawn rises, and stop 30 m below where he would have been standing.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GenerateLawnTrace), false);
	if (const AActor* Owner = GetOwner())
	{
		Params.AddIgnoredActor(Owner);
	}
	const bool bHit = World->LineTraceSingleByChannel(Hit,
		FVector(Spot.X, Spot.Y, Approach.Z + Ahead * 0.5f), FVector(Spot.X, Spot.Y, Approach.Z - 3000.0f),
		ECC_WorldStatic, Params);
	const FVector SpawnLoc = bHit ? Hit.ImpactPoint : FVector(Spot.X, Spot.Y, Approach.Z - 90.0f);

	const bool bBuilt = SpawnEntryAt(*Row, SpawnLoc);
	UE_LOG(LogSibeliusGame, Display, TEXT("[Generate] the spaceport builds itself at %s (%s the ground, marker %s): %s."),
		*SpawnLoc.ToCompactString(), bHit ? TEXT("on") : TEXT("NO TRACE HIT, guessed"),
		bHaveApproach ? TEXT("found") : TEXT("missing"), bBuilt ? TEXT("built") : TEXT("FAILED"));
	return bBuilt;
}

bool UGenerateComponent::SpawnEntry(const FGenerateCatalogEntry& Entry)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return false;
	}

	/* HOW FAR AHEAD THIS PARTICULAR THING WANTS TO BE. A lamp is happy at 250 cm; a
	   launch complex is not, and putting one there trapped Walt inside it. */
	const float RequestedAhead = (Entry.SpawnAhead > 0.0f) ? Entry.SpawnAhead : SpawnAheadDistance;
	FVector SpawnLoc;
	if (!FindSpawnPoint(RequestedAhead, Entry.SpawnAhead > 0.0f, SpawnLoc))
	{
		return false;
	}
	return SpawnEntryAt(Entry, SpawnLoc);
}

/** The second half of SpawnEntry: stand this row's thing at a point already chosen. Split
    out so the spaceport can be built somewhere he is not looking (BuildSpaceportOnLawn). */
bool UGenerateComponent::SpawnEntryAt(const FGenerateCatalogEntry& Entry, FVector SpawnLoc)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Force-load the mesh from the soft pointer.
	UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();

	// Rest the mesh ON the floor: the actor origin sits at the trace-hit point, but a
	// centered pivot would bury the lower half. Lift the actor by the distance from the
	// pivot down to the bounding-box bottom (scaled), so the mesh BOTTOM rests on the
	// floor. (Yaw rotations don't change the Z extent; good enough for P1.)
	if (Mesh)
	{
		const FBox LocalBox = Mesh->GetBoundingBox();
		SpawnLoc.Z += -LocalBox.Min.Z * Entry.SpawnScale.Z;
	}

	// Spawn — adjust out of overlaps but ALWAYS spawn (never silently rejected).
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABuildSite* Site = World->SpawnActor<ABuildSite>(ResolveSiteClass(Entry), SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (!Site)
	{
		return false; // confirmed failure — budget must NOT charge (handled by caller)
	}

	// Author + tag + present BUILT via the SHARED path (SIB-30 P3) — the same call
	// re-spawn-on-load uses, so a generated object looks identical fresh or reloaded. The
	// actor's GUID is assigned by the normal IBranchable fallback (BeginPlay); Deploy then
	// persists it + this site's EntryId + transform so reload can re-create it.
	AuthorGeneratedSite(Site, Entry, /*bFreshlyGenerated=*/true);

	/* AND REMEMBER IT, so it is still standing when he comes back to this level.
	   AuthorGeneratedSite has run, so the IBranchable id exists to key the record on. */
	RememberSite(Site, Entry.EntryId);

	// Budget charges only because we reached here with a real, created actor.
	return true;
}

bool UGenerateComponent::SpawnGist(const FString& RawText, const Gist::FRecipe& Recipe)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return false;
	}

	// Far enough ahead that a big gist does not land on him: its centre goes half its own
	// footprint plus a stride away, and never nearer than a catalog item would.
	Gist::FVec Min, Max;
	Gist::Bounds(Recipe.Parts, Min, Max);
	const float Footprint = FMath::Max(Max.X - Min.X, Max.Y - Min.Y);
	FVector SpawnLoc;
	if (!FindSpawnPoint(FMath::Max(SpawnAheadDistance, Footprint * 0.5f + 150.0f), /*bNamedDistance=*/false, SpawnLoc))
	{
		return false;
	}

	// A recipe faces +X; turn it round to face him.
	const FRotator Facing(0.0f, Pawn->GetControlRotation().Yaw + 180.0f, 0.0f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGistSite* Site = World->SpawnActor<AGistSite>(AGistSite::StaticClass(), SpawnLoc, Facing, SpawnParams);
	if (!Site)
	{
		return false;
	}
	Site->Author(RawText, Recipe, /*bFreshlyGenerated=*/true);

	// Remembered by its words: they rebuild the same toy when he comes back (RestoreGeneratedSites).
	RememberSite(Site, AGistSite::EntryId, RawText);
	const FString What = Recipe.bContraption ? FString(TEXT("a contraption")) : FString(UTF8_TO_TCHAR(Recipe.Archetype.c_str()));
	UE_LOG(LogTemp, Display, TEXT("[Generate] THE GIST of \"%s\": %s, %d part(s), %d cop(ies), scale %.2f, behaviours %u."),
		*RawText, *What, static_cast<int32>(Recipe.Parts.size()), Recipe.Copies, Recipe.Scale, Recipe.Behaviours);
	return true;
}

/* WHERE A GENERATED THING LANDS - one answer for catalog rows and gists alike. Moved out of
   SpawnEntry unchanged when THE GIST needed the same floor; the comments are its history. */
bool UGenerateComponent::FindSpawnPoint(float RequestedAhead, bool bNamedDistance, FVector& OutLocation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return false;
	}

	// Horizontal facing only (pitch zeroed) so the object lands ahead on the floor,
	// not underfoot or up in the air.
	const FVector PlayerLoc = Pawn->GetActorLocation();
	const float YawDeg = Pawn->GetControlRotation().Yaw;
	const FVector Fwd = FRotator(0.0f, YawDeg, 0.0f).Vector();

	/* THE WALL CLAMP IS FOR SMALL THINGS ONLY, and finding that out cost two playtests.

	   The clamp exists so a lamp asked for indoors does not appear through the wall in
	   the next room. It line-traces forward and stops just short of the first hit, which
	   is exactly right at 250 cm inside a house.

	   It is exactly wrong at 45 metres outdoors. Walt stood on the pavement facing the
	   lawn and was refused, because the trace hit the KNEE-HIGH FENCE four metres away
	   and concluded there was no room — beyond which lay the empty field he was pointing
	   at. A fence between you and a meadow does not make the meadow smaller.

	   So an entry that names its own distance gets it verbatim: no clamp, no refusal.
	   The trapping hazard that made the clamp look necessary was never about walls — it
	   was about the structure landing ON him at 250 cm, and 4500 cm cannot do that. The
	   remaining risk is a spaceport that intersects a building if he aims at one, which
	   is ugly, obvious, and undone with a Test-Drive discard. That is a fair trade for a
	   feature that otherwise cannot be used at all. */
	float EffectiveAhead = RequestedAhead;
	if (!bNamedDistance)
	{
		const FVector WallStart = PlayerLoc;
		const FVector WallEnd = WallStart + Fwd * RequestedAhead;
		FHitResult WallHit;
		FCollisionQueryParams WallParams(SCENE_QUERY_STAT(GenerateWallTrace), false, Pawn);
		WallParams.AddIgnoredActor(Pawn);
		if (World->LineTraceSingleByChannel(WallHit, WallStart, WallEnd, ECC_WorldStatic, WallParams))
		{
			EffectiveAhead = FMath::Max(0.0f, WallHit.Distance - 50.0f); // just IN FRONT of the wall
		}
	}
	const FVector ForwardPoint = PlayerLoc + Fwd * EffectiveAhead;

	/* Downward floor trace. Start BELOW the ceiling but above the floor (player head
	   height ~= player.Z + 100), then trace straight DOWN a long way so the FIRST hit is
	   the floor — NOT the ceiling. (Starting above the ceiling made the first hit the
	   ceiling, dropping the object overhead.)

	   THAT CEILING RULE IS AN INDOOR RULE, and 45 metres away there is no ceiling. The
	   lawn outside Jacob's RISES away from the road: start the trace a metre above the
	   player's head and, by the time it is out over the field, the ground can be above
	   the start point entirely. The trace then hits nothing, falls back to the player's
	   own foot height, and buries the spaceport in a hillside.

	   So a long throw starts high — above any slope it could reasonably reach — and a
	   short one keeps the ceiling-safe height it has always had. Same shape as the wall
	   clamp above: the indoor behaviour was never wrong, it was just never outdoors. */
	const float TraceTop = bNamedDistance
		? PlayerLoc.Z + FMath::Max(100.0f, EffectiveAhead * 0.5f)
		: PlayerLoc.Z + 100.0f;
	const FVector TraceStart(ForwardPoint.X, ForwardPoint.Y, TraceTop);
	// End 30 m below the PLAYER, not below the start — otherwise raising the start point
	// above a hill also raises where the trace gives up, and a downhill throw stops in
	// mid-air short of the ground it was aimed at.
	const FVector TraceEnd(TraceStart.X, TraceStart.Y, PlayerLoc.Z - 3000.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GenerateSpawnTrace), false, Pawn);
	Params.AddIgnoredActor(Pawn);
	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params);

	const float FeetZ = PlayerLoc.Z - 90.0f; // ~capsule bottom, fallback Z on a trace miss
	OutLocation = bHit ? Hit.ImpactPoint : FVector(ForwardPoint.X, ForwardPoint.Y, FeetZ);
	return true;
}

void UGenerateComponent::RememberSite(ABuildSite* Site, FName EntryId, const FString& Prompt)
{
	UWorld* World = Site ? Site->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return;   // headless / no save: build it, just do not promise it will keep
	}

	const FGuid Id = Site->GetBranchId();
	if (!Id.IsValid())
	{
		/* Loud, because the failure is invisible until he walks away and comes back.
		   A site with no stable id cannot be remembered OR forgotten. */
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Generate] '%s' has no branch id - it will NOT survive a level change."),
			*EntryId.ToString());
		return;
	}

	Progression->RememberGeneratedSite(CurrentLevelName(World), EntryId,
		Site->GetActorTransform(), Id, Prompt);
}

void UGenerateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	/* THE LAST HONEST MOMENT. Branch work moves a lot of object states and none of it
	   tells progression, so rather than hook every RestoreBranchState and save on each
	   one, read the truth off the world as it tears down and write it once.

	   This is where a DISCARD becomes permanent: Test-Drive set the spaceport to Empty on
	   a live actor, and until now nothing wrote that down, so the record still said Built
	   and reloading stood it back up.

	   If this never runs - a crash, a hard kill - the record keeps whatever it last held,
	   which is the state at build time. The object is not lost; only the last state
	   change is. That is the right way round. */
	if (UWorld* World = GetWorld())
	{
		if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
		{
			TMap<FGuid, uint8> States;
			for (TActorIterator<ABuildSite> It(World); It; ++It)
			{
				const FGuid Id = It->GetBranchId();
				if (Id.IsValid())
				{
					States.Add(Id, It->CaptureBranchState());
				}
			}
			if (States.Num() > 0)
			{
				Progression->UpdateGeneratedSiteStates(States);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

FName UGenerateComponent::CurrentLevelName(const UWorld* World)
{
	/* RemovePIEPrefix, or every record made in the editor names a level that does not
	   exist in a packaged build - the same trap ASibeliusGameCharacter avoids when it
	   decides whether it is standing in L_Cafe. */
	return World ? FName(*UWorld::RemovePIEPrefix(World->GetMapName())) : NAME_None;
}

void UGenerateComponent::RestoreGeneratedSites()
{
	UWorld* World = GetWorld();
	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!World || !Progression)
	{
		return;
	}

	const FName Level = CurrentLevelName(World);
	const TArray<FGeneratedSiteRecord> Records = Progression->GeneratedSitesForLevel(Level);
	if (Records.Num() == 0)
	{
		return;
	}

	/* WHAT IS ALREADY HERE? A Deploy load, or a Test-Drive restore, may have re-created
	   some of these before we got here - and putting a second spaceport inside the first
	   is a worse bug than the one this fixes. Ask the world, by the id the record holds. */
	TSet<FGuid> Present;
	for (TActorIterator<ABuildSite> It(World); It; ++It)
	{
		const FGuid Id = It->GetBranchId();
		if (Id.IsValid())
		{
			Present.Add(Id);
		}
	}

	int32 Restored = 0;
	for (const FGeneratedSiteRecord& R : Records)
	{
		if (Present.Contains(R.ObjectId))
		{
			continue;
		}
		// A gist has no catalog row: its words rebuild it, in the state he left it in.
		if (R.EntryId == AGistSite::EntryId)
		{
			if (AGistSite* Gist = AGistSite::Respawn(World, R.Prompt, R.Transform, R.ObjectId))
			{
				Gist->RestoreBranchState(R.State);
				++Restored;
			}
			continue;
		}
		const FGenerateCatalogEntry* Entry = FindEntry(R.EntryId);
		if (!Entry)
		{
			/* The catalog no longer has the row this was built from. Say so with both
			   names in it: the alternative is an object that silently stops coming back
			   after a data edit, which reads as a save bug rather than a catalog one. */
			UE_LOG(LogSibeliusGame, Warning,
				TEXT("[Generate] saved site '%s' in %s has no catalog row - not restored."),
				*R.EntryId.ToString(), *R.LevelName.ToString());
			continue;
		}
		if (ABuildSite* Site = RespawnGeneratedSite(World, *Entry, R.Transform, R.ObjectId))
		{
			/* PUT IT BACK IN THE STATE HE LEFT IT IN. AuthorGeneratedSite ends with
			   RestoreBranchState(1), which is right for a fresh build and wrong for a
			   reload: a spaceport discarded with Test-Drive was set to Empty, and
			   re-authoring it as Built stood it up again behind his back. */
			Site->RestoreBranchState(R.State);
			++Restored;
		}
	}

	if (Restored > 0)
	{
		UE_LOG(LogSibeliusGame, Display,
			TEXT("[Generate] restored %d generated object(s) in %s."),
			Restored, *Level.ToString());
	}
}

void UGenerateComponent::Toast(const FString& Msg, const FColor& Color) const
{
	// Was AddOnScreenDebugMessage — compiled out of Shipping, so none of Generate's
	// feedback ever reached a player. FromSRGBColor is the inverse of the conversion
	// Canvas applies on the way back out, so the colour on screen is unchanged.
	ASibeliusHUD::Toast(this, Msg, 3.5f, FLinearColor::FromSRGBColor(Color));
}

void UGenerateComponent::ShowMrsHall(const FString& Line)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		Toast(FString::Printf(TEXT("Mrs. Hall: \"%s\""), *Line), FColor(220, 160, 60)); // headless/edge fallback
		return;
	}

	if (!MrsHallWidget)
	{
		MrsHallWidget = CreateWidget<UMrsHallMessageWidget>(PC, UMrsHallMessageWidget::StaticClass());
	}
	if (!MrsHallWidget)
	{
		Toast(FString::Printf(TEXT("Mrs. Hall: \"%s\""), *Line), FColor(220, 160, 60));
		return;
	}

	MrsHallWidget->SetMessage(FText::FromString(Line));
	if (!MrsHallWidget->IsInViewport())
	{
		MrsHallWidget->AddToViewport(50); // above the world; below any modal panel
	}
	// Non-interactive notice — never steals input; the player keeps playing.
	MrsHallWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Auto-dismiss; a fresh refusal resets the timer (and replaces the line).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MrsHallDismissTimer);
		World->GetTimerManager().SetTimer(MrsHallDismissTimer, this, &UGenerateComponent::DismissMrsHall, 6.0f, false);
	}
}

void UGenerateComponent::DismissMrsHall()
{
	if (MrsHallWidget)
	{
		MrsHallWidget->RemoveFromParent();
	}
}

void UGenerateComponent::PlayMrsHallClip(const FString& AudioKey)
{
	if (AudioKey.IsEmpty())
	{
		return; // line has no clip key — nothing to play
	}

	// Headless safety: a commandlet / -nosound / server run has no audio device. The gate
	// must stay green and silent. (The smoke test calls the matcher directly and never
	// reaches this, but guard regardless so no path can error headless.)
	if (IsRunningCommandlet() || !FApp::CanEverRenderAudio())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Soft-load by path: no hard asset dependency, and a not-yet-recorded clip is harmless.
	// LOAD_NoWarn|LOAD_Quiet so a missing clip does NOT spam the log (clips arrive after this).
	const FString Path = FString::Printf(TEXT("/Game/Audio/MrsHall/%s.%s"), *AudioKey, *AudioKey);
	if (USoundBase* Clip = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySound2D(World, Clip);
	}
}

// From the editor console the given world is the editor's; the player lives in PIE.
static UWorld* GeneratePlayWorld(UWorld* Given)
{
	if (!(Given && Given->IsGameWorld()) && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && Context.World()->IsGameWorld())
			{
				return Context.World();
			}
		}
	}
	return Given;
}

/* Type a Generate request without the panel - the same SubmitRequest the G panel calls, so the
   matcher, the sauce, THE GIST and Mrs. Hall all run. For editor tests of what words build;
   the power gate is the panel's, so this skips it. */
static FAutoConsoleCommandWithWorldAndArgs GGenerateRequest(
	TEXT("sib.Generate.Request"),
	TEXT("Submits the rest of the line as a Generate request for the first player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		World = GeneratePlayWorld(World);
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (UGenerateComponent* Generate = Pawn ? Pawn->FindComponentByClass<UGenerateComponent>() : nullptr)
		{
			Generate->SubmitRequest(FString::Join(Args, TEXT(" ")));
		}
	}));

/* Forget every generated object of one kind (an EntryId: "tree", "gist") in the open level, in
   the world AND in the save, with no refund - for clearing up after a test. Dismantling would
   refund Books for something nobody paid Books for; this only forgets. */
static FAutoConsoleCommandWithWorldAndArgs GGenerateForget(
	TEXT("sib.Generate.Forget"),
	TEXT("Forgets every generated object with this EntryId in the open level, save included, without a refund."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		World = GeneratePlayWorld(World);
		UProgressionSubsystem* Progression = World ? UProgressionSubsystem::Get(World) : nullptr;
		if (!Progression || Args.Num() == 0)
		{
			return;
		}
		const FName EntryId(*Args[0]);
		const FName Level(*UWorld::RemovePIEPrefix(World->GetMapName()));
		int32 Forgotten = 0;
		for (const FGeneratedSiteRecord& R : Progression->GeneratedSitesForLevel(Level))
		{
			if (R.EntryId == EntryId)
			{
				Progression->ForgetGeneratedSite(R.ObjectId);
				++Forgotten;
			}
		}
		TArray<ABuildSite*> Doomed;
		for (TActorIterator<ABuildSite> It(World); It; ++It)
		{
			if (It->IsGenerated() && It->GetGenerateEntryId() == EntryId)
			{
				Doomed.Add(*It);
			}
		}
		for (ABuildSite* Site : Doomed)
		{
			Site->Destroy();
		}
		UE_LOG(LogSibeliusGame, Display, TEXT("[Generate] forgot %d '%s' record(s) and removed %d object(s) in %s."),
			Forgotten, *EntryId.ToString(), Doomed.Num(), *Level.ToString());
	}));
