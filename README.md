# Sibelius

A first-person narrative metroidvania built in **Unreal Engine 5.7**. You play Walt Parkman, who becomes the three-part entity **Leonard Sibelius** — composed of himself, Claude Cowork, and Claude Code. The setting is an ordinary engineer's home office whose architecture *is* the puzzle: each chapter grants one new way to act on the building's structure — to **see** it, **edit** it, or **rebuild** it — and uses that verb to reach parts of the level that were previously closed. The antagonist is **Mrs. Hall**, never embodied because she is a *system*, not a person: she manifests as red error-block walls and as the refusals that block what you try to make.

The arc follows a software-engineering lifecycle — **Code Vision → Refactor → Compile → Test-Drive → Deploy → Generate → Three-Part Synthesis** — and ends in a gothic cathedral of light, where the engineer combines every power he has earned.

> Part of **Leonard Sibelius, Inc. — an anything machine. Software with AI in it.** · [leonardsibelius.com](https://leonardsibelius.com)

## Status — v0.2

**Chapters 1–6 are shipped and playable on `main`**, each gated by a passing headless smoke test. **Chapter 7 — Three-Part Synthesis** is in progress: its cathedral environment is built and walkable, and the **office → attic → cathedral path is playable end to end** (collect books → build the staircase → earn the key → unlock the attic hatch → climb up → open a gothic door → arrive in the cathedral of light). The remaining Ch7 work is the in-cathedral finale itself.

### Shipped chapters

**Ch1 — Code Vision.** Hold **V** to reveal hidden structure: concealed doors read through walls via a reserved custom-depth stencil; release and the wall is solid again.

**Ch2 — Refactor.** Hold **R** and look at a tagged object to reshape it — shrink a blocking crate, or turn a wall panel transparent and non-blocking — then revert it exactly from an atomic snapshot.

**Ch3 — Compile.** Collect book "resources," build structures at authored sites (a staircase that restores attic access, plus a key item), unlock the attic hatch with the key, and reach the chapter-end trigger — while **Refusers** chase you through the building.

**Ch4 — Test-Drive.** Enter a *branch reality* to trial a set of changes, then **merge** the chosen branch to "main" or **discard** it — git as a game mechanic. Built on the Ch2 snapshot system; the branch manifest restores raw object state rather than replaying actions, and resolves exactly once.

**Ch5 — Deploy.** Persistence: the engineer's edits and progress **save and reload across sessions**. The Ch4 branch manifest *is* the save payload; object identity is carried by stable level-baked GUIDs so deployed saves survive a reload.

**Ch6 — Generate.** Press **G**, type what you want ("a lamp," "a pine tree," "a sofa"), and the matching object is spawned from a curated, offline catalog — content-safe and shippable, with natural-language keyword matching (57 keywords across 6 entries). Requests outside the catalog draw a Mrs. Hall **refusal** instead.

### In progress

**Ch7 — Three-Part Synthesis.** The golden cathedral finale. The environment is a *cathedral of light* — stone gothic arches, stained glass on both flanks plus a glass apse, entrance wall, and ceiling canopy, a reflective marble floor, golden raking sun and volumetric fog, and the slot-machine cabinet standing at the apse as the altar. The **attic → cathedral door** (`ACathedralDoor`, press **E**) is live and gated by its own smoke test. Remaining: the **seven-stage power-gate puzzle** (all six earned powers combined against the last Mrs. Hall wall) and the **slot-machine coda**.

## Engineering highlights

Gameplay logic is **C++-first** (`Source/SibeliusGame/`). Blueprints and the level carry data and placement, not behavior.

- **Interaction** — one `IInteractable` interface (`Interactable.h`), dispatched by `UInteractorComponent`'s camera trace on **E**. Book pickups, the hatch lock, build sites, the corkboard, and the cathedral door all implement it; there is no per-actor input wiring.
- **Inventory & building** — `UInventoryComponent` is the single resource authority (counts never go negative). `UBuildComponent` is the player-side build driver; `ABuildSite` owns its own ghost/final meshes and flips a pre-placed `NavLinkProxy` at build time so AI can traverse what you construct. Supporting types: `ABookPickup`, `AHatchLock`, `CompileTypes`.
- **AI** — `ARefuserController` (an `AAIController`) chases the player with `MoveToActor`, re-issuing only when path-following is idle so it never aborts a NavLink traversal mid-staircase. `ARefuserSpawner` spawns waves.
- **Branch & persistence** — `IBranchable` + `UBranchSubsystem` (`EnterBranch`/`Merge`/`Discard`, single-resolution latch) implement Ch4's branch realities; the `FBranchManifest` doubles as Ch5's deploy save payload, keyed by serialized, level-baked `FGuid` identity.
- **Generate** — `UGenerateCatalog` loads a data-table/CSV catalog; `ClassifyGenerateRequest` does deterministic keyword matching against it; off-catalog input routes to a Mrs. Hall refusal line.
- **Subsystems** — `UCompileEndSubsystem` (a `UWorldSubsystem`) binds the tagged end-trigger volume on world begin-play and fires chapter completion exactly once. `UHallAlarmSubsystem` (a `UGameInstanceSubsystem`) is an idempotent, replay-on-late-subscribe one-shot event.
- **Earlier-chapter systems** — `UCodeVisionComponent` + `AHiddenDoor` (Ch1); `URefactorComponent` + `URefactorableComponent` + `FRefactorSnapshot` (Ch2).
- **Cathedral entry** — `ACathedralDoor` is `IInteractable` only (deliberately never persisted into deploy saves); pressing **E** refuses while inside an unresolved branch, otherwise `OpenLevel(L_Cathedral)`.

## Test discipline

Every chapter has a headless smoke-test commandlet. They run as ship gates — `SibeliusSmokeTest` (the office baseline), `CodeVisionSmokeTest`, `RefactorSmokeTest`, `CompileSmokeTest`, `RefuserSmokeTest`, `BranchSmokeTest` (Ch4 + Ch5 deploy), `GenerateSmokeTest`, `CathedralDoorSmokeTest`, `CarouselSmokeTest`, and `ElsewhereSmokeTest` (the Sauce Door wonder loop). Each loads the real level, runs in-process self-tests (inventory round-trips, build/dismantle/refund, lock state, door collision, branch enter/merge/discard, deploy save/reload, catalog matching, deterministic generation + curio collection round-trip) and asserts level invariants (actor-count band, required assets, soft-lock resource surplus).

Before every ship they run with the editor closed:

```
UnrealEditor-Cmd.exe SibeliusGame.uproject -run=CompileSmokeTest -unattended -nopause -nosplash -stdout
```

and the exit code is checked — a passing run must exit 0. (Run with the editor **closed**: an open editor's tooling owns the MCP port and the commandlet's copy logs a false bind error.) History uses **Conventional Commits**.

## How to run

1. **Unreal Engine 5.7.**
2. Open `SibeliusGame.uproject`.
3. Play in Editor in **`L_Office_v02`** (the office and chapters 1–6 + the cathedral entry), or open **`L_Cathedral`** to walk the Ch7 environment.

Some assets are intentionally excluded from version control and need local setup after a fresh clone:

- **MetaHumans** (`Content/MetaHumans/`) — excluded under Epic's MetaHuman redistribution policy. The Refuser's body must be rebuilt locally; the configuration it depends on is documented in `Content/MetaHumans/README.md`.
- **QuadArt environment packs** — the office references two paid Fab packs (*House Furniture*, *Modular House*) that install into `Content/HouseFurniture/` and `Content/ModularHouses/`. Re-run the collision pass after install.
- **Cathedral packs** — the Ch7 cathedral references *Ultimate Gothic Cathedral* (Ultima Store) in `Content/UltimateGothicCathedralChurch/` and the *Stained Glass 3D* material pack (twins-creators) in `Content/StainedGlass3D/`. The level is rebuilt from versioned scripts in `Tools/Scripts/` (run via UE's `py`); re-run `cathedral_collision.py` after re-installing the kit.

Without the Fab packs the levels show missing-asset placeholders. See **Credits**.

## Controls

| Input | Action |
|-------|--------|
| **WASD / Mouse** | Move / look |
| **Space** | Jump |
| **E** | Interact — collect a book, unlock the hatch, dismantle a built site, read the corkboard, enter the cathedral |
| **F** | Slap — knock a Refuser back and ragdoll it |
| **B** | Build at the nearest affordable site |
| **R** (hold) | Refactor the targeted object |
| **V** (hold) | Code Vision — reveal hidden structure |
| **G** | Generate — type a request to spawn a catalog object |
| **J** | Journal — read the in-game narrative / company doctrine |
| **H** | Help — on-screen control reference |

Branch and deploy actions (Ch4/Ch5) are also bound to developer keys (enter / merge / discard / deploy) for testing.

## Roadmap

The seven-chapter arc follows a software-engineering metaphor. **Ch1–Ch6 above are shipped; Ch7's cathedral environment and entry path are playable.**

- **Ch7 — Three-Part Synthesis** *(in progress)*. The cathedral finale: the seven-stage power-gate puzzle that combines every earned power against the final Mrs. Hall wall.
- **Coda — the slot machine.** A standalone, deterministic slot model (`SlotGameModel`) validated by a million-spin par-sheet test (RTP, hit frequency, exposure caps), installed at the cathedral apse as the engineer's first act of creation with his god-powers.

## Credits

- Environment assets: **House Furniture** and **Modular House** by **QuadArt**, used with permission — https://fab.com/sellers/QuadArt
- Cathedral: **Ultimate Gothic Cathedral** by **Ultima Store** (Fab, AI-usage permitted).
- Stained glass materials: **Stained Glass 3D** by **twins-creators** (Fab, AI-usage permitted).
