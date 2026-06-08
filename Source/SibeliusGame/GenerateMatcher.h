// GenerateMatcher.h
//
// SIB-30 — Ch6 SPIKE. The PURE matcher seam. ClassifyGenerateRequest's output depends
// only on (input, catalog, budget): no network, no RNG, no clock — so it's offline and
// deterministic (G4). This is the piece allowed to graduate into P0 once it's green.

#pragma once

#include "CoreMinimal.h"
#include "GenerateTypes.h"

// Normalize (lowercase, strip punctuation) + tokenize on whitespace. Exposed so the
// smoke test can introspect tokenization.
SIBELIUSGAME_API TArray<FString> TokenizeGenerateInput(const FString& RawInput);

// Resolve a natural-language request against the closed catalog:
//   1. tokenize the input
//   2. score each entry = # of input tokens matching any of its keywords (exact token)
//   3. highest score wins; 0 -> RefusedNoMatch; top-score tie -> RefusedAmbiguous (DB)
//   4. resolved but Cost > RemainingBudget -> RefusedOverBudget
//   5. otherwise Resolved(EntryId, Cost)
// Pure: same (input, catalog, budget) always yields the same result.
SIBELIUSGAME_API FGenerateResolution ClassifyGenerateRequest(
	const FString& RawInput,
	const TArray<FGenerateCatalogEntry>& Catalog,
	int32 RemainingBudget);
