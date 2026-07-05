# Claude Code handoff — World Three P0 + Attic "Carousel of Fates" door (June 13, 2026)

Cowork drafted these; Walt robocopies into the game repo; **Claude Code integrates, compiles, gates, commits.**
Full design context: `walt-cowork-memory/sibelius-game-world3-sauce-cauldron-spec.md` (SS1–SS12 ledger).

---

## Two work items in this drop

**A. World-three P0** — the cauldron + book-rain C++ stubs + smoke gate (unblocks placing the cauldron in `L_AI_Temple`).
**B. Attic door redesign** — convert the attic `ACathedralDoor` → a Code-Vision `AHiddenDoor` with the "Carousel of Fates" sign.

---

## File manifest + where each goes

| File | Destination | Module |
|------|-------------|--------|
| `SauceCauldron.h` / `.cpp` | `sibelius-game/Source/SibeliusGame/` | RUNTIME |
| `BookRain.h` / `.cpp` | `sibelius-game/Source/SibeliusGame/` | RUNTIME |
| `SauceSmokeTestCommandlet-SKELETON.cpp` | `sibelius-game/Source/SibeliusGameEditor/` | EDITOR-ONLY |
| `import_carousel_sign.py` | `C:/Users/wpark/Claude/` (run via `py`) | — |
| `T_sign_carousel.png` (in repo root, sibling of this folder) | `sibelius-game/Tools/Art/` | — |

Stage with **robocopy** (long-path-safe), then `git status --short` before committing. **Cowork does NOT run git** (sandbox `index.lock` trap) — Walt runs it.

---

## A. World-three P0 — integration notes

1. **`ASauceCauldron`** (runtime): state holder + `IInteractable`. `FeedSauce(Delta)` clamps `BlendProgress`, fires `OnSauceComplete` ONCE at threshold (one-shot latch, idempotent — SS9). P0 `Interact` is a status log; P5 wires `OnSauceComplete` → `USibeliusProgressSubsystem::bSauceComplete` + `AAIApparition::TriggerApparition(clue)`.
   - **Reconcile `IInteractable`**: the stub assumes `Interact_Implementation(AActor*)` + `GetInteractionPrompt_Implementation()`. Match the EXACT signatures/param types in the project's `Interactable.h` (HatchLock/CathedralDoor are the reference implementations). Fix the include path if it isn't `Interactable.h`.
2. **`ABookRain`** (runtime): pooled mesh-component spawner — books fall from `SourceLocations` to `MouthLocation` along a quadratic arc and recycle (NO per-book destroy — SS1). Cosmetic + transient: never `IBranchable`, never saved (SS3). `FeedPerBook` stays 0 in P1 (ambient = spectacle); the P2 loop drives the meter.
3. **`SIBELIUSGAME_API`** is on both classes so the editor module's commandlet can reference them (PK13). If the editor module needs new deps to see them, add to `SibeliusGameEditor.Build.cs`.
4. **`SauceSmokeTestCommandlet`**: the SKELETON spells out the 3 asserts only. **Clone the Main()/world-setup/cleanup shell from an existing gate** (`RefuserSmokeTestCommandlet` / `GenerateSmokeTestCommandlet`) so it matches the family: NAMED namespace + `using` in function bodies only; `World->CleanupWorld()` on exit (else exit 3); no variable shadowing (warnings-as-errors). Add a `.h` mirroring the existing commandlet headers.
5. **Build + gate** (EDITOR CLOSED — port 3000):
   ```
   Build.bat SibeliusGameEditor Win64 Development -project=...SibeliusGame.uproject -waitmutex
   UnrealEditor-Cmd.exe ...SibeliusGame.uproject -run=SauceSmokeTest -unattended -nopause -nosplash -stdout
   ```
   Expect: `=== SAUCE SMOKE TEST PASSED (World3 P0 …) ===`, exit 0. Then run the full sweep to confirm no regressions.
6. **Walt's editor placement (after compile):** in `L_AI_Temple`, place `ASauceCauldron` at the base of the central golden spire (between the dragons); place `ABookRain`, set `SourceLocations` to the spire crown, `MouthLocation` to the cauldron mouth, assign book meshes (free "Low Poly Realistic Books" once installed), point `Cauldron` at the placed cauldron. PIE is the real gate for the rain (SS8).

---

## B. Attic "Carousel of Fates" door

Goal: the attic→cathedral door becomes a **second `AHiddenDoor`** (the class already does Code-Vision reveal + per-instance `SignMesh`/`SignTexture` + `TravelTargetLevel` + revealed-state QueryOnly/Pawn-ignore collision — SIB-44). Only the sign + target differ from the office obelisk.

1. Robocopy `T_sign_carousel.png` → `Tools/Art/`; run `import_carousel_sign.py` (`py "..."`, Cmd box). Imports to `/Game/Signs/T_sign_carousel`, **import-only** (does NOT auto-assign — don't clobber the obelisk's Sauce sign).
2. In `L_Office_v02` attic: delete the old `ACathedralDoor`, place an `AHiddenDoor` in its transform. Set `DoorMesh = SM_Door_Cathedral_Huge`, `TravelTargetLevel = L_Cathedral`, `TravelPromptText = "Enter the Carousel of Fates [E]"`, `SignTexture = T_sign_carousel`. Sign orientation **roll-only, never pitch=90** (June 12 gimbal-lock lesson; start from the Sauce sign's values: Location (0,70,10), roll-90, Width 100, Height ~320 and adjust to the door).

### ⚠️ GATE CAVEAT — check BEFORE deleting the attic ACathedralDoor
`CathedralDoorSmokeTest` strict-asserts a **placed `ACathedralDoor`**. Removing the attic one may turn that gate red.
- Confirm which map the commandlet loads and whether it requires the attic door specifically.
- The cathedral's **return doors are still `ACathedralDoor`**, so the class isn't dead — but if the gate loads `L_Office_v02` and expects a CathedralDoor there, update/repoint the assert (it should now expect the `AHiddenDoor`, or target the map that still has a CathedralDoor).
- This is the standing "check the gate before changing a load-bearing actor" rule (it saved the staircase and the obelisk). Resolve it, then re-run `CodeVisionSmokeTest` + `CathedralDoorSmokeTest` editor-closed.

---

## Suggested commit (Walt runs)

```
feat(world3): P0 Sauce cauldron + book-rain stubs + gate; attic Carousel-of-Fates hidden door
- ASauceCauldron (IInteractable, one-shot completion), ABookRain (pooled fall-and-recycle), SauceSmokeTest
- Attic ACathedralDoor -> AHiddenDoor + T_sign_carousel sign, TravelTargetLevel=L_Cathedral
- CathedralDoorSmokeTest assert updated for the door swap
Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```

---

## Predict-bugs to watch (from the spec)

SS1 pool no-destroy · SS3 books never persist · SS5 floor-plane anti-fall-through in L_AI_Temple · SS8 headless gate sees STATE only, PIE is the real visual gate · SS9 one-shot completion · SS10 interaction-prompt HUD still owed (cauldron prompt will be tiny until then) · plus the gate caveat above.
