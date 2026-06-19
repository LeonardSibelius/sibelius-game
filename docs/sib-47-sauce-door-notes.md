# SIB-47 — The Sauce Door (the "Elsewhere" wonder loop) — MVP notes

**Status:** code prototype written (June 17, 2026). Headless gate `-run=ElsewhereSmokeTest`
is Walt's to run. Editor assembly (the map + placement + DataTable import + materials) is
the follow-up — see **Editor follow-up** below.

Design source: `walt-cowork-memory/sibelius-game-sauce-door-design.md`. The pillar is
**wonder, not greed**: you always find a curio (no losing); rarity is a bonus. This MVP
proves the loop *step through → see a wild place → grab a curio → watch the Cabinet fill →
go again* (§9).

## What the prototype gives you (all C++, all in `Source/SibeliusGame/`)

| Piece | File | Role |
|---|---|---|
| Content model | `ElsewhereTypes.h` | place-type + curio DataTable rows, the throwaway `FElsewherePlan`, the persisted `FCollectedCurio` / `FCurioCollection`. Content-as-data like `CarouselTypes.h`. |
| Generation core | `ElsewhereGen.{h,cpp}` | **pure + headless**: `RollPlan(seed)` → deterministic place + curio + layout/mood sub-seeds; the code-default registry of 5 place-types × 15 curios (works on a fresh clone). The `FCarouselSim` of this feature. |
| Generation brain | `ElsewhereSubsystem.{h,cpp}` | `UGameInstanceSubsystem`: loads the registry (DataTable-or-default), **stages** a plan that survives the level travel, **discards** it on return. |
| Collection / Cabinet truth | `CurioCollectionSubsystem.{h,cpp}` + `ElsewhereSaveGame.h` | owns the collection + score; saves **curios + score only** through `FSibeliusSaveIO`. The save type structurally can't hold a room (the discard rule, §4). |
| Kitchen trigger | `SauceDoor.{h,cpp}` | **subclass of `AHiddenDoor`** — reuses the game's Code-Vision reveal exactly (hold **V**, the panel shimmers in, like the office/attic hidden doors). The only override: when revealed, **E stages an Elsewhere + travels** instead of opening a fixed `TravelTargetLevel`. Reveal == armed (no separate Sauce-completion gate). |
| The collectable | `Curio.{h,cpp}` | the one glowing curio (`ABookPickup` pattern); E writes to the collection subsystem, then destroys. |
| Room assembler | `ElsewhereBuilder.{h,cpp}` | reads the staged plan, assembles a seeded modular room — **floor + ceiling tiles, perimeter walls with a doorway, scattered props** from the place-type's **kit palette** (one ISM per mesh), mood light — spawns the curio + the return door. Kit-absent → engine-shape fallback. |
| The way home | `ReturnDoor.{h,cpp}` | E discards the staged plan and travels back to the house. |
| The wonder's home | `CabinetOfCuriosities.{h,cpp}` | one slot per known curio, lit when owned; fills as you collect. |
| Ship gate | `SibeliusGameEditor/ElsewhereSmokeTestCommandlet.{h,cpp}` | proves the whole loop headless (see below). |

## Dressing: place-type → kit palette (SIB-47 dressing pass)

A place-type carries a **modular kit palette** (`FPlaceTypeDef`: `FloorMeshes`/`WallMeshes`/
`CeilingMeshes`/`PropMeshes` soft-ref lists + `KitTileSize`/`KitWallHeight`/`KitMeshScale`).
`AssembleGeometry()` lays a real room from it, seeded from the run:

- **Floor + ceiling** tiled across `RoomExtent` on the `KitTileSize` grid.
- **Perimeter walls** on all four edges, with a **doorway gap** on the west edge (the
  return door stands there).
