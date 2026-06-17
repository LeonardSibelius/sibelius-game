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
| Kitchen trigger | `SauceDoor.{h,cpp}` | sibling of `AHiddenDoor`: reveal pattern, but **armed by the Sauce** (binds `ASauceCauldron::OnSauceComplete`). On E it **stages an Elsewhere + travels**, instead of opening a fixed level. |
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

**First dressed place-type: The Server Cathedral → a Crebotoly modular sci-fi kit.** Its
palette points at the kit's *expected* import paths (`/Game/Crebotoly/Meshes/SM_*`). After
`Fab → Add to Project`, **repoint the palette in `DT_ElsewherePlaces`** to the kit's real
meshes (the soft-refs are data, so no recompile). Until then it renders as a real gray-box
hall. **Asset policy: the Crebotoly `.uasset` bytes are NOT committed** — referenced by
path only, same discipline as the Dragon Temple / cathedral packs.

## The PCG seam

`AssembleGeometry()` is the **single method** a `UPCGComponent->Generate()` call can replace
for richer scatter once the loop's proven (the PCG plugin isn't enabled yet, and a PCG graph
is an editor-authored asset). Everything else in the loop — plan → curio → return → discard,
the save rule, the Cabinet — is unchanged when PCG drops in. That's the seam: the wonder
loop is proven in code today; swapping the geometry backend to PCG is an isolated, additive
step (enable the PCG plugin, author one graph per place-type, point the builder at it).

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
   `.umap`): a sun + skylight, a `PlayerStart` inside the west doorway, and one
   `AElsewhereBuilder` at origin with `bPreviewWhenUnstaged=true`. PIE it directly to walk the
   Server Cathedral; arriving via the Sauce Door uses the real staged plan instead. Re-run the
   script (`py "Tools/Scripts/build_elsewhere_map.py"`) to regenerate (it's idempotent).
2. **Add the Crebotoly kit:** in-editor **Fab → My Library → Add to Project**. Then import
   `Data/ElsewherePlaces.csv` → `DT_ElsewherePlaces` and `Data/ElsewhereCurios.csv` →
   `DT_ElsewhereCurios` into `Content/Data/` (runtime uses them if present, else the identical
   code defaults — same setup as `DT_Carousel*`), and **set the Server Cathedral row's
   `FloorMeshes`/`WallMeshes`/`CeilingMeshes`/`PropMeshes` to the kit's real meshes**
   (`KitTileSize` to the kit's module size). The gray-box hall becomes the real sci-fi room.
3. **Kitchen wiring (in `L_Office_v02`):** place an `ASauceDoor` in the kitchen wall and set its
   `Cauldron` to the kitchen's `ASauceCauldron` (using the Sauce arms the door). Place an
   `ACabinetOfCuriosities` somewhere in the house.
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
