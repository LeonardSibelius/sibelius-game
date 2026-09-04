// GenerateComponent.cpp — SIB-30 Ch6 P1. Resolve a typed request -> spawn / refuse.

#include "GenerateComponent.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
#include "GenerateMatcher.h"
#include "GenerateCatalog.h"
#include "MrsHallLines.h"           // P2: refusal lines + DC blocklist (data-driven)
#include "MrsHallMessageWidget.h"   // P2: the styled refusal memo
#include "MrsHallSubsystem.h"       // SPINE Move 2: the one channel she speaks through

#include "BuildSite.h"
#include "Branchable.h"              // GetBranchId - the id a record is keyed on
#include "ProgressionSubsystem.h"    // where generated objects are remembered
#include "SibeliusGame.h"            // LogSibeliusGame
#include "EngineUtils.h"             // TActorIterator - what is already here
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
		UE_LOG(LogTemp, Display, TEXT("[Generate] catalog loaded: %d entries, budget %d."), Catalog.Num(), RemainingBudget);
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
}

const FGenerateCatalogEntry* UGenerateComponent::FindEntry(const FName& Id) const
{
	return Catalog.FindByPredicate([&Id](const FGenerateCatalogEntry& E) { return E.EntryId == Id; });
}

EGenerateOutcome UGenerateComponent::SubmitRequest(const FString& RawText)
{
	const FGenerateResolution R = ClassifyGenerateRequest(RawText, Catalog, RemainingBudget, Blocklist);

	/* SAY WHAT WAS ASKED AND WHAT WAS DECIDED — every time, refusal or not.

	   Mrs. Hall is deliberately in-fiction: she says "we don't keep that here" and never
	   which of four reasons that was, which is right for the player and useless for
	   anyone debugging. Twice now a refusal has had to be diagnosed by inference, once
	   from a screenshot of her memo. The resolution already carries RefusalReason; it was
	   simply never written down. One line, and the next "why did Generate say no" is a
	   log search instead of an argument. */
	UE_LOG(LogTemp, Display,
		TEXT("[Generate] request \"%s\" -> outcome %d (%s), entry '%s', cost %d, budget %d."),
		*RawText, static_cast<int32>(R.Outcome),
		R.RefusalReason.IsEmpty() ? TEXT("resolved") : *R.RefusalReason,
		*R.EntryId.ToString(), R.Cost, RemainingBudget);

	// Success path is unchanged: the object appears + a small neutral confirm. Don't over-style.
	if (R.Outcome == EGenerateOutcome::Resolved)
	{
		if (const FGenerateCatalogEntry* E = FindEntry(R.EntryId))
		{
			if (SpawnEntry(*E))
			{
				RemainingBudget -= R.Cost;
				bHasSpawnedThisSession = true; // SIB-31: the cathedral door's generate gate
				Toast(FString::Printf(TEXT("%s appears.   (budget %d)"), *E->DisplayName.ToString(), RemainingBudget), FColor::Green);
			}
			else
			{
				Toast(TEXT("...nothing came (could not place it)."), FColor::Red);
			}
		}
		return R.Outcome;
	}

	// Refusal (no-match / ambiguous / over-budget / unsafe): Mrs. Hall, in her own voice,
	// from data — rotated deterministically. The styled memo replaces the P1 placeholder text.
	const FMrsHallLine Picked = PickMrsHallLine(RefusalLines, R.Outcome, RefusalCount++);
	FString Line = Picked.Line;
	if (Line.IsEmpty())
	{
		Line = TEXT("Absolutely not."); // last-ditch fallback if the lines table is missing
	}
	/* Presentation now goes through UMrsHallSubsystem (docs/SPINE.md Move 2) — the same
	   memo, timer and clip path every other beat in the game uses, so there is exactly one
	   copy of "Mrs. Hall says something". The LINE SELECTION stays here: Chapter 6 picks
	   from its own EGenerateOutcome-keyed refusal table, which is untouched. */
	if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
	{
		Hall->SayLine(Line, Picked.AudioKey);   // silent until the clip is recorded
	}
	else
	{
		ShowMrsHall(Line);   // no world subsystem (headless): the local fallback still works
	}
	return R.Outcome;
}

bool UGenerateComponent::SpawnEntry(const FGenerateCatalogEntry& Entry)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
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

	/* HOW FAR AHEAD THIS PARTICULAR THING WANTS TO BE. A lamp is happy at 250 cm; a
	   launch complex is not, and putting one there trapped Walt inside it. */
	const float RequestedAhead = (Entry.SpawnAhead > 0.0f) ? Entry.SpawnAhead : SpawnAheadDistance;

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
	if (Entry.SpawnAhead <= 0.0f)
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
	const float TraceTop = (Entry.SpawnAhead > 0.0f)
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
	FVector SpawnLoc = bHit ? Hit.ImpactPoint : FVector(ForwardPoint.X, ForwardPoint.Y, FeetZ);

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
	RememberSite(Site, Entry);

	// Budget charges only because we reached here with a real, created actor.
	return true;
}

void UGenerateComponent::RememberSite(ABuildSite* Site, const FGenerateCatalogEntry& Entry)
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
			*Entry.EntryId.ToString());
		return;
	}

	Progression->RememberGeneratedSite(CurrentLevelName(World), Entry.EntryId,
		Site->GetActorTransform(), Id);
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
