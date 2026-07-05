# Sibelius

A first-person narrative metroidvania built in **Unreal Engine 5.7**. You play Walt Parkman, who becomes the three-part entity **Leonard Sibelius** — composed of himself, Claude Cowork, and Claude Code. The setting is an ordinary engineer's home office whose architecture *is* the puzzle: each chapter grants one new way to act on the building's structure — to **see** it, **edit** it, or **rebuild** it — and uses that verb to reach parts of the level that were previously closed. The antagonist is **Mrs. Hall**, never embodied because she is a *system*, not a person: she manifests as red error-block walls and as the refusals that block what you try to make.

The arc follows a software-engineering lifecycle — **Code Vision → Refactor → Compile → Test-Drive → Deploy → Generate → Three-Part Synthesis** — and ends in a gothic cathedral of light, where the engineer combines every power he has earned. Beyond the arc, a hidden **Sauce Door** in the kitchen opens onto **procedurally-generated worlds** built with Unreal Engine 5's PCG framework — a standing experiment in how far procedural generation can go toward *composing* a scene, not just populating one.

> Part of **Leonard Sibelius, Inc. — an anything machine. Software with AI in it.** · [leonardsibelius.com](https://leonardsibelius.com)

## Status — v0.5

**Chapters 1–6 are shipped and playable on `main`**, each gated by a passing headless smoke test. **Chapter 7 — Three-Part Synthesis** is in progress: its cathedral environment is built and walkable, and the **office → attic → cathedral path is playable end to end** (collect books → build the staircase → earn the key → unlock the attic hatch → climb up → open a gothic door → arrive in the cathedral of light). The remaining Ch7 work is the in-cathedral finale itself.

**The Many Worlds Door (Sauce Door) now opens onto a shuffled deck of eight authored forests** and is live as a free Windows build on itch (current build **v0.5.4**). Each world is a photoscanned poplar forest generated in-editor with UE5 PCG (the EasyBiomes kit plus a recipe/conductor system built for this project), hand-vetted, and baked to its own level; the door picks one at random on every entry — never the same twice in a row. Every arrival lands on the same composed view: the forest road, a lone hero poplar down the sightline, three Shinbi watchers at the roadside, and a sailboat run aground where no boat should be.

### Shipped chapters

**Ch1 — Code Vision.** Hold **V** to reveal hidden structure: concealed doors read through walls via a reserved custom-depth stencil; release and the wall is solid again.

**Ch2 — Refactor.** Hold **R** and look at a tagged object to reshape it — shrink a blocking crate, or turn a wall panel transparent and non-blocking — then revert it exactly from an atomic snapshot.

**Ch3 — Compile.** Collect book "resources," build structures at authored sites (a staircase that restores attic access, plus a key item), unlock the attic hatch with the key, and reach the chapter-end trigger — while **Refusers** chase you through the building.

**Ch4 — Test-Drive.** Enter a *branch reality* to trial a set of changes, then **merge** the chosen branch to "main" or **discard** it — git as a game mechanic. Built on the Ch2 snapshot system; the branch manifest restores raw object state rather than replaying actions, and resolves exactly once.

**Ch5 — Deploy.** Persistence: the engineer's edits and progress **save and reload across sessions**. The Ch4 branch manifest *is* the save payload; object identity is carried by stable level-baked GUIDs so deployed saves survive a reload.

**Ch6 — Generate.** Press **G**, type what you want ("a lamp," "a pine tree," "a sofa"), and the matching object is spawned from a curated, offline catalog — content-safe and shippable, with natural-language keyword matching (57 keywords across 6 entries). Requests outside the catalog draw a Mrs. Hall **refusal** instead.

### Shipped features (beyond the arc)

**The Many Worlds Door.** Use the Sauce of all Knowledge in the kitchen and a hidden door shimmers into being, revealed with **Code Vision (V)** and marked *Many Worlds — no two alike*. Step through (**E**) and you arrive in one of eight **Elsewhere** forests — poplar woods dealt from different seeds, each with its own arrangement of canopy, meadow, dead-wood, and dense growth, and its own re-rolled insect and wind ambience. Wander as far as you like; press **O — Back to Office** at any time, and step through again for a different world (the door never repeats itself back-to-back). The earlier curio / Cabinet of Curiosities loop is set aside for now; the feature chases one feeling — *I wonder what's in there this time* — wonder, not collection.

### In progress

**Ch7 — Three-Part Synthesis.** The golden cathedral finale. The environment is a *cathedral of light* — stone gothic arches, stained glass on both flanks plus a glass apse, entrance wall, and ceiling canopy, a reflective marble floor, golden raking sun and volumetric fog, and the slot-machine cabinet standing at the apse as the altar. The **attic → cathedral door** (`ACathedralDoor`, press **E**) is live and gated by its own smoke test. Remaining: the **seven-stage power-gate puzzle** (all six earned powers combined against the last Mrs. Hall wall) and the **slot-machine coda**.

## Procedural generation (UE5 PCG)

The Many Worlds forests are built on **Unreal Engine 5's PCG framework** via the **EasyBiomes** photoscanned poplar kit — real captured plant geometry as **Nanite full-geometry foliage**, not alpha-mapped cards, which is why it holds up close. On top of the kit sits a small authored system built for this project:

- **World Conductor** (`BP_WorldConductor`) — one seed re-rolls the whole world: it reseeds and row-rotates the four hand-drawn spline regions (each region draws a different kit "look" — meadowed, bushy, dense, dead — from a data table), and applies the recipe's sun + fog mood before the regions spawn.
- **World Recipe** (`PDA_WorldRecipe` / `DA_Recipe_01_PoplarForest`) — a data asset that owns the vegetation palette, densities, lighting mood, and the hero mesh. The recipe drives generation; the graphs stay untouched.
- **Anchors rig** (`BP_WorldAnchors`) — the authored, seed-independent layer: the arrival view, the hero poplar, three idle-animated Shinbi figures standing at the road's edge (where no seed can grow a tree through them), and the surprise sailboat. The seed decides the flora; the rig is composition.

**Why the worlds are baked.** Runtime PCG generation does not survive cooking with this kit: a packaged build refuses to schedule on-demand generation, and the kit's runtime-generation mode breaks its own spline interior sampling (established empirically across a de-risk build series; details in `docs/design/Recipe_01_ModeB_Poplar_Forest.md`). So each world is generated and vetted **in the editor**, saved as its own level (`L_Forest_01..08`), and the door shuffles the deck at runtime. A finite deck of composed worlds beat an infinite stream of unvetted ones anyway: one seed put a tree through a Shinbi's chest, and in a baked deck that world simply doesn't ship.

**Honest scope.** PCG *places* objects well — varied, plausible, deterministic — but it does not yet *compose* an art-directed scene. That gap (placement vs. composition) is the stated research mission of the project, documented in the *Outwork* notes, not a defect to hide. The recipe/conductor/anchors system is the current best answer: randomness only inside fences, composition on rails.

## Engineering highlights

Gameplay logic is **C++-first** (`Source/SibeliusGame/`). Blueprints and the level carry data and placement, not behavior.

- **Interaction** — one `IInteractable` interface (`Interactable.h`), dispatched by `UInteractorComponent`'s camera trace on **E**. Book pickups, the hatch lock, build sites, the corkboard, the cathedral door, and the Sauce Door / curio all implement it; there is no per-actor input wiring.
- **Inventory & building** — `UInventoryComponent` is the single resource authority (counts never go negative). `UBuildComponent` is the player-side build driver; `ABuildSite` owns its own ghost/final meshes and flips a pre-placed `NavLinkProxy` at build time so AI can traverse what you construct. Supporting types: `ABookPickup`, `AHatchLock`, `CompileTypes`.
- **AI** — `ARefuserController` (an `AAIController`) chases the player with `MoveToActor`, re-issuing only when path-following is idle so it never aborts a NavLink traversal mid-staircase. `ARefuserSpawner` spawns waves.
- **Branch & persistence** — `IBranchable` + `UBranchSubsystem` (`EnterBranch`/`Merge`/`Discard`, single-resolution latch) implement Ch4's branch realities; the `FBranchManifest` doubles as Ch5's deploy save payload, keyed by serialized, level-baked `FGuid` identity.
- **Generate** — `UGenerateCatalog` loads a data-table/CSV catalog; `ClassifyGenerateRequest` does deterministic keyword matching against it; off-catalog input routes to a Mrs. Hall refusal line.
- **Many Worlds / Elsewhere** — `ASauceDoor` (an `AHiddenDoor` subclass) reveals on Code Vision and holds the **deck**: an `EditAnywhere` `TravelTargetLevels` array of baked forest levels. On interact it picks one at random — never the same twice in a row (session-scoped memory) — writes it into the inherited `TravelTargetLevel`, and the parent's travel path does the rest (branch-gating, travel cover, `OpenLevel`), so the smoke-tested reveal behavior is untouched. An empty deck falls back to the plain single-target door. World-side, `BP_WorldConductor` + `PDA_WorldRecipe` + `BP_WorldAnchors` (see above) generate and compose each deck level in-editor. The earlier `AElsewhereBuilder` / `ACurio` / `ACabinetOfCuriosities` runtime-room flow is set aside but retained in the codebase.
- **Subsystems** — `UCompileEndSubsystem` (a `UWorldSubsystem`) binds the tagged end-trigger volume on world begin-play and fires chapter completion exactly once. `UHallAlarmSubsystem` (a `UGameInstanceSubsystem`) is an idempotent, replay-on-late-subscribe one-shot event.
- **Earlier-chapter systems** — `UCodeVisionComponent` + `AHiddenDoor` (Ch1); `URefactorComponent` + `URefactorableComponent` + `FRefactorSnapshot` (Ch2).
- **Cathedral entry** — `ACathedralDoor` is `IInteractable` only (deliberately never persisted into deploy saves); pressing **E** refuses while inside an unresolved branch, otherwise `OpenLevel(L_Cathedral)`.

## Test discipline

Every chapter and major feature has a headless smoke-test commandlet. They run as ship gates — `SibeliusSmokeTest` (the office baseline), `CodeVisionSmokeTest`, `RefactorSmokeTest`, `CompileSmokeTest`, `RefuserSmokeTest`, `BranchSmokeTest` (Ch4 + Ch5 deploy), `GenerateSmokeTest`, `CathedralDoorSmokeTest`, `CarouselSmokeTest`, and `ElsewhereSmokeTest` (the Many Worlds gate — the forest level loads, the back-to-office rule is live in every away level and a no-op in the office, and the Sauce Door's Code-Vision reveal drives collision both ways). Each loads the real level, runs in-process self-tests (inventory round-trips, build/dismantle/refund, lock state, door collision, branch enter/merge/discard, deploy save/reload, catalog matching) and asserts level invariants (actor-count band, required assets, soft-lock resource surplus).

Before every ship they run with the editor closed:

```
UnrealEditor-Cmd.exe SibeliusGame.uproject -run=CompileSmokeTest -unattended -nopause -nosplash -stdout
```

and the exit code is checked — a passing run must exit 0. (Run with the editor **closed**: an open editor's tooling owns the MCP port and the commandlet's copy logs a false bind error.)

> **Verification note.** Headless smoke tests prove geometry, state, and invariants. They do **not** prove player-facing look, real input, or CharacterMovement-vs-Pawn collision — those are confirmed by a human Play-In-Editor walk-test. Orientation and visual placement are deliberately exposed as tunable properties and finalized by eye, never claimed "verified" from a headless run. The Many Worlds PlayerStart lesson is canon here: PIE spawns at the editor camera when a level has no PlayerStart, so only a **packaged** build proves arrival placement.

History uses **Conventional Commits**.

## How to run

1. **Unreal Engine 5.7.**
2. Open `SibeliusGame.uproject`.
3. Play in Editor in **`L_Office_v02`** (the office, chapters 1–6, the cathedral entry, and the Many Worlds door), open **`L_Cathedral`** to walk the Ch7 environment, or open any **`L_Forest_01..08`** to walk a Many Worlds forest directly. **`L_Elsewhere_Dev`** is the forest workbench — select the placed `BP_WorldConductor`, set a World Seed, and press **ConductWorld** to grow a new world in-editor (~1 min).

Some assets are intentionally excluded from version control and need local setup after a fresh clone:

- **MetaHumans** (`Content/MetaHumans/`) — excluded under Epic's MetaHuman redistribution policy. The Refuser's body must be rebuilt locally; the configuration it depends on is documented in `Content/MetaHumans/README.md`.
- **QuadArt environment packs** — the office references two paid Fab packs (*House Furniture*, *Modular House*) that install into `Content/HouseFurniture/` and `Content/ModularHouses/`. Re-run the collision pass after install.
- **Cathedral packs** — the Ch7 cathedral references *Ultimate Gothic Cathedral* (Ultima Store) in `Content/UltimateGothicCathedralChurch/` and the *Stained Glass 3D* material pack (twins-creators) in `Content/StainedGlass3D/`. The level is rebuilt from versioned scripts in `Tools/Scripts/` (run via UE's `py`); re-run `cathedral_collision.py` after re-installing the kit.
- **Many Worlds forest kit** — the eight `L_Forest_*` levels and the workbench reference *Broadleaf Poplar Forest — PCG Biome* by EasyBiomes, which installs into `Content/EasyBiomes/`. Note: the kit is used **stock** — the deck requires no kit modifications.
- **Forest figures & surprise** — *Paragon: Shinbi* (Epic, free on Fab) in `Content/ParagonShinbi/`; the *VOL16 Boats* pack in `Content/Vehicles/`.
- **Elsewhere kits** — the set-aside runtime rooms reference Crebotoly *Modular Sci-Fi Environment* kits and *SciFi Boxes* prop kits (Fab, AI-usage permitted).

Without the Fab packs the levels show missing-asset placeholders. See **Credits**.

## Controls

| Input | Action |
|-------|--------|
| **WASD / Mouse** | Move / look |
| **Space** | Jump |
| **E** | Interact — collect a book, unlock the hatch, dismantle a built site, read the corkboard, enter the cathedral, use the Sauce, step through the Many Worlds door |
| **F** | Slap — knock a Refuser back and ragdoll it |
| **B** | Build at the nearest affordable site |
| **R** (hold) | Refactor the targeted object |
| **V** (hold) | Code Vision — reveal hidden structure (and the hidden Sauce Door) |
| **G** | Generate — type a request to spawn a catalog object |
| **J** | Journal — read the in-game narrative / company doctrine |
| **H** | Help — on-screen control reference |
| **O** | Back to Office — return from a Many Worlds level at any time |

Branch and deploy actions (Ch4/Ch5) are also bound to developer keys (enter / merge / discard / deploy) for testing.

## Roadmap

The seven-chapter arc follows a software-engineering metaphor. **Ch1–Ch6 above are shipped; Ch7's cathedral environment and entry path are playable; the Many Worlds Door and its deck of eight forests are shipped.**

- **Ch7 — Three-Part Synthesis** *(in progress)*. The cathedral finale: the seven-stage power-gate puzzle that combines every earned power against the final Mrs. Hall wall.
- **Coda — the slot machine.** A standalone, deterministic slot model (`SlotGameModel`) validated by a million-spin par-sheet test (RTP, hit frequency, exposure caps), installed at the cathedral apse as the engineer's first act of creation with his god-powers.
- **More worlds for the deck** *(planned)*. New cards are cheap now — one seed, one vetting walk, one Save As. Beyond the poplar forest: new terrain stages and kits (a flooded library, a clockwork attic, a starlit void) — the real lever for "a different world each time."
- **True runtime regeneration** *(parked)*. Blocked by PCG runtime generation not running in cooked builds with the current kit/engine; revisit on future engine versions. The deck architecture already isolates this — only the door's pick logic would change.
- **PCG composition** *(ongoing research)*. Pushing each new generation of AI at UE5's procedural tools until scatter becomes art direction — placement that *composes*, not just populates.

## Credits

- Environment assets: **House Furniture** and **Modular House** by **QuadArt**, used with permission — https://fab.com/sellers/QuadArt
- Cathedral: **Ultimate Gothic Cathedral** by **Ultima Store** (Fab, AI-usage permitted).
- Stained glass materials: **Stained Glass 3D** by **twins-creators** (Fab, AI-usage permitted).
- Elsewhere kits: **Modular Sci-Fi Environment** and **SciFi Boxes** by **Crebotoly** (Fab, AI-usage permitted).
- Many Worlds forest: **Broadleaf Poplar Forest — PCG Biome** by **EasyBiomes** (Fab) — photoscanned, **Nanite full-geometry foliage** (no alpha-card fakery), the product of 8+ years of photogrammetry R&D. Used with thanks and admiration. https://www.fab.com/listings/61f2b0fc-5656-46b7-86ef-3c2100cebcb4
- Forest watchers: **Paragon: Shinbi** by **Epic Games** (free).
- The grounded sailboat: **VOL16 Boats** pack (Fab).
