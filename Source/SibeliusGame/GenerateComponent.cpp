// GenerateComponent.cpp — SIB-30 Ch6 P1. Resolve a typed request -> spawn / refuse.

#include "GenerateComponent.h"
#include "SibeliusHUD.h"   // player-facing messages draw on the HUD canvas (Shipping-safe)
#include "GenerateMatcher.h"
#include "GenerateCatalog.h"
#include "MrsHallLines.h"           // P2: refusal lines + DC blocklist (data-driven)
#include "MrsHallMessageWidget.h"   // P2: the styled refusal memo

#include "BuildSite.h"
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

// SIB-30 P3 — shared generated-site authoring. ONE place that turns a catalog entry into a
// built, tagged ABuildSite, so live generation and re-spawn-on-load can't diverge.
void AuthorGeneratedSite(ABuildSite* Site, const FGenerateCatalogEntry& Entry)
{
	if (!Site)
	{
		return;
	}

	// Author the catalog mesh + per-entry transform onto the buildable's FinalMesh
	// (SIB-40-in-data — e.g. the key's Scale 0.25 / Yaw 90). Soft-load is cached/cheap.
	UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
	if (Site->FinalMesh)
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
	ABuildSite* Site = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SavedTransform, SpawnParams);
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
}

const FGenerateCatalogEntry* UGenerateComponent::FindEntry(const FName& Id) const
{
	return Catalog.FindByPredicate([&Id](const FGenerateCatalogEntry& E) { return E.EntryId == Id; });
}

EGenerateOutcome UGenerateComponent::SubmitRequest(const FString& RawText)
{
	const FGenerateResolution R = ClassifyGenerateRequest(RawText, Catalog, RemainingBudget, Blocklist);

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
	ShowMrsHall(Line);
	PlayMrsHallClip(Picked.AudioKey); // P2.5: her voice alongside the memo (silent until clips exist)
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

	// Clamp the forward distance to the nearest wall/window so the object never spawns
	// beyond it (outside the house). Trace forward at chest height (~capsule center).
	float EffectiveAhead = SpawnAheadDistance;
	{
		const FVector WallStart = PlayerLoc;
		const FVector WallEnd = WallStart + Fwd * SpawnAheadDistance;
		FHitResult WallHit;
		FCollisionQueryParams WallParams(SCENE_QUERY_STAT(GenerateWallTrace), false, Pawn);
		WallParams.AddIgnoredActor(Pawn);
		if (World->LineTraceSingleByChannel(WallHit, WallStart, WallEnd, ECC_WorldStatic, WallParams))
		{
			EffectiveAhead = FMath::Max(0.0f, WallHit.Distance - 50.0f); // just IN FRONT of the wall
		}
	}
	const FVector ForwardPoint = PlayerLoc + Fwd * EffectiveAhead;

	// Downward floor trace. Start BELOW the ceiling but above the floor (player head
	// height ~= player.Z + 100), then trace straight DOWN a long way so the FIRST hit is
	// the floor — NOT the ceiling. (Starting above the ceiling made the first hit the
	// ceiling, dropping the object overhead.)
	const FVector TraceStart(ForwardPoint.X, ForwardPoint.Y, PlayerLoc.Z + 100.0f);
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 3000.0f);
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
	ABuildSite* Site = World->SpawnActor<ABuildSite>(ABuildSite::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (!Site)
	{
		return false; // confirmed failure — budget must NOT charge (handled by caller)
	}

	// Author + tag + present BUILT via the SHARED path (SIB-30 P3) — the same call
	// re-spawn-on-load uses, so a generated object looks identical fresh or reloaded. The
	// actor's GUID is assigned by the normal IBranchable fallback (BeginPlay); Deploy then
	// persists it + this site's EntryId + transform so reload can re-create it.
	AuthorGeneratedSite(Site, Entry);

	// Budget charges only because we reached here with a real, created actor.
	return true;
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
