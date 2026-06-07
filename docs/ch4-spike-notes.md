# SIB-28 — Ch4 "Test-Drive" research spike

**Type:** research spike (answers + throwaway proof → go/no-go), not a build.
**Timebox:** ~90 min. **Branch:** `feat/sib-28-spike` (nothing on main).
**Mechanic:** enter a *branch reality*, act without consequence, then **merge** to
main world or **discard**. Snapshot a **bounded, declared** state set — never a
world dump. Desaturated post-process while branched.

**Smoke-test bar (for the eventual build):** entering a branch records the
declared set · in-branch edits don't touch main until merge · discard restores
EXACTLY · merge applies once cleanly (idempotent) · nested/re-entered branches
don't leak.

---

## STEP 1 — State inventory + branch manifest (read-only) — answers T1

### What player-mutable state exists today (office game, Ch1–Ch3)

Source of truth read directly from `Source/SibeliusGame/*.h`.

| # | State | Owner / field | Type | Player verb that changes it | In declared set? |
|---|-------|---------------|------|------------------------------|------------------|
| 1 | Refactor applied | `URefactorableComponent::bIsRefactored` (owns `FRefactorSnapshot`) | bool / object | **Refactor (R)** | **YES** |
| 2 | Inventory ledger | `UInventoryComponent::Counts` `TMap<EResourceType,int32>` = {Book, Key} | 2× int32 | collect / build-spend / build-grant / unlock-spend | **YES** |
| 3 | Build-site built | `ABuildSite::bIsBuilt` (owns `ApplyBuiltState`) | bool / site | **Build (B)** / Dismantle (E) | **YES** |
| 4 | Hatch locked | `AHatchLock::bLocked` (owns `ApplyLockedState`) | bool / hatch | Unlock (E, spends 1 Key) | **YES** |
| 5 | Book pickup taken | `ABookPickup::bCollected` | bool / pickup | Collect (E) → +1 Book | **DECISION** (see below) |
| 6 | Corkboard fired | `ACorkboardTrigger::bTriggered` | bool | one-shot narrative trigger (Ch1) | exclude (progression) |
| 7 | Hidden door revealed | `AHiddenDoor::bRevealedState` | bool | Ch1 reveal | exclude (out of Ch4 scope) |
| 8 | Code-vision on | `UCodeVisionComponent::bIsActive` | bool | toggle view mode | exclude (transient view mode, not world state) |
| 9 | Hall alarm | `UHallAlarmSubsystem::bAlarmTriggered` | bool | enter hall (Ch3) | exclude / **suspend** while branched |
| 10 | Compile-end fired | `UCompileEndSubsystem::bFired` | bool | reach end (Ch3) | exclude (progression one-shot) |
| 11 | Refuser waves + spawned AI | `URefuserSpawner::WavesRemaining` + spawned `ARefuserController` pawns | dynamic actors | spawned by alarm | **exclude by design** (T2) |
| 12 | Tool targeting | `URefactorComponent::CurrentTarget`, `UBuildComponent::CurrentSite` | transient ptr | per-tick trace | exclude (derived/transient) |

### Key architectural finding (de-risks the whole chapter)

The codebase is **already built for this**. Every world-edit object *owns and
reverts its own state*:
- `URefactorableComponent` — owns `FRefactorSnapshot`, captured once (R9),
  restored whole (R6); `RunRefactorSelfTest()` **already proves** apply→apply→
  revert restores material+scale+collision byte-for-byte.
- `ABuildSite` / `AHatchLock` — private `ApplyBuiltState(bool)` /
  `ApplyLockedState(bool)`; state is one bool each.

So the branch round-trip is just the **aggregate of already-proven per-object
restores + the inventory ledger restore**. The header comments even call this
out: RefactorTypes.h says *"Ch4 Test-Drive serializes this same shape, so design
it well here"* and BuildSite.h cites *"same principle as Ch2's
URefactorableComponent."* This is the intended seam.

### The declared set

```
Declared set  =  Inventory ledger (whole)  +  { Refactorables }  +  { BuildSites }  +  { HatchLocks }
```

Everything in it is a **bool-per-object plus two ints**. Progression/narrative
one-shots (alarm, compile-end, corkboard) and **dynamic AI** (Refusers) are
deliberately **out** — and *suspended* while branched (see T2) so the snapshot
stays bounded and complete. That suspension is what makes T1 (completeness)
tractable: you can't leak state you can't mutate in-branch.

