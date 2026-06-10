# SIB-31 — Cathedral door (Ch7 entry): predicted-bugs ledger

The attic→cathedral threshold. `ACathedralDoor` (Source/SibeliusGame/CathedralDoor.h/.cpp),
IInteractable, **not** IBranchable. E opens `TargetLevelName` (default `L_Cathedral`)
unless branched. Gate: `CathedralDoorSmokeTestCommandlet`.

## Predicted bugs

### D1 — travel while branched → dangling snapshot stack
`OpenLevel` mid-branch would tear down the world under the snapshot stack: the
branches could never be merged or discarded, and a later deploy/apply would read
half-resolved state. **Mitigation:** `ACathedralDoor::IsTravelAllowed(Branch)` —
travel only at depth 0, same reasoning as `UBranchSubsystem::CanDeploy`. Refusal is
legible to the player (red toast: *"Merge or discard your branches before
ascending"*), and the door does nothing else.
**Test:** smoke test drives the static guard against a `NewObject`'d subsystem —
allowed at depth 0, refused at depth 1 (after `EnterBranch`), allowed again after
`DiscardBranch`. PIE re-check (Walt): branch with 7, press E on the door → red
refusal, no level change.

### D2 — bad level name → silent no-op
`UGameplayStatics::OpenLevel` with a name that resolves to nothing fails silently —
E would just do nothing forever. **Mitigation/test:** smoke test asserts the
`TargetLevelName` package exists on disk (`/Game/Maps/L_Cathedral` for the default
short name; a value containing `/` is checked verbatim). `Content/Maps/L_Cathedral.umap`
is present.

### D3 — door leaking into persistence
If the door were IBranchable it would get a GUID, enter the registry, and show up
in deploy saves — orphan noise on every reload of a save written with the door
present. **Mitigation:** ACathedralDoor deliberately does NOT implement IBranchable
(no GUID property at all). **Test:** smoke test asserts the class doesn't implement
the interface, and (record-count proxy) that `RebuildRegistry` over the level with a
door present never indexes it — the registry is exactly what `BuildDeploySave`
gathers from, so no registry entry ⇒ no save record.

### D4 — headless crash
`BeginPlay` returns immediately under `IsRunningCommandlet()` (house rule): no
timer, no player-pawn poll, no visibility flip under the gate. The generate-gate
poll only ever starts in a real game world. **Test:** full gate sweep green (all
smoke commandlets, editor closed).

### D5 — E on the door routing through the dismantle/refund sharp edge
The door is not an `ABuildSite`, so the E-refund soft-lock
([buildsite-dismantle-softlock]) can't apply — but verify focus lands cleanly.
**Test (PIE, Walt):** look at the placed door → prompt reads *"Enter the cathedral
[E]"* and nothing else within trace range claims focus; E travels (unbranched).
Headless half: smoke test asserts the prompt text on every placed door.

### D6 — SibeliusSmokeTest actor-count band
Placing the door adds **+1** actor to L_Office_v02 (~1,074 today; band 1000–1150 —
plenty of headroom). **Test:** re-run `SibeliusSmokeTest` after Walt places the
door; the band assert stays green unchanged.

## Gate modes (pre- vs post-placement)

The smoke test's "at least one placed ACathedralDoor in L_Office_v02" assert is
**strict by default**. Walt places the instance by hand *after* this C++ gate, so
the pre-placement run uses the explicit escape:

```powershell
# pre-placement (today): placed-door assert downgraded to WARN, remaining checks
# run on a transient spawn
-run=CathedralDoorSmokeTest -allowunplaced

# post-placement (the real bar, after Walt's editor work): plain run, strict
-run=CathedralDoorSmokeTest
```

## Generate gate (`bRequireGenerateUse`, default false — v1 ships ungated)

When true, the door starts hidden + `NoCollision` (one `ApplyRevealed` path drives
both, the CV4/CV8 lesson) and a 0.5 s timer polls the player pawn's
`UGenerateComponent::HasSpawnedThisSession()` — a session-local bool set on the
first successful **live** `SpawnEntry` (deploy re-spawns don't count, budget
restores don't count). Reveal clears the timer. Cheap, headless-safe, nothing
persisted.

## Status

- C++ + smoke commandlet: done on `feat/sib-31-cathedral-door`.
- Gate (pre-placement mode + full sweep): see commit message / gate log.
- Next (Walt): place door in L_Office_v02 attic, assign `SM_Door_Cathedral_Huge`,
  save; then plain `CathedralDoorSmokeTest` + `SibeliusSmokeTest` (D6) + PIE checks
  (D1, D5); merge after green.
