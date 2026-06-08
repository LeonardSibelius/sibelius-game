# Ch6 "Generate" — predict-bugs SPIKE findings (SIB-30)

**Status:** spike code written on `feat/sib-30`; gate (`-run=GenerateSmokeTest`) is Walt's to run.
**Scope reminder:** this is the *spike*, not the chapter — pure data + logic + a headless commandlet.
No gameplay UI, no PIE, no art. Confirms the G1–G6 ledger before P0.

**Locked design (Walt, June 8 — not re-decided here):** Option B = a curated, **closed** catalog of
pre-authored objects matched to natural-language input by **keyword/synonym overlap**, keywords in
**data**. Closed set ⇒ content-safe by construction. No-match / out-of-catalog / unsafe / over-budget
⇒ in-fiction **Mrs. Hall refusal**. Resolved spawns route through the existing **Ch3 build + Ch5
persist** pipeline. Offline, deterministic, no network, no RNG.

## What the spike builds
- `GenerateTypes.h` — `FGenerateCatalogEntry` (USTRUCT : `FTableRowBase`, so it's DataTable/CSV-ready),
  `EGenerateOutcome`, `FGenerateResolution`.
- `GenerateMatcher.h/.cpp` — `ClassifyGenerateRequest(input, catalog, remainingBudget)`, the **pure**
  matcher seam (tokenize → score by keyword overlap → highest wins → tie/no-match/over-budget
  refusals). This is the piece allowed to graduate into P0.
- `GenerateSmokeTestCommandlet` — headless G1–G6 gate, mirrors `BranchSmokeTest`. Disposable in-test
  catalog (ladder / lamp / crate / key) + a 12-row phrasing table.

## Verdict per G (predicted — the gate confirms)

| G | Assertion | Predicted |
|---|-----------|-----------|
| **G1 — scope trap** | No input ever resolves to a non-catalog `EntryId`; the only spawn path is a resolved id. | **PASS by construction** — `ClassifyGenerateRequest` can only return an `EntryId` that came from the passed catalog. |
| **G2 — varied phrasing** | 12 phrasings (≈3 per item, incl. "something to climb", "a light please", "storage container") resolve to the right entry; gibberish → `RefusedNoMatch`; a real keyword tie ("light box") → `RefusedAmbiguous`. | **PASS** — simple exact-token overlap clears every authored phrasing in the table. See "phrasings that miss" below. |
| **G3 — content safety** | Out-of-catalog / "unsafe" phrasings ("a gun", "set it on fire", "poison", "conjure a dragon") never resolve. | **PASS** — they score 0 → `RefusedNoMatch`. Nothing arbitrary enters the world. |
| **G4 — deterministic / offline** | Same `(input, catalog, budget)` → identical result; tokenizer stable across case/punctuation; no network / RNG / clock. | **PASS** — pure function; the only inputs are its arguments. |
| **G5 — budget economy** | A resolve costs its `Cost`; `Cost > RemainingBudget` → `RefusedOverBudget`, *distinct* from `RefusedNoMatch`. | **PASS** — over-budget is its own outcome, never conflated with no-match. |
| **G6 — persistence via the real pipeline** | A resolved entry, spawned as an `ABuildSite`, built with the real Ch3 `Build()` verb, gets a stable `FGuid` and round-trips a deploy → reset → `ApplyDeployedSave` (the real Ch5 path) back to BUILT. | **PASS** — reuses the proven Ch5 GUID-keyed deltas (`UBranchSubsystem` / `FSibeliusSaveIO`); confirmed against the real repo signatures, not assumed. |

### G2 phrasings that miss (→ P4 keyword-growth backlog)
With the disposable catalog's keyword lists, **none of the 12 authored phrasings are predicted to
miss** — they were written to contain a real keyword. That's the point of P4: in the *real* catalog,
player phrasings the keyword set doesn't yet cover are the growth signal. The spike's value is proving
the **mechanism** (exact-token overlap + tie/budget refusals) is sound; the matcher will need fuzzier
matching only if exact-token overlap proves too brittle in playtest (see DB/limitations).

## Matcher limitations surfaced (inform P-phasing, not blockers)
- **Exact-token only** — "ladders" (plural) or "lighting" won't match "ladder"/"light". Cheap future
  upgrades: stemming, or just adding plurals/variants to the keyword data (P4 is literally this).
- **No multi-word keywords** — "step ladder" matches on "ladder" anyway, but a keyword like
  "fire escape" can't be a single token. If needed, add phrase matching in P1.
- **Score is raw overlap count** — longer keyword lists are slightly favored on ties. Fine for now;
  revisit only if real-catalog ties get noisy.

## DECISIONS for Walt (design is yours — these are recommendations)

- **DA — catalog storage.** **Recommend: `UDataTable` backed by a CSV.** It's the lowest-friction
  place to *grow keywords over time* (spreadsheet-editable, no recompile, no editor round-trip), which
  is exactly the stated goal. `FGenerateCatalogEntry` already derives `FTableRowBase` so it's ready to
  be a DataTable row. (Alternative `UPrimaryDataAsset` is nicer for the soft `Mesh` ref but worse for
  bulk keyword editing.) *Spike used a throwaway C++ catalog so it depends on no asset.*
- **DB — tie-breaking.** **Recommend: `RefusedAmbiguous`** ("be more specific" — Mrs. Hall pushes
  back) over a silent deterministic pick. Better story, never a wrong guess. **Implemented this way in
  the spike** and asserted (G2/DB).
- **DC — safety blocklist.** **Recommend: defer to P2.** The closed catalog already guarantees safety,
  so it's not needed to prove the spike — out-of-catalog input already refuses. A small tone blocklist
  later just upgrades a generic no-match into a *pointed* `RefusedUnsafe` line. The enum value
  `RefusedUnsafe` exists as a hook; nothing sets it yet.
- **DD — input modality.** **Recommend: typed text box.** Offline, deterministic, accessible. Voice/STT
  would break G4 (needs network / is non-deterministic). Confirm typed.

## Recommended next step (P0)
Once Walt confirms **DA (DataTable/CSV)** and **DB (RefusedAmbiguous, already done)**, the matcher seam
graduates as-is into P0: swap the throwaway C++ catalog for a CSV-backed `UDataTable`, and wire
`ClassifyGenerateRequest`'s `Resolved` outcome to spawn the entry's `Mesh`/buildable through the Ch3
build entry point. G6 already proves the spawned result persists.