**BookPickup decision (#5).** Collecting a book mutates *both* inventory and the
pickup's `bCollected`. If collection is allowed in-branch we MUST snapshot
`bCollected` too, else discard restores the count but the book stays gone (a
leak). Two clean options:
- **(A, recommended) Suspend pickups while branched.** Test-Drive is for
  *engineering* verbs (refactor / build / dismantle / unlock), not scavenging.
  Smallest declared set, zero pickup bookkeeping.
- **(B) Include `ABookPickup::bCollected`** in the declared set (one more bool
  per pickup) if product wants "grab this book, then test-build with it."
  Trivial to add; raises the completeness surface.

### The branch manifest — exact serializable struct (the T1 answer)

Each branchable object exposes a tiny capture/restore via a new interface; the
subsystem aggregates. State per object is a single bool, so:

```cpp
// One declared, registered, branchable world-edit object.
USTRUCT()
struct FBranchObjectState
{
    GENERATED_BODY()
    UPROPERTY() int32 RegistryIndex = INDEX_NONE; // in-session identity (see note)
    UPROPERTY() uint8 State = 0;                   // bIsRefactored / bIsBuilt / bLocked
};

USTRUCT()
struct FResourceEntry
{
    GENERATED_BODY()
    UPROPERTY() EResourceType Resource = EResourceType::Book;
    UPROPERTY() int32 Count = 0;
};

// The entire branch snapshot. Bounded by construction.
USTRUCT()
struct FBranchManifest
{
    GENERATED_BODY()
    UPROPERTY() TArray<FResourceEntry>     Resources;  // whole ledger (2 entries today)
    UPROPERTY() TArray<FBranchObjectState> Objects;    // one per registered branchable
};

// Implemented by URefactorableComponent / ABuildSite / AHatchLock.
class IBranchable
{
    virtual uint8 CaptureBranchState() const = 0;  // pack its bool(s)
    virtual void  RestoreBranchState(uint8 State) = 0; // set raw state, DON'T replay the verb
};
```

**Identity note.** Ch4 branching is **same-session** (no save/load — that's Ch5
Deploy, ledger C5). So identity can just be a **registry index** into the
subsystem's `TArray<IBranchable>` collected on branch-enter — no GUIDs needed
yet. `FBuildRecord::SiteId` (FGuid) already exists as Ch5 groundwork; promote to
GUIDs only when Test-Drive state must survive a save.

**Restore rule (critical).** On discard, call `RestoreBranchState(rawBool)` /
write the ledger counts directly — **never replay Build()/TryUnlock()/Collect()**,
because those have side effects (spend inventory, grant key, hide pickup). Raw
state in, no verbs. The inventory is restored as a whole, so the
build-spends-books / unlock-spends-key couplings restore consistently.

### Projected size (preview of T3)

`2 resource ints + N booleans`. Even at, say, 40 refactorables + 12 build sites
+ 3 hatches that's ~55 bytes of real state. **Trivial** — confirmed in T3.

---

## STEP 2 — Round-trip proof (throwaway)

No C++ compiler in the spike shell (and a UE build is out of timebox), so the
proof is in two parts, both on `feat/sib-28-spike`, both clearly marked spike:

1. **`docs/spike/branch_roundtrip_proof.mjs`** — a runnable Node model of the
   declared-set branch semantics (subsystem + the four engineering verbs). It
   actually executes here and exercises the **entire smoke-test bar**.
2. **`docs/spike/BranchSpike_RoundTrip.cpp`** — the in-engine equivalent as a
   reference free function over the *real* classes (`URefactorableComponent`,
   `UInventoryComponent`). Kept out of `Source/` so it can't break the module
   build; drop behind a commandlet wrapper (copy RefactorSmokeTest's scaffold)
   to run for real.

### Result — `node docs/spike/branch_roundtrip_proof.mjs` → exit 0

```
=== SIB-28 branch round-trip proof ===
  [PASS] enter records the declared set (== main)
  [PASS] in-branch edits do NOT touch main before merge/discard
  [PASS] live actually changed (refactor+build+unlock applied)
  [PASS] discard restores main EXACTLY
  [PASS] merge applies the branch to main
  [PASS] merge closes the branch
  [PASS] merge is idempotent (second apply == first)
  [PASS] nested discard returns to the outer branch exactly (no leak in)
  [PASS] outer discard returns to main exactly (no leak out)
  [PASS] re-entered branch starts clean from main (no residue)
=== ALL PASS (declared-set round-trip proven) ===
```

**Verdict:** the declared-set approach restores EXACTLY, merges idempotently, and
nests without leaking — every line of the smoke bar passes against the model.
Two design rules the proof made concrete:

- **Restore writes RAW state, never replays verbs.** `Build()`/`TryUnlock()`/
  `Collect()` have side effects (spend books, grant/spend key, hide pickup).
  Discard sets `bIsBuilt`/`bLocked`/`bIsRefactored` and the ledger *directly*;
  the inventory is restored as a whole so the spend/grant couplings stay
  consistent. (The C++ sketch restores the ledger as a delta to the captured
  counts because `UInventoryComponent` has no setter — production should add
  `Capture/Restore` to the IBranchable surface so it's a straight overwrite.)
- **Merge = "keep live, advance the baseline."** Idempotency falls out for free:
  re-restoring to the merged baseline is a no-op. The guard only has to stop a
  *double-apply* from a UI double-fire (T5).

The in-engine version is even safer than the model: per-object visual exactness
(material/scale/collision) is **already proven** by `RunRefactorSelfTest()`, so
Ch4 only adds the one declared bool + the ledger on top of a proven base.

---

## STEP 3 — Risk verdicts

### T2 — Refusers / ragdolls on restore → **EXCLUDE by design + FREEZE, don't snapshot**

`ARefuserSpawner` spawns AI pawns in waves on the Hall alarm (timers, navmesh
projection, behaviour-tree + ragdoll physics). Their true state — transform,
velocity, ragdoll bodies, AI blackboard/BT, nav path — is exactly the **unbounded
world-dump T1 forbids**, and fragile to serialize/replay.

**Verdict:** keep dynamic actors out of the declared set. While branched:
**suspend** the spawner (drop its alarm subscription + pause its wave timers) and
**freeze** any already-live Refusers (disable AI tick, set physics asleep/
kinematic). On exit (merge *or* discard) they resume in place — **no per-actor
snapshot, no rewind, zero leak surface**. Justification: (1) snapshotting them is
the trap we're avoiding; (2) they're spawned by progression, which we already
suspend; (3) design intent is an *engineering* sandbox, not test-driving combat.
Edge: branching mid-chase freezes the threat for the branch's duration — if that
reads as exploitable, gate branch-entry to non-alarm states; default = allow +
freeze.

### T3 — Snapshot size → **trivial, confirmed**

Declared set = `2 int32 (ledger) + N booleans`. Today N is tiny (a few
refactorables, ~2 build sites, 1 hatch). Pessimistic 40 refactor + 12 build + 3
hatch = 55 bools + 2 ints ≈ **63 bytes packed** (~300 bytes as a
`TArray<FBranchObjectState>`). Captured synchronously in **one frame**; no
streaming, no async, no perf budget needed. The size discipline is enforced by
construction (declared set, dynamic actors excluded), not by hoping the dump
stays small.

### T4 — Branch-state signaling → **design note (post-process + HUD)**

- **Post-process:** on enter, blend in a **desaturate + cool-tint** PP over ~0.2s
  (a global PP material driven from `SibeliusGameCameraManager`, or an unbound
  `APostProcessVolume` whose blend weight the subsystem lerps); blend out on exit.
  The desaturation is the always-on "this isn't real yet" cue.
- **HUD marker:** persistent framed vignette + a top strip
  `⎇ BRANCH REALITY — [E] merge · [Q] discard`, showing **nesting depth** when >1.
  Reuse the existing alarm screen-flash/alert-text plumbing (`PlayAlarmFeedback`)
  for the overlay — the path already exists.

Driven entirely by the subsystem's Branched state, so signal and state can't
desync. No experiment needed.

### T5 — Idempotency guard location → **inside `UBranchSubsystem`, at the resolve transition**

The subsystem is a 3-state FSM: **Idle → Branched(depth ≥ 1) → Resolving → Idle**.
`merge()`/`discard()` are valid **only** from Branched; the first one latches the
branch into **Resolving** (and sets a per-branch `bMerged`), so any second
merge/discard from a UI double-fire (button + keybind, double-click) is a **no-op**.
The guard lives in the subsystem — the **single authority** — never in the UI,
which is allowed to be dumb. Rule: *exactly one resolution (merge XOR discard) per
branch entry; latch on first, ignore the rest.* This is the `top.merged` guard
from the proof, promoted into the state machine.

### T6 — Precedence: Test-Drive (Ch4) vs Deploy (Ch5)

**Deploy persists *main* to disk; you can only Deploy from main, never from inside
a branch.** A branch is uncommitted and ephemeral, so a save must be preceded by a
merge-or-discard — the subsystem refuses Deploy while Branched (or auto-resolves
first, per product call). The two compose because they speak the *same shape*:
`FBranchManifest` IS the Deploy payload. Ch4's branching works on the in-session
**committed** declared set; Ch5 Deploy serializes that committed set to disk. The
only delta is **identity promotion** — Ch4's registry-index identity becomes a
stable `FGuid` (the `FBuildRecord::SiteId` groundwork already sitting in
CompileTypes.h). One line: *branch ⊏ session; deploy reads main only; merge
precedes deploy.*

---

## FINISH — Go/No-Go + phased implementation plan

### Recommendation: **GO** (low risk, high architectural alignment)

The chapter's #1 risk — T1 snapshot completeness — is **tractable and small**.
The state players can change is a **bounded declared set of bools + two ints**;
the codebase is *already* architected around "every object owns and reverts its
own state" (the Refactorable/BuildSite/HatchLock comments explicitly name Ch4 as
the consumer); per-object visual exactness is **already proven** by
`RunRefactorSelfTest()`; dynamic actors are **excluded by design** (freeze, don't
snapshot); and the declared-set round-trip — discard-exact, merge-idempotent,
nested-no-leak — is **proven** by the Step-2 spike. There's no world-dump, no
serialization of physics/AI, no novel persistence (that's Ch5). Build it.

One product call to make before Phase 1: **BookPickup in-branch** — suspend
pickups (recommended, smallest set) vs include `bCollected` (one bool/pickup).

### Phased plan (Ch3-style: each phase ships green with its own smoke asserts)

**Phase 0 — Seam: `IBranchable` + registry (no player-visible behavior).**
- Add `IBranchable { CaptureBranchState()/RestoreBranchState() }`, implemented by
  `URefactorableComponent`, `ABuildSite`, `AHatchLock`; add `Capture/Restore` to
  `UInventoryComponent` (whole-ledger overwrite). `UBranchSubsystem` registers
  branchables on BeginPlay and aggregates `FBranchManifest`.
- *Smoke:* registry finds ≥1 of each declared type in L_Office_v02 · each
  branchable Capture→mutate→Restore round-trips (reuse `RunRefactorSelfTest`) ·
  inventory Capture→spend→Restore is exact.

**Phase 1 — Enter / Discard (the core safety).**
- `EnterBranch()` snapshots the declared set; `DiscardBranch()` restores it
  exactly via raw-state writes (never replay verbs). On enter: suspend
  progression (alarm/spawner/pickups) + freeze live Refusers; resume on exit.
- *Smoke (the bar):* entering records the declared set (manifest == main, non-empty)
  · in-branch refactor+build+unlock leave the committed set untouched · discard
  restores EXACTLY (manifest deep-equal) · Refusers frozen while branched, resume
  on exit.

**Phase 2 — Merge + idempotency.**
- `MergeBranch()` advances the baseline once; FSM `Branched → Resolving` guard +
  per-branch `bMerged` make a double-fire a no-op.
- *Smoke:* merge applies once (committed == live) · second merge/discard is a
  no-op · merge-then-discard rejected.

**Phase 3 — Nesting + signaling.**
- Stack-based nested branches; desaturate post-process + HUD branch marker driven
  by the subsystem's Branched depth.
- *Smoke:* nested discard → outer exact · outer discard → main exact · re-enter
  clean (no residue) · PP/HUD active iff depth ≥ 1.

**Phase 4 — Deploy boundary (Ch5 seam; guard only, no persistence yet).**
- Subsystem refuses Deploy while Branched (or auto-resolves first).
- *Smoke:* save/Deploy entry point errors/no-ops while Branched, allowed from main.

The Phase-1 smoke asserts ARE the user's stated bar; Phases 2–3 cover the
merge/idempotent/nested lines. Recommend a `BranchSmokeTestCommandlet` mirroring
the existing per-chapter commandlets, growing one assert block per phase.

### Spike status
All four steps completed within the ~90-min timebox. Throwaway artifacts
(`docs/spike/*`) to be deleted at implementation start. Nothing landed on main;
all work is on `feat/sib-28-spike`.

