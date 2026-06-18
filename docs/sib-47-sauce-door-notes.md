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

## The PCG seam → real PCG (spike in progress)

`AssembleGeometry()` was always the **single method** a `UPCGComponent->Generate()` replaces.
The PCG spike (branch `feat/sib-47-pcg-spike`) is making the "UE5 PCG" claim literally true,
incrementally, without ever breaking the playable loop.

### Done (plumbing + graph — compiles, gate green, graph authored headless)
- **PCG plugin enabled** (`SibeliusGame.uproject`) + `"PCG"` module dep in
  `SibeliusGame.Build.cs`.
- **Real `UPCGComponent`** on `AElsewhereBuilder` (`PCGScatter`, `GenerateOnDemand`).
- **Seam wired** in `AssembleGeometry`'s prop step: `if (bUsePCGScatter && RunPCGScatter())
  -> PCG owns the props; else the C++ seeded scatter (fallback)`. `RunPCGScatter` sets the
  graph, sets `Seed = LayoutSeed` (deterministic), and calls `Generate(true)`.
- **The graph `/Game/PCG/PCG_ElsewhereScatter` is AUTHORED** (headless, via
  `Tools/Scripts/build_pcg_scatter_graph.py`): **`World Ray Hit Query → Surface Sampler →
  Transform Points → Static Mesh Spawner → Output`**, all edges wired (4 nodes, 4 edges,
  2 meshes — verified on reload). Transform randomizes yaw 0–360 + ±40cm offset (seeded by
  the component Seed); Spawner = weighted selector with two `_K` meshes (`SM_Lamp_AA_Base`,
  `SM_Lamp_AB_Base`).
- **THE EMPTY-FLOOR FIX (this session).** Two bugs, both fixed in the rebuild script:
  1. **No surface.** The Surface Sampler's `Surface` pin needs real `UPCGSurfaceData`; it was
     fed the **Input node's `In`** (the component's *actor* data — NOT a surface), so the
     sampler logged "No surfaces found from which to generate", aborted, and scattered
     nothing. Fixed by inserting a **World Ray Hit Query** node → `Surface Sampler.Surface`.
     With default params it ray-casts straight **down from the top of the PCG component's own
     actor bounds to the bottom**, landing points on the Elsewhere floor mesh (the floor ISMs
     use a `BlockAll` profile → they block the node's default `ECC_WorldStatic` channel). No
     manual Z/extent tuning — both the ray and the sampler footprint derive from the builder
     actor's bounds (`Surface Sampler` `Unbounded=false`, no Bounding Shape wired → uses the
     component/room bounds).
  2. **Density too sparse to ever show.** `points_per_squared_meter` was `0.0008`; the
     ServerCathedral floor is ~20×20 m = 400 m², so the *expected* count was 0.32 points —
     usually **zero**, which also read as "empty floor." Raised to **`0.01/m²` → ~4 hero
     props** (matches the C++ fallback's 3–6). This is the COUNT KNOB to tune in the script.
- **`L_Elsewhere` builder has `bUsePCGScatter = true`** (graph wired via the C++ default).
  The **gate's** builder uses the CDO default `false` → C++ path → `ElsewhereSmokeTest`
  stays green (3 deterministic props). Floor/walls/ceiling, curio, return door, shafts stay C++.

### ▶ RESUME HERE — Walt's PIE-verify (the render gate; can't be done headless)
The surface fix + density are authored and the graph is rebuilt (editor-closed). PCG
**generation output can't be rendered/verified headless**, so this is a PIE eyeball:
1. PIE `L_Elsewhere` (the builder has `bUsePCGScatter=true`). **Expect ~4 lamp props scattered
   on the floor** (`SM_Lamp_AA/AB_Base`), random yaw, standing on the floor surface.
2. **Determinism:** re-enter / re-PIE → **identical layout** (component Seed = the run's
   LayoutSeed drives sampler jitter + transform; World Ray Hit is a pure function of the
   fixed geometry). Confirm same-seed → same-scatter.
3. **If still empty,** check the PIE log (filter `LogPCG`/`Surface`): the likely culprits are
   now (a) the floor ISM collision not present at Generate time, or (b) `Generate(true)`
   running before the floor instances are added. Both point at *ordering/collision*, not the
   graph. Quick probes: raise `points_per_squared_meter` in the script + rebuild; or confirm
   the floor's `BlockAll` blocks `WorldStatic`.
4. **Tune knobs** (rebuild with `py Tools/Scripts/build_pcg_scatter_graph.py`, editor closed):
   count = `points_per_squared_meter`; nicer meshes = the Spawner's weighted `MESHES` list.
5. **Instant revert if needed:** set `bUsePCGScatter=false` on the builder → C++ props return.
6. Optionally make `ElsewhereSmokeTest` assert the PCG path deterministically (it currently
   keeps the C++ path as the determinism handle, per the spike note).
7. Then migrate further (walls/ceiling via PCG) only after the scatter slice is proven.

Everything else in the loop — plan → curio → return → discard, the save rule, the Cabinet —
is untouched; the seam keeps the swap isolated and additive.

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
