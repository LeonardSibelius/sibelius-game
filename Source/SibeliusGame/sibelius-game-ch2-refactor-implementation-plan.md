# Ch2 — Refactor — Implementation Plan + Predict-Bugs Ledger

*Front-loaded spec for the second story chapter. Companion to `sibelius-game-seven-chapter-build-map.md` (Ch2) and the shipped Ch1. Linear: **SIB-26**. Branch: `feat/v0.2-realistic-office`. Ch1 (SIB-25) shipped June 2 — both smoke tests green; this builds on that foundation.*

**Gate FIRST, same as Ch1.** Fill Phase 0 and write the smoke-test assertions before the edit logic. Ch2 is the chapter where the **snapshot/revert** system is born — Ch4 Test-Drive reuses it wholesale — so getting the snapshot design right here pays off twice.

---

## The feeling we're building

> "Oh, I can change what's here."

Look at an object, hold the **Refactor** key — and you reshape it. A crate too big to pass shrinks until the path clears. A solid wall panel turns transparent and non-blocking, revealing the mechanism behind it. The engineer doesn't just *see* the world's structure now (Ch1) — he *edits* it. Mechanism is theme: refactoring is making a stubborn thing become what you need.

---

## Carry-forward from Ch1 (these are settled — don't relearn them)

1. **C++ for gameplay logic, minimal editor wiring.** Same as the Refusers and Code Vision. The editor is for assets, input actions, and placement only.
2. **`BindAction` needs `.Get()`** when binding to a `TObjectPtr` component.
3. **Actor-finds-player at BeginPlay times out in the heavy office** — the door bind needed a 30s retry + MPC-poll fallback. Any new "find the player" code uses the same robust pattern (or binds from the character side).
4. **Per-instance materials are mandatory** (this *is* the Ch2 top risk, R1 — see below).
5. **Ship straight into `L_Office_v02`** — no throwaway flat level; the office already works. Point the new smoke test at the office.
6. **Reserved stencil range** is documented in `CodeVisionStencil.h` (250). Refactor's selection highlight uses **251** if we add an outline (see Phase 3).
7. **Conventional commits** + `Co-Authored-By` trailer; push `feat/v0.2-realistic-office:main` at ship; smoke tests gate it.

---

## ⚠️ ONE SCOPE DECISION — needs your nod before I code

Two ways to do the "edit properties" mechanic:

**A) MVP — object-declared refactor (recommended).** Each refactorable object carries a component that declares *its own* refactor (this crate's refactor = shrink to 0.4×; this wall panel's refactor = swap to the transparent material + drop collision). The player targets it and triggers the change — a clean binary **normal ⇄ refactored** toggle, with full snapshot/revert. The player still chooses *what* and *when*; the level author chooses the *what-it-becomes*. Ships fast, fully testable, delivers the feeling, and builds the snapshot system Ch4 needs. **This is what the plan below specs.**

**B) Full engineer's editor — player-driven property dial + radial UI.** Player selects an object, opens a radial, picks a property (color / scale / mass / transparency), and dials the value freely. Much richer and truer to the "engineer edits anything" fantasy — but it's a real UMG/UI build (the slow, fiddly Blueprint-graph kind we keep avoiding), and a much bigger surface to test.

**Recommendation:** ship **A** as Ch2, and file **B** as a later enhancement once the underlying edit+snapshot system (built in A) is proven. Mirrors how we made the Ch1 Investigation Board a stretch. Say the word if you'd rather swing for B now.

*The rest of this doc assumes A.*

---

## Architecture (snapshot-driven, C++-first)

```
Player holds R (Refactor mode)
        │
        ▼
URefactorComponent (on character)
  • camera line-trace each tick → nearest actor with a URefactorableComponent
  • HUD prompt "[R] Refactor" when one is targeted (+ optional stencil-251 outline)
  • on trigger → target->ApplyRefactor()  (or RevertRefactor() to toggle back)
        │
        ▼
URefactorableComponent (on each editable object) — owns its own state + snapshot
  • EditType: Material | Scale | (Mass — deferred)   [hard-listed, R5]
  • target values (RefactoredMaterial / RefactoredScale)
  • FRefactorSnapshot: captures ORIGINAL material(s), scale, [mass] on first edit
  • ApplyRefactor(): per-instance MID for material (R1); SetWorldScale3D + collision
    update (R2); flips bIsRefactored; broadcasts
  • RevertRefactor(): restores exactly from the snapshot (R6)
```

Core principle, same spirit as Ch1's single-source-of-truth: **each refactorable owns and reverts its own state via one snapshot struct.** The player component only *selects and triggers* — it never stores per-object edit state. That keeps revert correct (R6) and gives Ch4 a clean, per-object snapshot to branch on.

