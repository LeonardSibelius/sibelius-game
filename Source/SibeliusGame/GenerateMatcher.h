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
//   2. DECISION DC: any token in Blocklist -> RefusedUnsafe (checked BEFORE scoring, so an
//      obviously-bad word gets a pointed refusal, not a generic no-match). Empty Blocklist
//      (the default) skips this step — keeps the pre-P2 3-arg calls behaving identically.
//   3. score each entry = # of input tokens matching any of its keywords (exact token)
//   4. highest score wins; 0 -> RefusedNoMatch; top-score tie -> RefusedAmbiguous (DB)
//   5. resolved but Cost > RemainingBudget -> RefusedOverBudget
//   6. otherwise Resolved(EntryId, Cost)
// Pure: same (input, catalog, budget, blocklist) always yields the same result.
SIBELIUSGAME_API FGenerateResolution ClassifyGenerateRequest(
	const FString& RawInput,
	const TArray<FGenerateCatalogEntry>& Catalog,
	int32 RemainingBudget,
	const TArray<FString>& Blocklist = TArray<FString>());
