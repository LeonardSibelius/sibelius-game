# SIB-43 — The Clue Loop (oracle computers, return door, progress flags)

Walt's spec (June 11, late): **the clues come from the AI.** The opening
apparition names the quest. The cathedral exit door (E) returns you to the
office. The office COMPUTERS — SM_Monitor_A1, SM_Laptop_A3, SM_Keyboard_A1 —
are the always-available oracle: E at ANY time replays the CURRENT clue
(clue 1 "Find the Carousel of Fates" before the slot is played; clue 2 "Find
the Sauce of all Knowledge" after). The FULL apparition ceremony plays EVERY
time — halo, gaze-steal, god-voice, input lock. Walt, overruling the lighter-
repeat idea: "I really like that Mount Sinai type of episode." The corkboard
stays Mrs. Hall's territory (objection + zombies) — computers speak for the
AI, the corkboard speaks for her.

## CL ledger — predicted BEFORE building

- CL1 INTRO REPLAY. Returning office→cathedral→office reloads L_Office_v02 —
  the intro apparition would re-fire its auto bang every time. Fix: progress
  lives in a GameInstance subsystem (survives OpenLevel); the apparition
  auto-plays only if !bIntroPlayed.
- CL2 FLAGS DIE WITH THE WORLD. OpenLevel destroys all actors — bSlotPlayed
  on the cabinet would be lost. USibeliusProgressSubsystem (GameInstance) is
  the single home for session progress. (Session-only; save-file coupling
  deliberately out of scope, consistent with the free-play slot decision.)
- CL3 SHELL COLLISION. The oracle terminals are invisible boxes over QuadArt
  meshes. They must block ECC_Visibility ONLY (QueryOnly; all other channels
  Ignore) — a stray Pawn block makes invisible furniture.
- CL4 DON'T TOUCH QUADART. Terminals are separate actors placed over the
  meshes by script; the licensed actors are never modified. Script finds them
  by label CONTAINS match (labels may carry suffixes); errors loudly with the
  candidate list if any of the three is missing.
- CL5 RE-ENTRANT E. E on a terminal while the ceremony is already playing
  must be a no-op (apparition ignores Trigger unless idle) — no stacked
  ceremonies, no double input locks.
- CL6 ONE APPARITION PER MAP. Terminals do not spawn their own god — they
  find the placed AAIApparition and Trigger it with the right voice. Missing
  apparition = loud error, graceful no-op.
- CL7 CLUE-2 VOICE MAY NOT EXIST YET. Walt records it later; until then the
  post-slot ceremony runs silent (AP3 fallback, never a soft-lock). Import to
  /Game/AIApparition/S_ai_clue2 — already inside DirectoriesToAlwaysCook
  (PK16 lesson handled by geography).
- CL8 RETURN DOOR PLACEMENT. Spawned behind the cathedral PlayerStart, mesh +
  scale set by script (SM_Door_Cathedral_Huge, the attic-door look). Must sit
  far from the altar so it never contests the slot cabinet's focus trace.
- CL9 TRAVEL GUARD HOLDS. ACathedralDoor refuses travel while branched —
  correct in both directions; no new logic.
- CL10 PROMPT TEXT. ACathedralDoor's prompt was hardcoded "Enter the
  cathedral [E]" — promoted to an EditAnywhere FText so the return door can
  say "Return to the office [E]". Default unchanged (attic door unaffected).
- CL11 PACKAGED LOOP. Both maps already in MapsToCook; new sound rides
  DirectoriesToAlwaysCook; re-test the full loop in a packaged build before
  the next itch push (butler diffs make that cheap).

## Files
- Source/SibeliusGame/SibeliusProgressSubsystem.h/.cpp (new)
- Source/SibeliusGame/AIClueTerminal.h/.cpp (new)
- Source/SibeliusGame/AIApparition.h/.cpp (trigger mode)
- Source/SibeliusGame/SlotCabinet.cpp (sets bSlotPlayed)
- Source/SibeliusGame/CathedralDoor.h/.cpp (PromptText property)
- Tools/Scripts/build_clue_loop.py (terminals in office; door in cathedral)