### Components / assets

- **`URefactorComponent`** (C++, on `SibeliusGameCharacter`). Input-driven: enter refactor mode, camera-trace for a `URefactorableComponent`, expose the current target, trigger apply/revert. Holds **no** per-object state.
- **`URefactorableComponent`** (C++ `UActorComponent`, added to editable objects). Declares `EditType` + target value(s); owns `FRefactorSnapshot`; `ApplyRefactor()` / `RevertRefactor()` / `IsRefactored()`; `FOnRefactorChanged` delegate. **This is the load-bearing, Ch4-reused piece.**
- **`FRefactorSnapshot`** (USTRUCT): original material array (per slot), original world scale, [original mass]. One struct captured atomically on first edit so revert is exact (R6) — designed now so Ch4 Test-Drive serializes the same shape.
- **`IA_Refactor`** (Input Action) added to `IMC_Default` on a key (suggest **R**).
- **`MI_RefactoredGlass`** (or similar) — a sample target material for the wall-panel puzzle (transparent/revealing). Author 1–2 demo target materials.
- **(Optional, Phase 3) `PP_Outline`** — a post-process outline reading custom stencil **251** to highlight the targeted object. Skippable for the green bar; a HUD text prompt is enough to function.

### The two MVP edit types

- **Material** (the R1 showcase): on apply, create a **Dynamic Material Instance per slot on this mesh only** and swap/param it; snapshot the originals. Never touch the shared source material. Puzzle: a wall panel → transparent + collision off → reveals/*opens* the way to a mechanism.
- **Scale** (R2): `SetWorldScale3D` then force a collision/bounds update; snapshot the original. Puzzle: an oversized crate shrinks until you can pass.

**Mass / "make liftable" is deferred** — it depends on a carry system the FP template doesn't have yet. Flagged in the ledger (R3) and revisited if/when a carry mechanic exists; for now the two visible, self-contained edits above carry the chapter.

---

## Phased build (pause-point checkpoints)

**Phase 0 — Predict-bugs gate (DO FIRST).** Fill the ledger; write the smoke-test assertion stubs (red) before edit logic.

**Phase 1 — Selection.** `URefactorComponent` + `URefactorableComponent` (empty edit for now). `IA_Refactor` on **R**. Camera trace finds the nearest refactorable; log/HUD-prompt the target. **Verify:** looking at a tagged object in `L_Office_v02` reliably reports it; looking away clears it.

**Phase 2 — Material refactor (R1 front and center).** `ApplyRefactor` for `EditType::Material`: per-instance MID, swap to the target material, snapshot originals; `RevertRefactor` restores. **Verify:** refactoring one of two identical wall panels changes **only that one** (sibling untouched — the whole point); revert restores exactly.

**Phase 3 — Scale refactor + collision (R2).** `EditType::Scale`: `SetWorldScale3D` + collision/bounds update; snapshot + revert. **Verify:** shrinking the blocking crate updates its collision so you can actually walk past (not just visually smaller); revert restores size + collision.

**Phase 4 — (Optional) selection outline.** `PP_Outline` reading stencil 251 for a clean targeted-object highlight. Not required for green.

**Phase 5 — Smoke test + ship.** `URefactorSmokeTestCommandlet` (`-run=RefactorSmokeTest`) green on `L_Office_v02`; office regression (`SibeliusSmokeTest`) still green; commit + push to main; SIB-26 → Done. Next link: **Ch3 — Compile (SIB-27)**.

---

## Predict-bugs ledger (guard + test — the preflight gate)

| ID | Predicted bug | Guard | Test that proves it |
|----|---------------|-------|---------------------|
| **R1** ⚠️ | Editing a material changes **all** instances sharing it | Create a **per-actor Dynamic Material Instance** before editing; never modify the source material | Smoke test: refactor one of two siblings; assert the other's material is unchanged |
| **R2** ⚠️ | Runtime scale change doesn't update collision / leaves nav stale → looks smaller but still blocks | After `SetWorldScale3D`, force collision/bounds update (and nav rebuild if the object affects nav) | Assert the shrunk crate's collision extent shrank and the player can pass; revert restores both |
| **R3** | Mass/physics edits destabilize (jitter, tunneling, launch) | **Deferred** — mass/liftable needs a carry system; not in MVP scope. When added: clamp ranges, re-init the body cleanly | N/A this chapter; flagged so it isn't silently attempted |
| **R4** | Selection ambiguity in the cluttered office (wrong actor picked) | Trace returns the nearest actor **with a `URefactorableComponent`**; HUD-confirm the target before applying | Trace test returns the intended refactorable, not a random office prop in front of it |
| **R5** | Editable-property scope creep (every property becomes a feature) | Hard-list the edit types in an enum: **Material, Scale** (Mass deferred). No free-form property editing in the MVP | Code review: `ERefactorEditType` is a closed set; smoke test only exercises the listed types |
| **R6** ⚠️ | Revert incomplete (visual reverts, scale/collision doesn't, or partial material) | Capture **all** edited fields in one `FRefactorSnapshot` atomically on first edit; revert restores the whole struct | Round-trip test: apply → revert → assert material, scale, AND collision identical to pre-edit |
| **R7** | "Find the player" selection times out in the heavy office (the Ch1 door bug) | The component lives **on the character**, so it already has the player — no find-player race. Trace originates from the owner's camera | Selection works on first frame of refactor mode in `L_Office_v02` |
| **R8** | MID leak / perf if a new MID is made on every toggle | Create the MID **once** per actor (cache it); reuse on re-refactor; revert swaps the material pointer, doesn't spawn MIDs | Toggle 10× and assert only one MID exists on the actor |
| **R9** | Snapshot taken twice (re-refactor overwrites the original with the already-edited state) | Snapshot only when **not already** holding one (`bHasSnapshot` guard); subsequent applies reuse it | Apply → apply → revert restores the TRUE original, not the refactored state |
| **R10** | Edits wrongly persist across chapter boundary (that's Ch5 Deploy, not Ch2) | Refactor edits are chapter-local; revert-all on chapter end / level reload | Leave + re-enter → world is in its original, un-refactored state |

⚠️ = R1, R2, R6 are the load-bearing risks; spend Phase 0 there.

---

## Smoke-test bar (`URefactorSmokeTestCommandlet`, `-run=RefactorSmokeTest`)

Exit 0 only when ALL assert (mirrors the Ch1 / SIB-19 commandlet; helpers in a **named** namespace `RefactorSmokeTestNS` to dodge the unity-build collision):

1. `IA_Refactor` exists and is mapped in `IMC_Default`.
2. `URefactorComponent` is present on the player character class (CDO check).
3. At least one actor in `L_Office_v02` has a `URefactorableComponent`; its `EditType` is in the allowed enum (R5).
4. **Material isolation (R1):** for two refactorables sharing a source material, applying to one creates a unique MID and leaves the other's material untouched.
5. **Scale + collision (R2):** applying a Scale refactor changes the actor scale **and** its collision bounds; revert restores both.
6. **Snapshot round-trip (R6/R9):** apply → apply → revert leaves material + scale + collision identical to the pre-edit original.
7. (Soft) optional `PP_Outline` / target material assets load if present.

Then the office regression: `SibeliusSmokeTest` on `L_Office_v02` still exit 0 (actor band — adding a couple of refactorables + the smoke setup stays in 1000–1150).

These assertions are designed to run **headlessly via a self-test on `URefactorableComponent`** (like Ch1's `RunCollisionSelfTest`) so no spawned pawn is needed — the component applies/reverts on itself and the commandlet checks the result.

---

## Windows three-part-entity handoff

- **Cowork (this doc + next: the C++):** plan, ledger, architecture, the source files.
- **Claude Code in PowerShell:** create/compile `RefactorComponent.{h,cpp}`, `RefactorableComponent.{h,cpp}`, `RefactorTypes.h` (the enum + `FRefactorSnapshot`), `RefactorSmokeTestCommandlet.{h,cpp}`; add the component + `IA_Refactor` binding to `SibeliusGameCharacter`; conventional commits + trailer.
- **By hand in the editor:** `IA_Refactor` → `IMC_Default` on **R**; author 1–2 target materials (e.g., `MI_RefactoredGlass`); add `URefactorableComponent` to a wall panel + a crate in `L_Office_v02` and set their EditType/targets; (optional) `PP_Outline`.
- **UnrealClaude panel:** actor placement/queries only — placing/tagging the refactorable crate + panel, reading back their transforms.

---

## Definition of done

- `-run=RefactorSmokeTest` exit 0 on `L_Office_v02` (material isolation, scale+collision, snapshot round-trip all pass).
- `-run=SibeliusSmokeTest` exit 0 (office regression).
- In-game: look at the crate, hold **R** → it shrinks and you walk past; look at the wall panel, hold **R** → it turns transparent + opens, revealing the mechanism; reverting (or chapter reset) restores both exactly, and refactoring one panel never touches its twin.
- Banked on `feat/v0.2-realistic-office`, pushed to `main`; SIB-26 → Done. Next: **Ch3 — Compile (SIB-27)**.

*Predict the bugs first. The snapshot struct is the thing to get right — Ch4 inherits it. Have fun: this is the chapter where the engineer stops looking and starts editing.*
