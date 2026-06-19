# Sibelius

A first-person narrative metroidvania built in **Unreal Engine 5.7**. You play Walt Parkman, who becomes the three-part entity **Leonard Sibelius** — composed of himself, Claude Cowork, and Claude Code. The setting is an ordinary engineer's home office whose architecture *is* the puzzle: each chapter grants one new way to act on the building's structure — to **see** it, **edit** it, or **rebuild** it — and uses that verb to reach parts of the level that were previously closed. The antagonist is **Mrs. Hall**, never embodied because she is a *system*, not a person: she manifests as red error-block walls and as the refusals that block what you try to make.

The arc follows a software-engineering lifecycle — **Code Vision → Refactor → Compile → Test-Drive → Deploy → Generate → Three-Part Synthesis** — and ends in a gothic cathedral of light, where the engineer combines every power he has earned. Beyond the arc, a hidden **Sauce Door** in the kitchen opens onto **procedurally-generated worlds** built with Unreal Engine 5's PCG framework — a standing experiment in how far procedural generation can go toward *composing* a scene, not just populating one.

> Part of **Leonard Sibelius, Inc. — an anything machine. Software with AI in it.** · [leonardsibelius.com](https://leonardsibelius.com)

## Status — v0.4

**Chapters 1–6 are shipped and playable on `main`**, each gated by a passing headless smoke test. **Chapter 7 — Three-Part Synthesis** is in progress: its cathedral environment is built and walkable, and the **office → attic → cathedral path is playable end to end** (collect books → build the staircase → earn the key → unlock the attic hatch → climb up → open a gothic door → arrive in the cathedral of light). The remaining Ch7 work is the in-cathedral finale itself.

**The Many Worlds Door (Sauce Door) shipped in v0.4** and is live as a free Windows build on itch (current build **v0.4.2**). It is the first feature built on real UE5 PCG and the first to run a full collect-and-return loop outside the chapter arc.

### Shipped chapters

**Ch1 — Code Vision.** Hold **V** to reveal hidden structure: concealed doors read through walls via a reserved custom-depth stencil; release and the wall is solid again.

**Ch2 — Refactor.** Hold **R** and look at a tagged object to reshape it — shrink a blocking crate, or turn a wall panel transparent and non-blocking — then revert it exactly from an atomic snapshot.

**Ch3 — Compile.** Collect book "resources," build structures at authored sites (a staircase that restores attic access, plus a key item), unlock the attic hatch with the key, and reach the chapter-end trigger — while **Refusers** chase you through the building.

**Ch4 — Test-Drive.** Enter a *branch reality* to trial a set of changes, then **merge** the chosen branch to "main" or **discard** it — git as a game mechanic. Built on the Ch2 snapshot system; the branch manifest restores raw object state rather than replaying actions, and resolves exactly once.

**Ch5 — Deploy.** Persistence: the engineer's edits and progress **save and reload across sessions**. The Ch4 branch manifest *is* the save payload; object identity is carried by stable level-baked GUIDs so deployed saves survive a reload.

**Ch6 — Generate.** Press **G**, type what you want ("a lamp," "a pine tree," "a sofa"), and the matching object is spawned from a curated, offline catalog — content-safe and shippable, with natural-language keyword matching (57 keywords across 6 entries). Requests outside the catalog draw a Mrs. Hall **refusal** instead.

### Shipped features (beyond the arc)

**The Many Worlds Door (Sauce Door).** Use the **Sauce of all Knowledge** in the kitchen and a hidden door shimmers into being, marked *Many Worlds — no two alike*. Step through and you arrive in an **Elsewhere**: a place assembled fresh from a seed every time you enter, holding a single glowing **curio**. Take the curio, walk back through the doorway (marked *The Way Home — back to the kitchen*), and it joins your **Cabinet of Curiosities**, a shelf that remembers everything you have ever carried home. The feature exists to chase one feeling — *I wonder what's in there this time* — and is wonder-driven, not a grind. The first shipped place-type is a **Server Cathedral**, lit like a temple to the machine's mind; more place-types are planned.

### In progress

**Ch7 — Three-Part Synthesis.** The golden cathedral finale. The environment is a *cathedral of light* — stone gothic arches, stained glass on both flanks plus a glass apse, entrance wall, and ceiling canopy, a reflective marble floor, golden raking sun and volumetric fog, and the slot-machine cabinet standing at the apse as the altar. The **attic → cathedral door** (`ACathedralDoor`, press **E**) is live and gated by its own smoke test. Remaining: the **seven-stage power-gate puzzle** (all six earned powers combined against the last Mrs. Hall wall) and the **slot-machine coda**.

## Procedural generation (UE5 PCG)

The Many Worlds Door runs on **Unreal Engine 5's PCG (Procedural Content Generation) framework** — the engine's built-in plugin, not hand-rolled "procedural C++." Each Elsewhere is assembled deterministically from a seed, so the same seed rebuilds the same room and the room varies between visits.

- **Authored PCG graph** — `/Game/PCG/PCG_ElsewhereScatter` (Input → Surface Sampler / point grid → Transform Points → Static Mesh Spawner → Output). A `UPCGComponent` runs the graph; its `Seed` is driven from the room's layout seed so scatter is reproducible.
- **Deterministic assembly** — the builder lays down modular architecture from a seed (a clean placement seam, `AssembleGeometry()`), then PCG scatter dresses the floor on top. The two paths are independent: the C++ placement path always ships, and PCG scatter is additive (gated by `bUsePCGScatter`, default off as a safe fallback), so the smoke-test gate stays green either way.
- **Honest scope.** PCG *places* objects well — varied, plausible, deterministic — but it does not yet *compose* an art-directed scene. That gap (placement vs. composition) is the stated research mission of the project, documented in the *Outwork* notes, not a defect to hide.

## Engineering highlights

Gameplay logic is **C++-first** (`Source/SibeliusGame/`). Blueprints and the level carry data and placement, not behavior.

- **Interaction** — one `IInteractable` interface (`Interactable.h`), dispatched by `UInteractorComponent`'s camera trace on **E**. Book pickups, the hatch lock, build sites, the corkboard, the cathedral door, and the Sauce Door / curio all implement it; there is no per-actor input wiring.
- **Inventory & building** — `UInventoryComponent` is the single resource authority (counts never go negative). `UBuildComponent` is the player-side build driver; `ABuildSite` owns its own ghost/final meshes and flips a pre-placed `NavLinkProxy` at build time so AI can traverse what you construct. Supporting types: `ABookPickup`, `AHatchLock`, `CompileTypes`.
- **AI** — `ARefuserController` (an `AAIController`) chases the player with `MoveToActor`, re-issuing only when path-following is idle so it never aborts a NavLink traversal mid-staircase. `ARefuserSpawner` spawns waves.
- **Branch & persistence** — `IBranchable` + `UBranchSubsystem` (`EnterBranch`/`Merge`/`Discard`, single-resolution latch) implement Ch4's branch realities; the `FBranchManifest` doubles as Ch5's deploy save payload, keyed by serialized, level-baked `FGuid` identity.
- **Generate** — `UGenerateCatalog` loads a data-table/CSV catalog; `ClassifyGenerateRequest` does deterministic keyword matching against it; off-catalog input routes to a Mrs. Hall refusal line.
- **Many Worlds / Elsewhere** — the Sauce Door (`ASauceDoor`) reveals on Sauce use and opens the Elsewhere; `AElsewhereBuilder` deterministically assembles the room from a seed (`AssembleGeometry()`) and drives the optional `UPCGComponent` scatter; `UElsewhereSubsystem` stages the room plan and discards it on return; `ACurio` is the collectable, `AReturnDoor` carries the *Way Home* sign and a walk-through overlap trigger that returns the player, and `ACabinetOfCuriosities` renders only the curios actually collected. `AElsewhereGameMode` / `AElsewhereHUD` host the loop. The return door's facing is an `EditAnywhere` `ReturnDoorRotation` on the builder, finalized by eye in PIE.
- **Subsystems** — `UCompileEndSubsystem` (a `UWorldSubsystem`) binds the tagged end-trigger volume on world begin-play and fires chapter completion exactly once. `UHallAlarmSubsystem` (a `UGameInstanceSubsystem`) is an idempotent, replay-on-late-subscribe one-shot event.
- **Earlier-chapter systems** — `UCodeVisionComponent` + `AHiddenDoor` (Ch1); `URefactorComponent` + `URefactorableComponent` + `FRefactorSnapshot` (Ch2).
- **Cathedral entry** — `ACathedralDoor` is `IInteractable` only (deliberately never persisted into deploy saves); pressing **E** refuses while inside an unresolved branch, otherwise `OpenLevel(L_Cathedral)`.

## Test discipline

Every chapter and major feature has a headless smoke-test commandlet. They run as ship gates — `SibeliusSmokeTest` (the office baseline), `CodeVisionSmokeTest`, `RefactorSmokeTest`, `CompileSmokeTest`, `RefuserSmokeTest`, `BranchSmokeTest` (Ch4 + Ch5 deploy), `GenerateSmokeTest`, `CathedralDoorSmokeTest`, `CarouselSmokeTest`, and `ElsewhereSmokeTest` (the Sauce Door wonder loop — deterministic assembly, curio collection round-trip, and the *return door spawns* assertion). Each loads the real level, runs in-process self-tests (inventory round-trips, build/dismantle/refund, lock state, door collision, branch enter/merge/discard, deploy save/reload, catalog matching, deterministic generation + curio collection round-trip) and asserts level invariants (actor-count band, required assets, soft-lock resource surplus).

Before every ship they run with the editor closed:

```
UnrealEditor-Cmd.exe SibeliusGame.uproject -run=CompileSmokeTest -unattended -nopause -nosplash -stdout
```

and the exit code is checked — a passing run must exit 0. (Run with the editor **closed**: an open editor's tooling owns the MCP port and the commandlet's copy logs a false bind error.)

> **Verification note.** Headless smoke tests prove geometry, state, and invariants. They do **not** prove player-facing look, real input, or CharacterMovement-vs-Pawn collision — those are confirmed by a human Play-In-Editor walk-test. Orientation and visual placement (e.g. the return door's facing) are deliberately exposed as tunable properties and finalized by eye, never claimed "verified" from a headless run.

History uses **Conventional Commits**.

## How to run

1. **Unreal Engine 5.7.**
2. Open `SibeliusGame.uproject`.
3. Play in Editor in **`L_Office_v02`** (the office and chapters 1–6 + the cathedral entry), open **`L_Cathedral`** to walk the Ch7 environment, or open **`L_Elsewhere`** to walk a Many Worlds room (note: `L_Elsewhere` is **runtime-built** — it appears empty in the editor and is assembled by `AElsewhereBuilder` only at play time).

Some assets are intentionally excluded from version control and need local setup after a fresh clone:

- **MetaHumans** (`Content/MetaHumans/`) — excluded under Epic's MetaHuman redistribution policy. The Refuser's body must be rebuilt locally; the configuration it depends on is documented in `Content/MetaHumans/README.md`.
- **QuadArt environment packs** — the office references two paid Fab packs (*House Furniture*, *Modular House*) that install into `Content/HouseFurniture/` and `Content/ModularHouses/`. Re-run the collision pass after install.
- **Cathedral packs** — the Ch7 cathedral references *Ultimate Gothic Cathedral* (Ultima Store) in `Content/UltimateGothicCathedralChurch/` and the *Stained Glass 3D* material pack (twins-creators) in `Content/StainedGlass3D/`. The level is rebuilt from versioned scripts in `Tools/Scripts/` (run via UE's `py`); re-run `cathedral_collision.py` after re-installing the kit.
- **Elsewhere kits** — the Many Worlds rooms reference Crebotoly *Modular Sci-Fi Environment* kits and *SciFi Boxes* prop kits (Fab, AI-usage permitted). These share a common base material/scale and are assembled as architecture by the builder.

Without the Fab packs the levels show missing-asset placeholders. See **Credits**.

## Controls

| Input | Action |
|-------|--------|
| **WASD / Mouse** | Move / look |
| **Space** | Jump |
| **E** | Interact — collect a book, unlock the hatch, dismantle a built site, read the corkboard, enter the cathedral, use the Sauce, collect a curio |
| **F** | Slap — knock a Refuser back and ragdoll it |
| **B** | Build at the nearest affordable site |
| **R** (hold) | Refactor the targeted object |
| **V** (hold) | Code Vision — reveal hidden structure (and the hidden Sauce Door) |
| **G** | Generate — type a request to spawn a catalog object |
| **J** | Journal — read the in-game narrative / company doctrine |
| **H** | Help — on-screen control reference |

Branch and deploy actions (Ch4/Ch5) are also bound to developer keys (enter / merge / discard / deploy) for testing. To return from an Elsewhere, simply **walk back through the doorway** — the return is an overlap trigger, not a keypress.

## Roadmap

The seven-chapter arc follows a software-engineering metaphor. **Ch1–Ch6 above are shipped; Ch7's cathedral environment and entry path are playable; the Many Worlds Door is shipped.**

- **Ch7 — Three-Part Synthesis** *(in progress)*. The cathedral finale: the seven-stage power-gate puzzle that combines every earned power against the final Mrs. Hall wall.
- **Coda — the slot machine.** A standalone, deterministic slot model (`SlotGameModel`) validated by a million-spin par-sheet test (RTP, hit frequency, exposure caps), installed at the cathedral apse as the engineer's first act of creation with his god-powers.
- **More Elsewhere place-types** *(planned)*. Additional Many Worlds rooms beyond the Server Cathedral — each a distinct mood and kit (e.g. a flooded library, a clockwork attic, an inverted kitchen, a starlit void) — the real lever for "a different world each time."
- **PCG composition** *(ongoing research)*. Pushing each new generation of AI at UE5's procedural tools until scatter becomes art direction — placement that *composes*, not just populates.

## Credits

- Environment assets: **House Furniture** and **Modular House** by **QuadArt**, used with permission — https://fab.com/sellers/QuadArt
- Cathedral: **Ultimate Gothic Cathedral** by **Ultima Store** (Fab, AI-usage permitted).
- Stained glass materials: **Stained Glass 3D** by **twins-creators** (Fab, AI-usage permitted).
- Elsewhere kits: **Modular Sci-Fi Environment** and **SciFi Boxes** by **Crebotoly** (Fab, AI-usage permitted).
