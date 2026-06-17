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
| Generation core | `ElsewhereGen.{h,cpp}` | **pure + headless**: `RollPlan(seed)` → deterministic place + curio + layout/mood sub-seeds; the code-default registry of 4 place-types × 12 curios (works on a fresh clone). The `FCarouselSim` of this feature. |
| Generation brain | `ElsewhereSubsystem.{h,cpp}` | `UGameInstanceSubsystem`: loads the registry (DataTable-or-default), **stages** a plan that survives the level travel, **discards** it on return. |
| Collection / Cabinet truth | `CurioCollectionSubsystem.{h,cpp}` + `ElsewhereSaveGame.h` | owns the collection + score; saves **curios + score only** through `FSibeliusSaveIO`. The save type structurally can't hold a room (the discard rule, §4). |
| Kitchen trigger | `SauceDoor.{h,cpp}` | sibling of `AHiddenDoor`: reveal pattern, but **armed by the Sauce** (binds `ASauceCauldron::OnSauceComplete`). On E it **stages an Elsewhere + travels**, instead of opening a fixed level. |
| The collectable | `Curio.{h,cpp}` | the one glowing curio (`ABookPickup` pattern); E writes to the collection subsystem, then destroys. |
| Room assembler | `ElsewhereBuilder.{h,cpp}` | reads the staged plan, assembles a seeded modular room (floor ISM + scattered props + mood light), spawns the curio + the return door. |
| The way home | `ReturnDoor.{h,cpp}` | E discards the staged plan and travels back to the house. |
| The wonder's home | `CabinetOfCuriosities.{h,cpp}` | one slot per known curio, lit when owned; fills as you collect. |
| Ship gate | `SibeliusGameEditor/ElsewhereSmokeTestCommandlet.{h,cpp}` | proves the whole loop headless (see below). |

## The PCG seam (important — read before "where's the PCG?")

The design calls for UE5's **PCG framework** to assemble the geometry. A PCG graph is an
**editor-authored asset** the runtime can't create from C++ in a headless commandlet, and
the PCG plugin isn't enabled in `SibeliusGame.uproject`. So the MVP ships a **deterministic
C++ assembly** in `AElsewhereBuilder::AssembleGeometry()` — *same inputs* (place-type +
seed), *same throwaway-room contract*, and fully gate-testable now.

`AssembleGeometry()` is the **single method** a `UPCGComponent->Generate()` call replaces
when the kit work lands. Everything else in the loop — plan → curio → return → discard,
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
(matching the plan) + a return door and is reproducible from the seed; the Cabinet fill
tracks the owned set; the Sauce Door arm-gate (unarmed wall vs. armed travel door).

## Editor follow-up (the assembly Walt does in-editor)

1. **DataTables (optional, live-tuning):** import `Data/ElsewherePlaces.csv` → `DT_ElsewherePlaces`
   and `Data/ElsewhereCurios.csv` → `DT_ElsewhereCurios` into `Content/Data/`. The runtime
   uses them if present, else the identical code defaults. (Same setup as `DT_Carousel*`.)
2. **`L_Elsewhere` map:** a small empty level with **one `AElsewhereBuilder`** at origin and a
   `PlayerStart`. That's all — the builder fills it on entry from the staged plan. (`SauceDoor.ElsewhereLevelName` defaults to `L_Elsewhere`.)
3. **Kitchen wiring (in `L_Office_v02`):** place an `ASauceDoor` in the kitchen wall and set its
   `Cauldron` to the kitchen's `ASauceCauldron` (using the Sauce arms the door). Place an
   `ACabinetOfCuriosities` somewhere in the house.
4. **Return target:** `ReturnDoor.HomeLevelName` defaults to `L_Office_v02`; set it to whatever
   the house map is.
5. **Polish (PIE-tunable):** real modular kit meshes on the place-types (`TileMesh`/`PropMesh`),
   a curio-glow material, a filled/dim Cabinet-slot material reading the per-instance custom
   data float, fog actors per mood. None of this changes the loop — it dresses it.
6. **PCG upgrade (later):** enable the PCG plugin, author a graph per place-type, replace the
   body of `AssembleGeometry()` with a `UPCGComponent` generate. The seam is already there.

## Why this shape

Mirrors the project's proven spine: a **pure headless core** (`FElsewhereGen`, like
`FCarouselSim`) wrapped by a **thin `UGameInstanceSubsystem`** (like `UCarouselRunSubsystem`),
**content-as-data** with a code-default fallback, and a **per-feature smoke gate**. That's
what lets the wonder loop be judged (Walt + Raymond) before any kit money is spent (§9).
