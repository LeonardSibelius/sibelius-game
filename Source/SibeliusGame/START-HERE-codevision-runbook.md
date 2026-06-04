# START HERE — Code Vision, one step at a time

*Don't read the seven source files. Don't read the plan. Just do the next unchecked step, hit its checkpoint, and message me. We go one box at a time. If a step throws an error, paste me the error and stop — don't push past a red checkpoint.*

*The pattern all session: I (Cowork) hand you exact text; you run Claude Code + PowerShell and click in the editor; you paste results back. Same three-part-entity loop that built CP1–CP3.*

---

## Stage 1 — Put the files where the engine can see them  (~2 min)

**Do:** Save/copy the seven files I just shared into your project's source folder:

`C:\Users\wpark\projects\sibelius-game\Source\SibeliusGame\`

(That's `CodeVisionStencil.h`, `CodeVisionComponent.h/.cpp`, `HiddenDoor.h/.cpp`, `CodeVisionSmokeTestCommandlet.h/.cpp`.)

**✅ Checkpoint:** all seven files are sitting in that folder next to your existing `SlapComponent.cpp`, `RefuserSpawner.cpp`, etc.

**Then message me: "files in place."** That's it for this step.

---

## Stage 2 — Let Claude Code do the wiring  (~10 min, Claude Code drives)

You won't touch C++ by hand. Open **Claude Code** in the project folder and paste it this, verbatim:

> Read `Source/SibeliusGame/` — I've added CodeVisionComponent, HiddenDoor, CodeVisionStencil, and CodeVisionSmokeTestCommandlet. Do three things:
> 1. In `SibeliusGame.Build.cs`, make sure `EnhancedInput` and `InputCore` are in the module dependencies.
> 2. In `SibeliusGameCharacter` (.h/.cpp), add a `UCodeVisionComponent` subobject named `CodeVisionComp`, a `UInputAction* CodeVisionAction` property, and bind it in `SetupPlayerInputComponent` so `Started` calls `ActivateCodeVision()` and `Completed` calls `DeactivateCodeVision()`. (The exact snippet is in `HANDOFF-codevision.md` if you want it.)
> 3. Don't compile yet — just make the edits and show me a summary.

**✅ Checkpoint:** Claude Code reports it edited `Build.cs` and `SibeliusGameCharacter`. Nothing compiled yet.

**Then message me: "wiring done"** and I'll give you the build command.

---

## Stage 3 — First compile  (~3–10 min, your first green/red)

**Do:** In PowerShell (adjust the UE path if yours differs):

```powershell
cd C:\Users\wpark\projects\sibelius-game
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" `
  SibeliusGameEditor Win64 Development `
  -project="C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" -waitmutex
```

**✅ Checkpoint (green):** ends with **"Build succeeded."**

**❌ If it errors:** copy the **first** error block (the topmost one — later errors are usually fallout) and paste it to me. The two spots I already flagged as likely (`GetScalarParameterByName`, `GetMappings`) are easy fixes if they come up. **Don't try to fix it solo — paste it, I'll hand you the correction, Claude Code applies it, recompile.** Expect maybe one round of this. That's normal, not failure.

**Then message me: "compiled"** (or paste the error).

---

## Stage 4 — Make the four editor assets  (~15 min, you click, I talk you through)

This is the only hand-work in the editor. Open the project in UE 5.7. We'll do these **together, one at a time** — when you get here, message me **"ready for assets"** and I'll walk you through each click for:

1. Project setting: Custom Depth-Stencil = "Enabled with Stencil"
2. `MPC_CodeVision` (one scalar, `Active`)
3. `IA_CodeVision` + add to `IMC_Default` on the **V** key
4. `PP_CodeVision` post-process material (the cyan-green overlay — I'll give you the node steps)

Don't try to do these from memory off the plan — let me narrate them live so you're never guessing.

**✅ Checkpoint:** the four assets exist; `IA_CodeVision` is assigned to the character's `CodeVisionAction`, `MPC_CodeVision` to the component.

---

## Stage 5 — The flat test level + the door  (~10 min)

**Do (with me narrating):** New Level → Basic → save as **`L_CodeVisionTest`**. Drag in a `BP_FirstPersonCharacter` setup like `L_RefuserTest`. Place an **`AHiddenDoor`** in front of the player; put a trigger box just past it and tag it **`CodeVisionEndTrigger`**.

**✅ Checkpoint (the fun one):** hit **Play**, hold **V** — the door reveals and you can walk through where there was a wall; release and it's a wall again. *That's Code Vision working.* Grab a screenshot.

**Message me "it works"** (or "the door's misbehaving" + what you see) and we debug from the predict-bugs ledger.

---

## Stage 6 — Make the smoke test green  (~5 min)

**Do:** Tell me the real asset paths you used (I'll have you copy them from the editor), I'll hand you the four edited lines for `CodeVisionSmokeTestCommandlet.cpp`, Claude Code applies + recompiles, then run:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" `
  -run=CodeVisionSmokeTest -unattended -nopause -nosplash -stdout
```

**✅ Checkpoint:** ends with **"CODE VISION SMOKE TEST PASSED (Ch1 green)."**

---

## Stage 7 — Into the office + ship  (~15 min)

Place the door + trigger in `L_Office_v02` (player's room), re-run the office regression:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" `
  -run=SibeliusSmokeTest -unattended -nopause -nosplash -stdout
```

**✅ Checkpoint:** both smoke tests exit 0. Then Claude Code commits + pushes (`feat/v0.2-realistic-office:main`), SIB-25 → Done. **Ch1 shipped.**

---

### The only thing you need to remember

You're on **Stage 1**. Do Stage 1, message me, I take it from there. We are never doing more than one box at a time.
