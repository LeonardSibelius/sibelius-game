// ElsewhereGen.h
//
// THE SAUCE DOOR — the pure, headless generation core (SIB-47). Same shape as
// FCarouselSim: static math + a code-default content slice so the feature works on a
// fresh clone with no authored DataTables, and the whole thing gates in a commandlet.
//
// The ONE determinism guarantee: RollPlan(Seed) is a pure function of (Seed, content).
// Same seed -> same place + curio + layout (the Carousel smoke test's determinism
// rule, applied to wonder). Different seeds -> variety. That's what lets the room be
// "discarded on exit, reproducible from seed if ever needed" (§4) — and what the gate
// asserts.

#pragma once

#include "CoreMinimal.h"
#include "ElsewhereTypes.h"

struct SIBELIUSGAME_API FElsewhereGen
{
	// The code-default content slice (MVP §9: ~3-4 place-types). Used when the
	// DataTables are absent — a fresh clone still plays the loop. The editor can
	// override via /Game/Data/DT_ElsewherePlaces + DT_ElsewhereCurios (see
	// UElsewhereSubsystem); these defaults mirror the seed CSVs in Data/.
	static void BuildDefaultPlaceTypes(TArray<FPlaceTypeDef>& OutPlaces);
	static void BuildDefaultCurios(TArray<FCurioDef>& OutCurios);

	// THE roll. Deterministic from Seed alone given the same content. Picks a place
	// (weighted), then a curio from THAT place's pool (weighted) — so the artifact
	// always fits its place (§6) — and derives layout/mood sub-seeds. Returns an
	// invalid plan only if the content is empty/malformed (no place, or a place with
	// an empty pool).
	static FElsewherePlan RollPlan(
		int32 Seed,
		const TArray<FPlaceTypeDef>& Places,
		const TArray<FCurioDef>& Curios);

	// Lookups (linear — the registries are tiny). Null if absent.
	static const FPlaceTypeDef* FindPlace(const TArray<FPlaceTypeDef>& Places, const FName& Id);
	static const FCurioDef* FindCurio(const TArray<FCurioDef>& Curios, const FName& Id);
};
