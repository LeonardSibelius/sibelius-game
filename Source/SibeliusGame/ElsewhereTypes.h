// ElsewhereTypes.h
//
// THE SAUCE DOOR — "Elsewhere" feature (SIB-47, June 17 2026). Data model for the
// procedurally-assembled places behind the kitchen's Sauce Door. See the design:
// walt-cowork-memory/sibelius-game-sauce-door-design.md.
//
// Design pillar is WONDER, not greed: you ALWAYS find a curio (no losing); rarity
// is a bonus, never a punishment. Each Elsewhere is generated from a SEED on entry,
// held in memory, and DISCARDED on exit — only the collected curios + score persist.
//
// Everything here is CONTENT, not code, on the same content-as-data philosophy as
// CarouselTypes.h: place-types and curios are FTableRowBase rows so they're
// live-tunable (DataTable in the editor, or the code-default registry in
// ElsewhereGen for a fresh clone). The generator (FElsewhereGen) is pure + headless
// so the whole loop gates in a commandlet, exactly like the Carousel sim.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ElsewhereTypes.generated.h"

// Wonder-flavored rarity. NOT a win/lose axis — a Common curio is still a delight;
// the rare/legendary ones are the "ooh, I haven't seen THAT before" bonus (§6).
UENUM(BlueprintType)
enum class EElsewhereRarity : uint8
{
	Common     UMETA(DisplayName = "Common"),
	Rare       UMETA(DisplayName = "Rare"),
	Legendary  UMETA(DisplayName = "Legendary")
};

// One place-type = a mood + a modular kit + variation rules + the curios it tends to
// hold (§5). A DataTable row so adding place-types is content work, not a recompile —
// the ongoing (fun) job per the design. The mesh/material refs are SOFT: the headless
// generator never loads them; only the runtime builder does.
USTRUCT(BlueprintType)
struct FPlaceTypeDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place") FText DisplayName;

	// Picked weighted on entry (a heavier place shows up more often). Variety is the
	// whole game (§7), so defaults keep these even.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place", meta = (ClampMin = "1")) int32 Weight = 10;

	// --- Mood (§4 variation levers): lighting + color + fog. The builder reads these
	// to set the room's atmosphere; the sim ignores them. ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Mood") FLinearColor AmbientColor = FLinearColor(0.05f, 0.06f, 0.08f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Mood") FLinearColor CurioGlowColor = FLinearColor(0.9f, 0.8f, 0.4f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Mood", meta = (ClampMin = "0.0")) float FogDensity = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Mood", meta = (ClampMin = "0.0")) float LightIntensity = 2.0f;

	// --- Layout (the PCG/assembly knobs). Room footprint in cm + how densely props
	// scatter. A seed jitters within these so each visit to the SAME place-type still
	// differs (§4). ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Layout") FVector RoomExtent = FVector(800.f, 800.f, 400.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Layout", meta = (ClampMin = "0", ClampMax = "64")) int32 PropCountMin = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Layout", meta = (ClampMin = "0", ClampMax = "64")) int32 PropCountMax = 14;

	// The curios this place can hold (§6 — the artifact must FIT its place). At least
	// one required; the generator rarity-weights the choice among these.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place") TArray<FName> CurioPool;

	// Presentation refs (soft — generator never touches them). The modular kit tile
	// (floor/wall) and an optional hero piece for signature flair (§4).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Art") TSoftObjectPtr<UStaticMesh> TileMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place|Art") TSoftObjectPtr<UStaticMesh> PropMesh;
};

// One curio = a strange, impossible artifact fitting its place (§6). DataTable row.
// FlavorNote is Leonard's handwriting (§6 optional depth) — threads light narrative
// through the collection.
USTRUCT(BlueprintType)
struct FCurioDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curio") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curio") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curio") EElsewhereRarity Rarity = EElsewhereRarity::Common;

	// Leonard's handwritten note for the Cabinet (§6).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curio") FText FlavorNote;

	// Weight WITHIN a place's pool. Rarer curios carry a lower default weight; the
	// generator normalises across whatever pool the place offers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curio", meta = (ClampMin = "1")) int32 Weight = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curio|Art") TSoftObjectPtr<UStaticMesh> Mesh;
};

// The deterministic RESULT of rolling a seed: which place, which curio, plus derived
// sub-seeds for layout + mood jitter. This is the throwaway thing — "generated from a
// seed on entry, held in memory, discarded on exit" (§4). NEVER saved (the discard
// rule); reproducible from Seed alone if ever needed.
USTRUCT(BlueprintType)
struct FElsewherePlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Elsewhere") int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Elsewhere") FName PlaceTypeId;
	UPROPERTY(BlueprintReadOnly, Category = "Elsewhere") FName CurioId;
	UPROPERTY(BlueprintReadOnly, Category = "Elsewhere") int32 LayoutSeed = 0;   // prop placement
	UPROPERTY(BlueprintReadOnly, Category = "Elsewhere") int32 MoodSeed = 0;     // lighting/fog jitter

	bool IsValid() const { return !PlaceTypeId.IsNone() && !CurioId.IsNone(); }
};

// One curio the player OWNS — the persisted unit (§3 "save curios + score"). Kept
// deliberately tiny: this is ALL that survives the room's discard, alongside score.
USTRUCT(BlueprintType)
struct FCollectedCurio
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Collection") FName CurioId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Collection") FName PlaceTypeId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Collection") EElsewhereRarity Rarity = EElsewhereRarity::Common;
};

// Pure-data collection state (the Cabinet's source of truth). PLAIN struct + methods
// on the FCarouselRun pattern: the subsystem wraps it, but the smoke gate drives it
// directly (a UGameInstanceSubsystem can't be NewObject'd headless). Owned is the
// UNIQUE set (a curio fills exactly one Cabinet slot); Score and TotalCollected still
// climb on duplicates so "go again" stays rewarding once the Cabinet is full.
USTRUCT(BlueprintType)
struct FCurioCollection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Collection") TArray<FCollectedCurio> Owned;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Collection") int32 Score = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Collection") int32 TotalCollected = 0;

	bool OwnsCurio(const FName& CurioId) const
	{
		return Owned.ContainsByPredicate([&CurioId](const FCollectedCurio& C) { return C.CurioId == CurioId; });
	}

	// Wonder-points per rarity — the gentle "did I find a rare one?" without a lose
	// condition. Common always scores; rarer scores more.
	static int32 RarityScore(EElsewhereRarity Rarity)
	{
		switch (Rarity)
		{
			case EElsewhereRarity::Legendary: return 25;
			case EElsewhereRarity::Rare:      return 10;
			default:                          return 3;
		}
	}

	// Add a collected curio. ALWAYS scores + bumps TotalCollected (every find is a
	// delight). Returns true iff this curio is NEW to the Cabinet (a fresh slot fills).
	bool Add(const FCurioDef& Def, const FName& PlaceTypeId)
	{
		Score += RarityScore(Def.Rarity);
		++TotalCollected;

		if (OwnsCurio(Def.Id))
		{
			return false;   // duplicate — score climbed, but no new Cabinet slot
		}

		FCollectedCurio Entry;
		Entry.CurioId = Def.Id;
		Entry.PlaceTypeId = PlaceTypeId;
		Entry.Rarity = Def.Rarity;
		Owned.Add(Entry);
		return true;
	}
};
