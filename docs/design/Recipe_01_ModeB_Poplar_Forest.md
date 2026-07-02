# World Recipe Worksheet — Recipe #1
## "Mode B Poplar Forest"

Project: Leonard Sibelius (UE 5.7) · Level: L_Poplar_Forest · Kit: EasyBiomes Poplar
Status: Step 2 complete (palette identified). Open items marked ⬜.

---

## Block A — Identity

| # | Field | Value |
|---|-------|-------|
| 1 | Recipe Name | Mode B Poplar Forest |
| 2 | Compatible Terrain Stages | Stage 1 (L_Poplar_Forest terrain) |
| 3 | Selection Weight | (deferred — default 1) |

## Block B — Vegetation Palette
*Design decision: palette references EasyBiomes **collections** (DT_..._Collection data tables), not individual meshes. Variants inside a collection count as one pick.*

| # | Field | Value |
|---|-------|-------|
| 4 | Hero Candidates | ⬜ TBD in Step 5 — pick largest variant inside DT_Poplar_Collection and/or a big rock from DT_Cliff_Collection |
| 5 | Canopy Set | DT_Poplar_Collection, DT_BoxElder_Collection |
| 6 | Midstory Set | DT_Cliff_Collection, DT_Branches_01_Collection *(no bush collection in this row — bushes live in the Fiedls_With_Bushes row)* |
| 7 | Ground Scatter Set | DT_MeadowPlants_Collection, DT_GrassCurly_Collection, DT_GrassPatches_Collection, DT_Fern_Collection, DT_Nettle_Collection |
| 8 | Accent Set | (empty on purpose — Recipe #1 is the pure baseline) |

## Block C — Mood

| # | Field | Value |
|---|-------|-------|
| 9 | Lighting Preset | "Mood_ForestMorning" = current level lighting: DirectionalLight + ExponentialHeightFog (hazy coastal morning, warm-green). ⬜ Capture exact values in Step 4. |
| 10 | Audio Ambience | (deferred) |

## Block D — Composition Parameters

| # | Field | Value |
|---|-------|-------|
| 11 | Densities (canopy / midstory / scatter) | 0.6 / 0.5 / 0.8 (starting values, tune by eye in Step 3) |
| 12 | Clearing Size Range | 8–20 m radius |
| 13 | Path Width | 3 m |
| 14 | Edge Falloff | (deferred — graph default) |

## Block E — Surprise Object

| # | Field | Value |
|---|-------|-------|
| 15 | Surprise Pool Tag | "any" |
| 16 | Integration Scatter Set | (deferred — defaults to item 7) |

## Block F — Budgets & Shinbi

| # | Field | Value |
|---|-------|-------|
| 17 | Instance Budget (canopy / midstory / scatter) | 400 / 1,500 / 25,000 — ceilings, profile in Step 8 |
| 18 | Shinbi Pose/Anim Tag | (deferred — default idle) |

---

## Discovered kit architecture (reference notes)

- Each **BP_Biome_Poplar** actor = one spline region. Its Details → Biome Preset picks a **Data Table + Row Name**.
- **DT_Biome_Poplar** (Content/EasyBiomes/PCG/BiomePresets) has 4 rows = 4 pre-made looks:
  Forest_With_Meadows · Fiedls_With_Bushes (kit's typo) · Forest_Dead · Forest_Dense
- Each row = MeshCollections array + BiomeGraph + CoverageGraph + TerrainGraph.
  Forest_With_Meadows uses the 9 collections listed in Blocks B above.
- Rows share collections and vary the mix → a row is already a "mini-recipe."
  Our recipe system can work by pointing biome actors at **our own data table rows** in the same format.
- Mesh collection tables live in Content/EasyBiomes/Foliage/Trees/Poplar/ (and BoxElder/).
- ✅ Row assignments confirmed — each spline region uses a different row:
  | Actor | Row |
  |---|---|
  | BP_Biome_Poplar | Forest_With_Meadows |
  | BP_Biome_Poplar2 | Fiedls_With_Bushes |
  | BP_Biome_Poplar3 | Forest_Dense |
  | BP_Biome_Poplar4 | Forest_Dead |
  → The level = 4 hand-drawn spline regions (Walt's composition) × 4 kit looks.
  → Variation levers for Step 3: (a) permute row assignments per region, (b) reseed the PCG graphs. Splines stay fixed per terrain stage — they ARE the authored composition.

## Step 3 findings (proven in L_Elsewhere_Dev)

- Row swap works: BP_Biome_Poplar4 changed Forest_Dead → Forest_With_Meadows, regenerated correctly.
- Reseed works: PCG_Biome component → Settings → Seed (was 42, tested 19). Full re-roll = set seed on PCG_Biome, PCG_Cover, PCG_Terrain.
- Regeneration cost: **< 1 second per region** via the kit's Spawn button (Details → Controls, actor Self selected).
- Correct workflow: use the kit's **Spawn / Clear** buttons, NOT the raw PCG Generate button. Bare Generate creates orphan PCGStampChild actors (duplicate forests); we deleted 4 of these.
- With Is Partitioned = on, Spawn output lives in **PCGWorldActor0** (engine-managed) — no loose actors, actor count stable at 543.
- Generation Trigger = "Generate on Demand" — biomes only rebuild when told. Perfect for door-open regeneration.
- Door-open sequence (proven by hand): Clear → set Row Name → set Seed → Spawn, per region.
- Known cosmetic issue: Clear logs warnings about stale WaterPlane/PostProcess refs (side effect of Save-As duplication). Non-fatal.
- Hero candidate spotted: SM_Poplar_01_Field_Trunk (open-grown "field" poplar, seen in stamp ISM list).
- Dev sandbox level: **L_Elsewhere_Dev** (copy of L_Poplar_Forest). Original level untouched.

## World Conductor v1 (WORKING — built in Content/Elsewhere/BP_WorldConductor)

- Actor blueprint placed in L_Elsewhere_Dev. Variables: BiomeRegions (BP_Biome_Poplar object ref array, instance-editable, holds regions 1–4), WorldSeed (int, instance-editable).
- Custom event **ConductWorld** (Call In Editor → button on the actor's Details panel):
  ForEachLoop over BiomeRegions → Set Seed (WorldSeed + Array Index) → call **Spawn** (the kit's Controls function).
- KEY LESSON: call the kit's plain **Spawn** function (under "Controls" category). It runs the full ceremony: Cleanup (self-cleans old output — no separate Clear needed) → flags → Spawn Runtime → Generate. Calling Spawn Terrain/Biome/Cover individually does nothing; Spawn Runtime alone is gated by the "Runtime Generation" checkbox (unticked; revisit when the door opens mid-gameplay).
- Full 4-region rebuild ≈ 71k PCG tasks, progress toast bottom-right, ~1 min. One button + one number = new world. ✔
- Logic lives in parent class **BP_Biome** (/EasyBiomes/PCG/Blueprints); BP_Biome_Poplar is a data-only child.
- Seed/density quirk RESOLVED: tested seeds 10 / 1,000 / 10,000 / 50,000 — no monotonic density trend, just normal per-seed variance. Any seed is fair game.
- Bonus discovery: the kit seeds living ambience too — BP_MeadowInsects and BP_WindDeciduous actors spawn per generation under PCGPartitionGridActor (actor count varies ~544–564; engine-managed, ignore).

## Open items

- ⬜ Hero candidate meshes (Step 5)
- ⬜ Exact lighting values for Mood_ForestMorning (Step 4)
- ⬜ Identify the green triangle billboard actors (possible future anchors)

## Build order (agreed plan)

1. ✅ Recipe schema (fields defined)
2. ✅ Recipe #1 on paper (this worksheet)
3. ✅ World Conductor v1 working: one world-seed → reseed + respawn all 4 regions. Next: row permutation per region (Set Biome Preset), then recipe data asset
4. Lighting/post-process preset applied on generation
5. Anchors on terrain (door, sightline, hero, surprise, Shinbi ×3)
6. Shinbi placement + sanity check
7. Surprise pool + integration pass
8. Async loading + budget caps, profile door-open time
9. Recipes #2–3, terrain stage 2

## Art-direction rules (quick reference)

1. One biome rules each world (~80/15/5)
2. Lighting is the glue — one locked mood per recipe
3. The door frames a postcard — arrival view always composed
4. Exactly one hero per world
5. Density in gradients, never uniform
6. Shinbi is the figure in the landscape — on the sightline, mid-distance
7. Surprise object = the wrong thing in the right place, integrated into the ground
8. Small palette, big repetition
9. Randomness only inside fences
