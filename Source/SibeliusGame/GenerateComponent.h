// GenerateComponent.h
//
// SIB-30 — Ch6 P1. Player-side Generate driver: owns the per-session generation budget
// and the loaded catalog, resolves a typed request through the pure matcher, and spawns
// the result as a real ABuildSite (so it rides the Ch3/Ch5 pipeline). Lives on the
// character, mirroring UBuildComponent.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GenerateTypes.h"
#include "MrsHallLines.h"   // FMrsHallLine (line + AudioKey)
#include "GenerateComponent.generated.h"

class UMrsHallMessageWidget;
class ABuildSite;
class UWorld;
struct FGenerateCatalogEntry;

// SIB-30 P3 — shared generated-site spawn helpers, used by BOTH live generation
// (UGenerateComponent::SpawnEntry) and re-spawn-on-load (UBranchSubsystem::ApplyDeployedSave)
// so the two paths can never drift. Neither touches the generation budget — budget is the
// caller's concern (live generation charges it; a re-spawn is a pure recreate and must not).

// Author a resolved catalog entry onto an ALREADY-spawned site: catalog mesh + per-entry
// FinalMesh transform (SIB-40-in-data), tag it generated with its EntryId, present it BUILT.
// bFreshlyGenerated: TRUE only on the live path, where the player is standing there
// watching. It fires ABuildSite::OnGeneratedFresh so a site that performs an arrival
// (ASpaceport) performs it once. The reload path leaves it false and stays silent —
// see the note on that hook in BuildSite.h.
SIBELIUSGAME_API void AuthorGeneratedSite(ABuildSite* Site, const FGenerateCatalogEntry& Entry,
	bool bFreshlyGenerated = false);

// Re-create a generated site from a save record: spawn an ABuildSite at SavedTransform
// VERBATIM (no placement re-trace, P3-3), force SavedId as the stable identity (P3-4), then
// author it from Entry. Returns the new actor (null on failure).
SIBELIUSGAME_API ABuildSite* RespawnGeneratedSite(UWorld* World, const FGenerateCatalogEntry& Entry,
	const FTransform& SavedTransform, const FGuid& SavedId);

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API UGenerateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGenerateComponent();

	// Resolve a typed request: spawn on Resolved (decrementing the budget) or show a
	// distinct refusal line per reason (the P1 seed of the Mrs. Hall refusal). Returns
	// the matcher outcome.
	EGenerateOutcome SubmitRequest(const FString& RawText);

	int32 GetRemainingBudget() const { return RemainingBudget; }
	int32 GetCatalogNum() const { return Catalog.Num(); }

	// SIB-31 (Ch7): true once LIVE generation has spawned at least once this session.
	// Session-local only (never persisted; deploy re-spawns don't count) — the
	// cathedral door's optional bRequireGenerateUse gate polls this. Cheap + headless-safe.
	bool HasSpawnedThisSession() const { return bHasSpawnedThisSession; }

	// SIB-30 P3: restore the persisted generation budget on deploy-reload (Deploy saves it,
	// ApplyDeployedSave writes it back). A pure restore — does NOT re-charge for re-spawns.
	void SetRemainingBudget(int32 InBudget) { RemainingBudget = InBudget; }

protected:
	virtual void BeginPlay() override;

	/** Writes back the branch state of every generated object as the level tears down —
	    so a Test-Drive discard survives leaving and returning. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Per-session generation budget (the in-fiction economy). Tunable; Walt curates.
	UPROPERTY(EditAnywhere, Category = "Generate")
	int32 RemainingBudget = 10;

	// How far in front of the player to place a generated object before the floor trace.
	UPROPERTY(EditAnywhere, Category = "Generate")
	float SpawnAheadDistance = 250.0f;

private:
	bool SpawnEntry(const FGenerateCatalogEntry& Entry);
	const FGenerateCatalogEntry* FindEntry(const FName& Id) const;

	/* PERSISTENCE ACROSS A LEVEL CHANGE. UBranchSubsystem is a WORLD subsystem and dies
	   with the world, so before this a generated object survived only a manual Deploy —
	   walk out of L_City and your lamp was gone. Walt found it with the spaceport, which
	   is the first generated thing the story has to leave and return to. */
	void RememberSite(ABuildSite* Site, const FGenerateCatalogEntry& Entry);

	/** Called from BeginPlay: put back everything recorded for the level we just entered. */
	void RestoreGeneratedSites();

	/** The open level's name with any PIE prefix removed — records must match in both. */
	static FName CurrentLevelName(const UWorld* World);
	void Toast(const FString& Msg, const FColor& Color) const;

	// P2: present Mrs. Hall's refusal as a styled memo (auto-dismissed); helper-toast fallback.
	void ShowMrsHall(const FString& Line);
	void DismissMrsHall();

	// P2.5: play the line's pre-generated voice clip alongside the memo. Silent + harmless
	// if the clip isn't imported yet; never runs headless (no audio device).
	void PlayMrsHallClip(const FString& AudioKey);

	TArray<FGenerateCatalogEntry> Catalog;

	// P2 data (loaded in BeginPlay): refusal lines grouped by outcome + the DC blocklist.
	TMap<EGenerateOutcome, TArray<FMrsHallLine>> RefusalLines;
	TArray<FString> Blocklist;

	// Rotates Mrs. Hall's lines deterministically across refusals (NOT RNG/clock).
	int32 RefusalCount = 0;

	// SIB-31: set on the first successful live SpawnEntry; see HasSpawnedThisSession().
	bool bHasSpawnedThisSession = false;

	UPROPERTY()
	TObjectPtr<UMrsHallMessageWidget> MrsHallWidget;

	FTimerHandle MrsHallDismissTimer;
};
