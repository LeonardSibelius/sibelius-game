# Sibelius

A first-person narrative metroidvania built in Unreal Engine 5.7. You play Walt
Parkman, who becomes the three-part entity **Leonard Sibelius** — composed of
himself, Claude Cowork, and Claude Code. The setting is an office whose
architecture is the puzzle: each chapter gives the engineer one new way to act on
the building's structure — to see it, edit it, or rebuild it — and uses that verb
to reach parts of the level that were previously closed.

## Status — v0.2

Chapters 1–3 are shipped, each gated by a passing headless smoke test.

- **Ch1 — Code Vision.** Hold **V** to reveal hidden structure: concealed doors
  read through walls via a reserved custom-depth stencil; release and the wall is
  solid again.
- **Ch2 — Refactor.** Hold **R** and look at a tagged object to reshape it —
  shrink a blocking crate, or turn a wall panel transparent and non-blocking —
  then revert it exactly from a snapshot.
- **Ch3 — Compile.** Collect book "resources," build structures at authored sites
  (a staircase that restores attic access, plus a key item), unlock the attic
  hatch with the key, and reach the chapter-end trigger — while Refusers chase you
  through the building.

## Engineering highlights

Gameplay logic is C++-first (`Source/SibeliusGame/`). Blueprints and the level
carry data and placement, not behavior.

- **Interaction** — one `IInteractable` interface (`Interactable.h`), dispatched
  by `UInteractorComponent`'s camera trace on **E**. Book pickups, the hatch lock,
  build sites, and the corkboard all implement it; there is no per-actor input
  wiring.
- **Inventory & building** — `UInventoryComponent` is the single resource
  authority (counts never go negative). `UBuildComponent` is the player-side build
  driver; `ABuildSite` owns its own ghost/final meshes and flips a pre-placed
  `NavLinkProxy` at build time so AI can traverse what you construct. Supporting
  types: `ABookPickup`, `AHatchLock`, `CompileTypes`.
- **AI** — `ARefuserController` (an `AAIController`) chases the player with
  `MoveToActor`, re-issuing only when path-following is idle so it never aborts a
  NavLink traversal mid-staircase. `ARefuserSpawner` spawns waves.
- **Subsystems** — `UCompileEndSubsystem` (a `UWorldSubsystem`) binds the tagged
  end-trigger volume on world begin-play and fires chapter completion exactly
  once. `UHallAlarmSubsystem` (a `UGameInstanceSubsystem`) is an idempotent,
  replay-on-late-subscribe one-shot event.
- **Earlier-chapter systems** — `UCodeVisionComponent` + `AHiddenDoor` (Ch1);
  `URefactorComponent` + `URefactorableComponent` + `FRefactorSnapshot` (Ch2).

### Test discipline

Every chapter has a headless smoke-test commandlet. Five run as ship gates —
`SibeliusSmokeTest` (the office baseline), `CodeVisionSmokeTest`,
`RefactorSmokeTest`, `CompileSmokeTest`, and `RefuserSmokeTest`. Each loads the
real level, runs in-process self-tests (inventory round-trips,
build/dismantle/refund, lock state, door collision) and asserts level invariants
(actor-count band, required assets, soft-lock resource surplus). Before every ship
they run with the **editor closed**:

```
UnrealEditor-Cmd.exe SibeliusGame.uproject -run=CompileSmokeTest -unattended -nopause -nosplash -stdout
```

and the **exit code is checked** — a passing run must exit 0. History uses
Conventional Commits.

## How to run

1. Unreal Engine **5.7**.
2. Open `SibeliusGame.uproject`.
3. Play in Editor in **`L_Office_v02`**.

Some assets are intentionally excluded from version control and need local setup
after a fresh clone:

- **MetaHumans** (`Content/MetaHumans/`) — excluded under Epic's MetaHuman
  redistribution policy. The Refuser's body must be rebuilt locally; the
  configuration it depends on is documented in
  [`Content/MetaHumans/README.md`](Content/MetaHumans/README.md).
- **QuadArt environment packs** — the office references two paid Fab packs
  (*House Furniture*, *Modular House*) that install into `Content/HouseFurniture/`
  and `Content/ModularHouses/`. Without them the level shows missing-asset
  placeholders. See Credits.

## Controls

| Input | Action |
|---|---|
| **WASD** / **Mouse** | Move / look |
| **Space** | Jump |
| **E** | Interact — collect a book, unlock the hatch, dismantle a built site, read the corkboard |
| **F** | Slap — knock a Refuser back and ragdoll it |
| **B** | Build at the nearest affordable site |
| **R** (hold) | Refactor the targeted object |
| **V** (hold) | Code Vision — reveal hidden structure |

## Roadmap

The seven-chapter arc follows a software-engineering metaphor; Ch1–Ch3 above are
shipped.

- **Ch4 — Test-Drive.** Reuses the Ch2 snapshot/revert system to trial-run a
  change before committing to it.
- **Ch5 — Deploy.** Persistence — the engineer's edits and progress saved across
  chapter boundaries.
- **Ch6–Ch7.** Remaining chapters of the lifecycle arc; not yet specified in-repo.
- **Coda — the slot machine.** A standalone, deterministic slot model
  (`SlotGameModel`) validated by a million-spin par-sheet test (RTP, hit
  frequency, exposure caps).

## Credits

**Environment assets:** *House Furniture* and *Modular House* by **QuadArt**, used
with permission — https://fab.com/sellers/QuadArt
