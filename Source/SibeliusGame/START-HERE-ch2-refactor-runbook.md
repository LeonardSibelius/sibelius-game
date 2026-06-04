# START HERE — Ch2 Refactor (SIB-26), one step at a time

*Same loop you ran for Ch1, which you've now done end-to-end. Do the next unchecked stage, hit its checkpoint, message me. Paste errors, don't push past red.*

## The five C++ files (copy into `Source/SibeliusGame/`)
`RefactorTypes.h`, `RefactorableComponent.h/.cpp`, `RefactorComponent.h/.cpp`, `RefactorSmokeTestCommandlet.h/.cpp`. Drafts to compile + iterate — `RefactorSmokeTestCommandlet.cpp` has placeholder asset paths and two `[API?]` call sites, same as Ch1.

---

## Stage 1 — Files in + Claude Code wiring
Drop the seven files in `Source/SibeliusGame/`, then paste to **Claude Code**:

> I've added Ch2 Refactor files to `Source/SibeliusGame/`: RefactorTypes.h, RefactorableComponent (.h/.cpp), RefactorComponent (.h/.cpp), RefactorSmokeTestCommandlet (.h/.cpp). Read them, then (don't compile yet):
> 1. In `SibeliusGameCharacter` (.h/.cpp): add a `URefactorComponent*` subobject named `RefactorComp` created in the constructor; add a `UPROPERTY(EditAnywhere) UInputAction* RefactorAction;`; and in `SetupPlayerInputComponent` bind it so `ETriggerEvent::Started` calls `RefactorComp->TriggerRefactor()` — use `RefactorComp.Get()` for the BindAction object (the Ch1 TObjectPtr lesson).
> 2. Make sure `RefactorSmokeTestCommandlet` only builds in editor targets (it already wraps Main in `#if WITH_EDITOR`).
> Show me a summary.

**✅** Claude Code reports the edits. → **Stage 2.**

---

## Stage 2 — Compile
```powershell
cd C:\Users\wpark\projects\sibelius-game
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" `
  SibeliusGameEditor Win64 Development `
  -project="C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" -waitmutex
```
**✅** "Build succeeded." **❌** Paste me the first error block (expect maybe one round — `GetMappings`, or a material/primitive include). → **Stage 3.**

---

## Stage 3 — Editor assets + placement (in `L_Office_v02`)
1. **`IA_Refactor`** (Input Action) in `Content/Input`; add it to **`IMC_Default`** on the **R** key. Assign it to the character's `RefactorAction`.
2. Author one target material for the wall-panel puzzle — e.g., duplicate a translucent/glass material as **`MI_RefactoredGlass`** (or any obviously-different material for now).
3. **Place two refactorables:**
   - A **crate** (any blocking static-mesh actor) in a spot that blocks a path → add a **`Refactorable Component`** → set **EditType = Scale**, **RefactoredScale = (0.4, 0.4, 0.4)**.
   - A **wall panel** (a `1M_Cube` like the Ch1 door works) → add a **Refactorable Component** → **EditType = Material**, **RefactoredMaterial = MI_RefactoredGlass**, tick **bDisableCollisionWhenRefactored**.
   - Make sure each refactorable's mesh is the actor's **root or first mesh component** (the component resolves that automatically).
4. **Save.**

**✅ The fun checkpoint:** Play, look at the crate, press **R** → it shrinks and you can pass; press R again → it restores. Look at the panel, press **R** → it swaps material + opens; again → restores. And refactoring one panel must NOT change an identical twin (R1). Message me **"refactor works"** or what's off — the logs/Output Log will help, and I'll debug from the R-ledger.

---

## Stage 4 — Smoke test green
1. Tell me the real asset paths you used; I'll give you the path edits for `RefactorSmokeTestCommandlet.cpp` (the `IAPath` especially), Claude Code applies, rebuild.
2. Run:
```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" `
  -run=RefactorSmokeTest -unattended -nopause -nosplash -stdout
```
**✅** "REFACTOR SMOKE TEST PASSED (Ch2 green)."

---

## Stage 5 — Office regression + ship
```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" `
  -run=SibeliusSmokeTest -unattended -nopause -nosplash -stdout
```
**✅** exit 0, actor band ok. Then Claude Code commits (Conventional + `Co-Authored-By` trailer) and pushes `feat/v0.2-realistic-office:main`. I'll flip **SIB-26 → Done** in Linear. Next link: **Ch3 — Compile (SIB-27)**.

---

### You're on Stage 1. One box at a time — I'm tracking where we are.