- **Props** scattered in the interior (count = `RandRange(PropCountMin, PropCountMax)` — the
  gate's determinism handle), random yaw, jittered scale.

Each distinct mesh gets its own `UInstancedStaticMeshComponent` (created on demand). A kit
mesh is placed at `KitMeshScale` (authored to its grid); a **missing** kit mesh (soft-ref
fails to load) falls back to a stretched engine shape — so the room **structure always
renders** and the headless gate is green with **zero marketplace bytes**.

**First dressed place-type: The Server Cathedral → the Crebotoly `ModularSciFiEnv_K` kit**
(chosen from the four installed kits — `_1`/`_F`/`_J`/`_K` — as the most cohesive, with
matching square 4×4m floor + ceiling, 4m wall mids, and industrial props). The palette is
wired to the kit's real meshes (`KitTileSize=400` for the 4m grid):

| Slot | Mesh |
|---|---|
| Floor | `/Game/ModularSciFiEnv_K/Meshes/Floors/SM_Floor_A_4x4m` |
| Wall | `/Game/ModularSciFiEnv_K/Meshes/Walls/SM_Wall_A_Mid_4x4m` |
| Ceiling | `/Game/ModularSciFiEnv_K/Meshes/Ceilings/SM_Ceiling_A_4x4m` |
| Props | `…/Lamps/SM_Lamp_AA_Base`, `…/Pipes/SM_Pipes_A_4m`, `…/Railings/SM_Railings_A_4m_A` |

These paths live in both `DT_ElsewherePlaces` (the runtime source of truth) and the
`ElsewhereGen` code default (fresh-clone fallback). **Asset policy: the kit `.uasset` bytes
are NOT committed** (gitignored `Content/ModularSciFiEnv_*/`) — referenced by path only,
same discipline as the Dragon Temple / cathedral packs. Vertical pivot/orientation per kit
mesh is an eyeball-and-nudge step (the builder places on a centered grid).

## Dressing status (atmosphere pass 1 — banked)

Server Cathedral mood is in: `CathedralGrade` PostProcessVolume (navy + gold grade,
bloom + vignette), builder-spawned warm-gold god-ray shafts (deterministic, down both
long walls), volumetric height fog, and a **dark void** exterior (no directional sun;
walls sealed floor-to-ceiling so no sky/void band shows). Tuning knobs: builder
`Shaft*` props + the top of `Tools/Scripts/build_elsewhere_map.py`.

**Still placeholder:** the scattered PROPS are engine cylinders (the C++ seeded scatter)
— the room is mid-dressing. Real prop dressing is the next pass, and is exactly what the
PCG spike below replaces.

## Dressing pass 2 — structural machinery + way-home beacon (banked)
The hall now reads as a real built space (PIE-verified live via ue_bridge screenshots):
- **Structural props** (`AElsewhereBuilder::SpawnStructuralProps`, toggle `bStructuralProps`):
  bulkhead **arch-ribs** (`SM_Bulkhead_A_Gate_A`) in rows at the 1/4 + 3/4 marks down the hall
  (span the width, player walks through, frame the curio) + **pipe runs** (`SM_Pipes_A_4m`)
  along both long-wall bases. Deterministic from the room grid (no RNG → doesn't perturb the
  scatter seed/gate). Kit-by-path, graceful skip when the kit's absent.
- **Richer PCG scatter**: the spawner weights **6** varied `_K` lamp bases (was 2) — the
  scattered detail layer. Big machinery is the C++ structural pass; small detail is PCG.
- **Return-door affordance**: a warm amber **beacon** (`ReturnBeacon`, tunable) at the doorway
  + prompt "`<- Back to the kitchen [E]`" (ASCII arrow — non-ASCII trips C4566 under -WX).

Known/by-design: rendering kit meshes through ISMs makes the editor prompt to save the kit's
shared `M_Base_MAT` (auto-sets `bUsedWithInstancedStaticMeshes`). It's **editor-only cosmetic**
— the cooker sets the flag at cook, and "Don't Save" is the fine workaround. We do NOT modify
the gitignored kit. (An in-memory auto-flag helper was tried and reverted — it forced the
default checker material; not worth the complexity.)

## The PCG seam → real PCG (spike in progress)

`AssembleGeometry()` was always the **single method** a `UPCGComponent->Generate()` replaces.
The PCG spike (branch `feat/sib-47-pcg-spike`) is making the "UE5 PCG" claim literally true,
incrementally, without ever breaking the playable loop.

### Done (plumbing + graph — compiles, gate green, graph authored headless)
- **PCG plugin enabled** (`SibeliusGame.uproject`) + `"PCG"` module dep in
  `SibeliusGame.Build.cs`.
- **Real `UPCGComponent`** on `AElsewhereBuilder` (`PCGScatter`, `GenerateOnDemand`).
- **Seam wired** in `AssembleGeometry`'s prop step: `if (bUsePCGScatter && RunPCGScatter())
  -> PCG owns the props; else the C++ seeded scatter (fallback)`. `RunPCGScatter` assigns the
  graph + `Seed = LayoutSeed` (deterministic) and **schedules `Generate()` for next tick**.
- **The graph `/Game/PCG/PCG_ElsewhereScatter` is AUTHORED** (headless, via
  `Tools/Scripts/build_pcg_scatter_graph.py`): **`Create Points Grid → Transform Points →
  Static Mesh Spawner → Output`**, all edges wired (3 nodes, 3 edges, 2 meshes — verified on
  reload). Grid: `CoordinateSpace=OriginalComponent`, `GridExtents=600`, `CellSize=600`
  (→ a 2×2 ≈ 4-prop layout centered on the builder at floor Z); Transform randomizes yaw
  0–360 + ±200cm XY jitter (seeded by the component Seed); Spawner = weighted selector with
  two `_K` meshes (`SM_Lamp_AA_Base`, `SM_Lamp_AB_Base`).
- **THE EMPTY-FLOOR FIX (root cause + 2 fixes).** PIE showed an empty floor; the log gave the
  real cause — **`LogPCG: Error: [RegisterOrUpdatePCGComponent] Component has invalid bounds,
  not registered nor updated` → `0 props`**. At BeginPlay (mid-`AssembleGeometry`) the
  runtime-built floor/wall ISMs haven't had their world bounds recomputed yet, so the actor's
  aggregate bounds are **invalid**. `UPCGComponent::CreateGenerateTask` does
  `if (!GetGridBounds().IsValid) { OnProcessGraphAborted(); return InvalidPCGTaskId; }` —
  **generation aborts before the graph runs**, so *no* sampler (the earlier World Ray Hit
  included) ever executes. Two fixes, both applied:
  1. **Defer generation (C++, mandatory).** `RunPCGScatter` now sets the graph + seed, then
     `World->GetTimerManager().SetTimerForNextTick(... GeneratePCGScatterDeferred)`. One tick
     later the ISM bounds are valid → the component registers and the graph runs.
  2. **Sample explicit bounds, not the world (graph).** Swapped World Ray Hit + Surface
     Sampler for **Create Points Grid** (`OriginalComponent` space → grid sits on the builder
     actor's transform at floor-stand Z, using its own extents). Removes the dependency on
     world collision / the physics scene entirely — timing-immune and deterministic.
- **`L_Elsewhere` builder has `bUsePCGScatter = true`** (graph wired via the C++ default).
  The **gate's** builder uses the CDO default `false` → C++ path → `ElsewhereSmokeTest`
  stays green (verified: 11 props + Server Cathedral 3 props, deterministic). The deferred
  PCG path can't affect the gate (CDO off, and the commandlet world doesn't tick).
  Floor/walls/ceiling, curio, return door, shafts stay C++.

### Richer per-seed scatter pass (banked — headless-verified; LOOK is Walt's PIE gate)
The C++ scatter is no longer a uniform cylinder spray — it's a deterministic, data-driven,
**per-seed-varying** detail layer with exclusion zones and an open path. New shape:
- **Data model** (`FScatterMeshDef` in `ElsewhereTypes.h`): per-mesh `Weight` / `bUpright` /
  `ScaleMin`–`Max` / `LeanJitterDeg`. The curated palette is the builder's **`ScatterSet`**
  default (ctor) — used for any place that doesn't author its own `FPlaceTypeDef::ScatterMeshes`
  (i.e. every place today), so it drives the look whether the **DataTable** or the code default
  is loaded. Per-place `ScatterMeshes`/`ScatterDensity`/`CorridorHalfWidth` exist as overrides.
- **Curated `_K` set (bounds-verified FLOOR-STANDING objects, 10 meshes, in lockstep with
  `build_pcg_scatter_graph.py`):** bulkhead end-units (`End_Mid_1m`/`_2m`/`_Top`/`_Low`), pipe
  junction boxes (`Pipes_B_1m_End`/`_Handler_A`), and posts/console greebles
  (`Railings_A_Pillar_A`/`_Long`, `Wall_A_Mid_1x1m_B`/`_Handle`). **The old lamp `_Base` meshes
  are flat ~5cm strip-light plates** (`dump_kit_bounds.py` proved it) — excluded from floor
  scatter. See [[kit-mesh-axis-convention]].
- **Determinism + kit-absent parity:** one `FRandomStream(LayoutSeed)`, a FIXED per-prop draw
  budget (`3K+5`) independent of exclusion hits and mesh-load state, so two fresh builds match
  AND kit-absent (gate) consumes the stream identically to kit-present. `PlaceScatter` returns
  the **instanced** count.
- **Exclusion + open path:** carve discs around the curio, the west doorway/return, and the
  player-spawn bay; keep a central **corridor** (Y-band on the doorway→curio approach) clear so
  the player always has a walk to the curio + the way home. Props may fill *behind* the curio.
- **Per-seed "what's behind the door this time?":** the seed picks a random **subset** of the
  palette (composition), a **density** wobble (count), and **cluster** centres (bunching) — so
  different seeds give visibly different rooms; same seed → identical (the gate proves it).
- **Tunables** (builder Details panel): `ScatterSet`, `Curio/Doorway/SpawnExclusionRadius`,
  `ScatterClusterBias`, `ScatterSubsetMin/Max`, `ScatterPlacementTries`; per-place
  `ScatterDensity`/`CorridorHalfWidth`; `FloorMaterialOverride` (real `_K` floor MI by path).
- **Floor material quick win:** `FloorMaterialOverride` (default `MI_Floor_A_Base`, BY PATH —
  no kit edit) reskins the floor ISM. Confirmed via `inspect_kit_materials.py` that the residual
  editor "checker" is the kit's shared `M_Base_MAT` lacking `bUsedWithInstancedStaticMeshes`
  (False on ALL kit MIs) — the known harmless editor-prompt / cook-time concern; we deliberately
  do NOT toggle that flag (the auto-flag helper was tried + reverted).

**Headless-verified** (`ElsewhereSmokeTest`, kit absent → engine-shape fallback): determinism
at the **transform** level (not just count), no prop inside any carve disc, the corridor is
clear, and **per-seed variation (3/3 seed-pairs differ** — e.g. seeds 101–606 → 12/13/14/6/8/10
props). The PCG graph rebuilt clean (`nodes=3 edges=3 meshes=10`) with the curated set.
**NOT verified (Walt's PIE gate, honestly):** that it LOOKS good / reads "different per seed" —
the kit meshes only render with the kit installed + PIE; bridge/run-pie is Simulate-only.

### ▶ RESUME HERE — Walt's PIE-verify (the render gate; can't be done headless)
Graph rebuilt + C++ rebuilt (editor-closed), gate green. The kit meshes + PCG generation
**can't be rendered/verified headless**, so this is a PIE eyeball:
1. PIE `L_Elsewhere` directly (preview build, C++ scatter path — `bUsePCGScatter` defaults
   true on that builder; set it **false** to eyeball the richer C++ scatter first). **Expect
   the curated `_K` machinery/posts** scattered in the side bays with a **clear central walk**
   to the curio; the floor reads as the real `_K` floor (accept the harmless save-kit-material
   prompt / "Don't Save"). Re-enter with a different seed → a **visibly different** mix/density.
   (Old PCG resume notes below are superseded by the curated set; the lamp-base expectation
   no longer applies.)
1b. PIE `L_Elsewhere` (the builder has `bUsePCGScatter=true`). **Expect ~4 props
   (`SM_Bulkhead_*`/`SM_Pipes_B_*`) appear on the floor one tick after load** (deferred Generate),
   random yaw, near the room centre.
2. **Determinism:** re-enter / re-PIE → **identical layout** (Create Points Grid is fixed;
   the component Seed = the run's LayoutSeed drives the Transform jitter). Same-seed → same.
3. **No more invalid-bounds abort:** the log should NOT show
   `[RegisterOrUpdatePCGComponent] Component has invalid bounds`. If props are still missing,
   confirm the deferred `Generate` fired (`LogElsewhereBuilder: ... deferred PCG Generate
   fired`, Verbose) and check `LogPCG` for a new reason.
4. **Tune knobs** (rebuild with `py Tools/Scripts/build_pcg_scatter_graph.py`, editor closed):
   count = grid `cell_size` (smaller = more) / `grid_extents`; scatter spread = Transform
   `offset_*`; nicer meshes = the Spawner's `MESHES` list.
5. **Instant revert if needed:** set `bUsePCGScatter=false` on the builder → C++ props return.
6. Optionally make `ElsewhereSmokeTest` assert the PCG path deterministically (it currently
   keeps the C++ path as the determinism handle, per the spike note).
7. Then migrate further (walls/ceiling via PCG) only after the scatter slice is proven.
   Later refinement: drive grid extents from the place-type `RoomExtent` (override the PCG
   param from C++) so the scatter fills each room instead of a fixed 6 m half-extent.

Everything else in the loop — plan → curio → return → discard, the save rule, the Cabinet —
is untouched; the seam keeps the swap isolated and additive.

## Regression fix — vertical sealed walls + dark-void doorway (PIE-verified via ue_bridge)
Three branch regressions vs main, all traced to ONE real bug + its knock-ons (commit after
the PCG work):
- **Walls mis-oriented (root cause).** The kit wall mesh `SM_Wall_A_Mid_4x4m` has its
  face-normal along local +X / width along local +Y (bounds `X∈[-20,5]`, `Y=[-200,200]`,
  `Z=[0,400]`), but `AssembleGeometry` placed walls with the fallback-cube convention (width
  along X) → every kit wall came out **rotated 90°**: thin fins poking into the room with
  ~375cm gaps. Fixed: panel convention (face +X / span Y) → **E/W edges yaw 0, N/S edges yaw
  90**, and the fallback cube's `WallFit` reshaped to match. See [[kit-mesh-axis-convention]].
- **"Daytime blue sky" = the wall gaps** leaking the engine default backdrop (the level has
  no sun/SkyAtmosphere by design). Sealing the walls stops the flooding; the open west
  doorway is now backed by a tall unlit **void backdrop slab** (added to the fallback-cube
  ISM with NO RNG draw, far west of the return door) so the portal reads dark.
- **"Collect/return does nothing" = the broken room**, not code. Curio/ReturnDoor/Subsystem
  are byte-identical to main; driving `ReturnDoor.Interact` live discarded the plan and
  traveled to `L_Office_v02` cleanly. The mis-oriented wall fins were blocking the interact
  trace / making the door unreachable; fixing the walls restores a navigable hall.

Verified live (Simulate + bridge): ISM instance transforms (sealed perimeter), screenshots
(interior dark, doorway dark), return travel, 4 PCG lamp props. **Still needs Walt's
hands-on PIE:** the interactive playthrough with real input — V-reveal at the kitchen Sauce
Door → step through → aim+E to collect the orb → E the return door (the bridge can only
Simulate, no player pawn, so keyboard interaction wasn't exercised). All constituent pieces
are verified.

## Running the gate (editor CLOSED)

```
UnrealEditor-Cmd.exe SibeliusGame.uproject -run=ElsewhereSmokeTest -unattended -nopause -nosplash -stdout
```

Exit 0 = pass. It asserts: ≥3 place-types with resolvable curio pools and all 3 rarities;
`RollPlan` deterministic + varied over 200 seeds + every curio fits its place; collection
scoring (always score, dupes don't refill a slot, rarer = more); the save round-trips
curios+score and carries **no room** (discard rule); the builder spawns exactly one curio
(matching the plan) + a return door and is reproducible from the seed; **the dressed Server
Cathedral builds deterministically with kit-absent fallback**; the Cabinet fill tracks the
owned set; the Sauce Door arm-gate (unarmed wall vs. armed travel door).

## Editor follow-up (the assembly Walt does in-editor)

1. **`L_Elsewhere` map — already built** by `Tools/Scripts/build_elsewhere_map.py` (committed
   `.umap`): a sun + skylight, a `PlayerStart` inside the west doorway, one
   `AElsewhereBuilder` at origin with `bPreviewWhenUnstaged=true`, and a **GameMode override
   = `AElsewhereGameMode`**. PIE it directly to walk the Server Cathedral; arriving via the
   Sauce Door uses the real staged plan instead. Re-run the script
   (`py "Tools/Scripts/build_elsewhere_map.py"`) to regenerate (it's idempotent).

   **Clean GameMode (`AElsewhereGameMode` + `AElsewhereHUD`):** the wonder room must be free
   of the main build system. The GameMode reuses the FirstPerson pawn + controller (so
   movement/look/E keep working — the input contexts live on the BP controller) but swaps in
   a crosshair-only HUD (no INVENTORY/PROGRESS/GENERATE/CONTROLS overlay). Crucially,
   `UBranchPIEComponent` checks for this GameMode and **skips `ApplyDeployedSave()` in the
   Elsewhere** — otherwise the character's deploy-restore re-spawns the main game's saved
   *generated* build sites into the throwaway room (a stray checker cube on the curio). The
   interaction prompt is drawn by `UInteractorComponent` (on-screen, not the HUD), so E-prompts
   still show.
2. **Kit + DataTables — DONE.** The four Crebotoly kits are installed (`ModularSciFiEnv_*`,
   gitignored). `DT_ElsewherePlaces` + `DT_ElsewhereCurios` are built from the CSVs by
   `Tools/Scripts/build_elsewhere_datatables.py` (committed `.uasset`s, in `Content/Data/`),
   with the Server Cathedral row's mesh palette already pointed at the `ModularSciFiEnv_K`
   meshes (verified via `get_data_table_column_as_string`). Re-run the script to regenerate
   from the CSVs if they change (it's idempotent — drops + recreates).
3. **Kitchen wiring (in `L_Office_v02`) — DONE** (`Tools/Scripts/build_kitchen_loop.py`):
   `SauceDoor_Kitchen` at the kitchen centre (-1728, 9240, 169) — **hold V to reveal it**, then
   E steps through to `L_Elsewhere`; `CabinetOfCuriosities` at (-2028, 9540, 59). Both are
   placeholder spots to nudge by eye (e.g. slide the door flush to a wall). No cauldron here
   (the office has no Sauce-feed; the cauldron is a World-Three / `L_AI_Temple` actor) — the
   door is reveal-gated like every other hidden door.
4. **Return target:** `ReturnDoor.HomeLevelName` defaults to `L_Office_v02`; set it to whatever
   the house map is.
5. **Polish (PIE-tunable):** a curio-glow material, a filled/dim Cabinet-slot material reading the per-instance custom
   data float, fog actors per mood. None of this changes the loop — it dresses it.
6. **PCG upgrade (later):** enable the PCG plugin, author a graph per place-type, replace the
   body of `AssembleGeometry()` with a `UPCGComponent` generate. The seam is already there.

## Why this shape

Mirrors the project's proven spine: a **pure headless core** (`FElsewhereGen`, like
`FCarouselSim`) wrapped by a **thin `UGameInstanceSubsystem`** (like `UCarouselRunSubsystem`),
**content-as-data** with a code-default fallback, and a **per-feature smoke gate**. That's
what lets the wonder loop be judged (Walt + Raymond) before any kit money is spent (§9).
