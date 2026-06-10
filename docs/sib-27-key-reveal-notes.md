# SIB-27 polish — Ch3 key acquisition: consume-on-build reveal

**Branch:** `feat/sib-27-key-reveal` (off main). Logic in C++, no Blueprint.
**Problem:** the KeyBuildSite (Output = KeyItem, copper key mesh) grants the Key
*and* leaves a dismantlable key prop on the floor. Pressing **E** on that prop runs
the BuildSite's dismantle/refund path, which spends the Key back out of inventory
and soft-locks the player out of the attic.

**Design option C (chosen):** *building the key IS acquiring it.* A brief
float-and-spin reveal of `FinalMesh`, then the mesh disappears and the site
becomes inert (terminal **Consumed** state). The staircase BuildSite keeps its
normal build/dismantle behavior — only sites with `bConsumeOnBuild = true` change.

`bConsumeOnBuild` is a per-instance `UPROPERTY` (EditAnywhere, default **false**).
Walt sets it by hand on the KeyBuildSite instance in the level.

## Reveal feel is PIE-only

The float-and-spin is cosmetic and runs only when the world can render
(`!IsRunningCommandlet() && FApp::CanEverRender() && World->IsGameWorld()`).
Headless (commandlet / -nullrhi / non-game world) skips straight to Consumed so
the gate stays green and deterministic. The smoke test therefore asserts the
*end state*, never the animation.

## Predicted-bugs ledger (honor in code + tests)

| # | Bug predicted | Mitigation | Test |
|---|---------------|------------|------|
| **K1** | Default regresses the staircase | `bConsumeOnBuild` defaults **false**; only consumable sites change behavior | Spawned non-consumable site still passes `RunBuildSelfTest` (build→dismantle→refund); default flag asserted false |
| **K2** | **E** on a consumed site dismantles/refunds → the soft-lock | A consumable site is never dismantlable: `Interact`/`Dismantle` no-op and `GetInteractionPrompt` returns empty when `bConsumeOnBuild` | After consume, `Dismantle` returns false, inventory unchanged, prompt empty |
| **K3** | Reveal not headless-safe | Headless skips the animation and consumes synchronously | Headless `Build` of a consumable site ends **Consumed**, mesh hidden + no collision, Key granted exactly once |
| **K4** | Double-grant on re-build | `Build` is idempotent behind the `bIsBuilt` latch (`CanBuild` false once built) | Re-`Build` of a built site returns false; Key count unchanged |
| **K5** | Reload replays the reveal or re-shows the key | On `BeginPlay`/restore, if `bConsumeOnBuild && bIsBuilt`, present **Consumed** immediately (mesh hidden, not interactable, no animation, no Key minted) | After a save/load round-trip (`RestoreBranchState(1)`) the site is Consumed, mesh hidden, Key intact (restore is RAW) |
| **K6** | Tick/timer leak | The reveal stops driving `Tick` once Consumed (`SetActorTickEnabled(false)`; Tick early-outs unless Revealing) | `IsActorTickEnabled()` is false after consume/reload |

## State machine

```
                 Build() on a consumable site
   (Idle/None) ───────────────────────────────► Revealing ──(~1.5s)──► Consumed
        │                                          (PIE only)             ▲
        │                                                                 │
        └────────────── Build() headless, or BeginPlay/restore of a ──────┘
                         built consumable site → straight to Consumed
```

`Revealing`: `FinalMesh` floats up ~60u and spins (yaw) over ~1.5s, then
`SetVisibility(false)` + `SetCollisionEnabled(NoCollision)` → `Consumed`.
`Consumed` is terminal: tick disabled, never interactable, never re-shown.

## Files touched

- `Source/SibeliusGame/BuildSite.h` / `.cpp` — `bConsumeOnBuild`, reveal state
  machine, Consumed terminal state, Interact/Dismantle guards, `RunConsumeOnBuildSelfTest`.
- `Source/SibeliusGame/CompileSmokeTestCommandlet.cpp` — K1–K5 state asserts on
  spawned controlled instances, plus real placed sites routed by `bConsumeOnBuild`.
