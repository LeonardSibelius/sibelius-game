# The Dragon's Wager — continuation handoff

Checkpoint: October 28, 2026 — third session (Claude Code, continuing GPT-6 Astra's two) complete.
Project: C:\Users\wpark\projects\sibelius-game
Map: /Game/Maps/L_IslaKeep, Isla's home through the attic portal.

## Current status

Current source compiles and links into the editor DLL; the standalone rules tests pass. This session
played the CURRENT build in normal PIE with the possessed first-person character walking real routes
(CharacterMovement, stairs, collision) and pressing E through the real interactor sweep: start, jewels,
crown, decoy, return, bank, raise, forfeit, catch/gold/respawn, and a save reloaded in a fresh editor
session all work. It fixed what play turned up (below). It is still a development checkpoint: no human
has played it, nothing is committed, packaged, uploaded or published.

Editor state at handoff: Unreal Editor left OPEN on L_IslaKeep with PIE stopped and nothing dirty, so
Walt can press Play. Live Coding was never used; every code change was an editor-closed build.

## Agreed game

The Dragon's Wager complements the lavish keep. Steal the crown above the staircase and bring it back while two dragons sweep amber gazes across the hall. Stone architecture provides cover. Optional jewels increase the purse but chime and draw attention. Spend a jewel to create a decoy. Return and bank the purse, or wager everything on another harder round. Getting caught loses unbanked treasure, briefly turns the player to gold, and restarts. Isla responds with dry remarks. Gold and jewel-colored effects should fit the room.

## Implementation now on disk

- Runtime ADragonsWager spawned once by AVirtualHome::BeginPlay only in L_IslaKeep; no map or vendor assets modified.
- Normal first-person/on-foot movement retained. Starts require a Character, not the simulation spectator.
- E at the pedestal starts play or returns the crown; crown and loose jewels are proximity pickups with visibility checks.
- K creates a decoy, B banks, T raises the wager, X forfeits. O retains the existing attic-return behavior. None of K/B/T/X is bound by the character.
- Ten rounds maximum. Purse stays at risk across rounds. Decoys consume the exact value of the jewel spent, including jewels carried from earlier rounds.
- Gaze cone tests both pawn center and eyes, with actual Visibility traces. Queries ignore only the originating dragon itself, allowing architecture to block them.
- The gaze is SHOWN with the same traces: 20 amber marks where the edge of each 19-degree cone lands (a ring draped over floor, stairs and walls), a sparse dotted line from the eye, and a glow where the centre lands. The amber spotlights (30000 unitless, unshadowed) only warm the area.
- Opening grace period, exposure warning and recovery behind cover, chimes, catch/retry, and balanced input cleanup.
- Capture: player meshes turn gold (snapshot/restore), the view fades to a gold tint for the 1.5 s freeze (first person never sees its own body), small amber sparkles. Tint and materials are restored on respawn and on any stop.
- Respawn after a catch teleports to where the player started; if that spot is occupied (Isla wanders the hall) it falls back to a PlayerStart, and only ends the wager if both fail.
- Camera/menu checks suspend gameplay and guard game input callbacks.
- Messages: during a wager Isla's lines go to the status panel only; when the wager is not running they are toasts. Every line is also logged as `LogSibeliusGame: [DragonsWager] ...`.
- Plaque: idle "THE DRAGON'S WAGER / [E] Steal the crown / Best: N treasure, round R" (or "No treasure banked yet"); during play "Round N of 10". One emerald per 100 banked rings the pedestal (max 8).
- Best score/round in a separate DragonsWager save slot; write failure reported.
- Normal on-foot HUD draws the leased minigame status panel.
- Original crown and faceted-jewel meshes, metallic gold/jewel materials, the keep's existing red-and-gold pedestal, restrained decorations, an objective marker.
- Read-only GetWagerState() Blueprint/Python function returns JSON: phase, round, purse, jewels, crown, exposure, watchOwners, best, frozen.
- Dev console command `sib.Wager.Key K|B|T|X` presses a wager key through the same handlers and gates as the keyboard (finds the PIE world from the editor console, like sib.Lantern.*).

## Changed files — include all of these when transferring or committing

New source:
- Source/SibeliusGame/DragonsWager.cpp
- Source/SibeliusGame/DragonsWager.h
- Source/SibeliusGame/DragonsWagerRules.h
- Source/SibeliusGame/DragonsWagerSaveGame.h

Modified shared source:
- Source/SibeliusGame/VirtualHome.cpp: includes DragonsWager.h and calls EnsureInKeep(GetWorld()) in BeginPlay.
- Source/SibeliusGame/SibeliusHUD.cpp: DrawTablePanel() also called in the ordinary on-foot draw path; existing cinematic path retained.

New assets (saved, untracked, NOT ignored):
- Content/DragonsWager/M_WagerGold.uasset (now saved with bUsedWithSkeletalMesh — see below)
- Content/DragonsWager/M_WagerGem.uasset
- Content/DragonsWager/SM_WagerCrown.uasset
- Content/DragonsWager/SM_WagerGem.uasset

New tooling and documentation:
- Tools/Scripts/make_dragons_wager_assets.py
- Tools/Scripts/inspect_dragons_wager.py
- Tools/Scripts/dragons_wager_cover_map.py (new this session)
- Tools/Tests/dragons_wager_test.cpp
- Tools/Tests/run_dragons_wager_tests.ps1
- docs/DRAGONS_WAGER_HANDOFF.md

The asset builder is reproducible, creates only missing assets, and runs in Unreal with PIE stopped. It generates OBJ sources under Saved/DragonsWagerAssetSource and imports them. Do not rebuild a loaded material in place. Mesh bounds were checked after import: crown 60 x 60 x 35 cm, gem 34 x 34 x 28 cm, both correctly Z-up. It now also sets bUsedWithSkeletalMesh on M_WagerGold (and adds it to an existing asset without rebuilding it): the engine had been setting that flag on the fly in the editor when the gold touched the player's skeletal meshes, which a cooked build cannot do — the caught player would have drawn the default material.

Hard references also use existing /Game/Fafnir_Keep_Meshingun/Meshes/SM_pedestal and /Game/LanternBurst/M_LanternOrb. The first is part of the keep (gitignored vendor pack, like the map — back up to E:). The second (committed) supplies the glowing marks; it is not modified.

## Session 3 findings and fixes

1. **The gaze was invisible.** The spotlights were 1800 *unitless* and cast shadows from an origin inside the dragons' heads, so the heads swallowed them; the 5 cm motes along the beam could not be seen. A player could not see the danger before being caught. Tried: 250000 unshadowed (blew the dragons out to white) and an additive cone mesh (invisible from outside in the bright hall, a full-screen haze from inside; removed, its material deleted). Shipped: the traced ring + dotted line + landing glow, lights at 30000 unshadowed. A still frame from the foot of the stairs shows the ring as an arc of marks across the carpet.
2. **Gold material flag** — see above. Fixed and saved.
3. **Catch invisible in first person** — gold tint added; sparkles shrunk (9 cm orbs 65 cm from the camera filled the view with flat yellow discs).
4. **Every Isla line appeared twice** (panel + toast) and the plaque still said "[E] Steal the crown" mid-run and "Round 0" on a fresh record. Fixed.
5. **Once, the wager dropped to idle while the player stood at the start spot unattended.** Not reproduced in a 45 s stand-still test (caught once, respawned normally). Most likely the occupied-start fallback with Isla standing there; that path now tries a PlayerStart first and every wager line is logged, so the next occurrence will say what happened.
6. **Cover barely exists on the route.** Saved/DragonsWager-cover-map.txt (from dragons_wager_cover_map.py): every floor cell from the foot of the stairs to the crown landing is visible to at least one dragon; the only cells hidden from both are a few corners near the entrance. "Stone blocks their gaze" is true of pillars at the edges, not of the route. The game is about timing the sweeps, not hiding. A design choice for Walt (options: let each dragon's own body block its own gaze, which turns the stairs into cover from the near dragon; or narrower/slower sweeps).
7. **Round 1 is forgiving.** A blind bot on the jewel-free centre line (never watching the dragons) cleared rounds 1-3 back to back and banked 600. Peak exposure: round 1 0.23 of 1.65 s; round 2 1.29 of 1.57; round 3 1.38 of 1.49. One sample. The 3 s grace covers most of the ~3.5 s climb if the player sets off at once. Jewels are the real risk: their chime turns both dragons onto the player (run 1 was caught on the way back after two jewels).

## Verification — do not overstate it

PASSED on the current build (normal PIE, possessed BP_FirstPersonCharacter_C; movement by AddMovementInput through CharacterMovement; E by DoInteract, the function the E binding calls, through the interactor's camera sweep):
- Editor-closed build: Result: Succeeded, no warnings (log Saved/DragonsWager-build.log). Rules tests pass at /W4 /WX. git diff --check clean on the two shared files.
- watchOwners = 2 at BeginPlay and at Start.
- Full round walked: pedestal E -> stairs -> 2 jewels -> crown -> K decoy (purse 150 -> 125, jewels 2 -> 1) -> walk back -> E return -> B bank 125 -> save written. A new editor session then loaded best 125 from disk.
- Caught: CharacterMesh0 and First Person Mesh turn M_WagerGold, originals (MI_Manny_01_New/02_New) restored after 1.5 s, respawn at the start with exposure 0; several catches in a row; gold tint seen mid-freeze and gone after respawn.
- T raises to round 2 with the purse carried; K with no jewels is refused; X forfeits to idle with the banked best kept.
- Status panel, objective marker, plaque states, toasts-only-when-idle, log lines: all seen.
- K/B/T/X were driven through `sib.Wager.Key` (same handlers and CanPlayInput gate). The keyboard binding itself was verified by Astra for B in session 2 and is unchanged.
- Test saves: this session's records (best 600 from bot runs) moved to Saved/DragonsWagerTestArtifacts/BotRunsTest.sav beside Astra's BankingTest.sav. The DragonsWager slot is clean. Other slots untouched.

NOT YET VERIFIED:
- A human playing it: whether the ring reads in motion, whether timing the sweeps is fair and fun, difficulty above round 3.
- Physical K/B/T/X key presses in this build; O to the attic during a wager; alternative player forms (BattleForm).
- The PlayerStart respawn fallback (not forced); the disk-write-failure line.
- Frame rate while hunting (about 50 line traces and ~120 instances rebuilt per tick).
- Cooked/packaged build (the gold-flag fix is for it but has not been cooked). Nothing has been released.

## Known review points and finishing work

1. Have Walt play it (editor is open; Play, walk to the pedestal on the right of the arrival, E). Ask: can you see where the dragons look? Is round 1 too easy?
2. Decide the cover question (finding 6) and whether to tune grace / sweep / catch time (finding 7). Rules live in DragonsWagerRules.h (tests cover them); sweep and gaze in ADragonsWager::UpdateWatch.
3. If the wager ever drops to idle on its own again, grep the log for `[DragonsWager]`.
4. Small instance arrays are rebuilt each active tick. Profile before optimizing.
5. Authored Isla responses are text only. No new recorded voice or dragon animations.
6. Screenshots 00176-00193 in Saved/Screenshots/WindowsEditor are this session's; 00186 (crown close-up), 00190 (ring on the stairs, plaque, record emeralds) and 00192 (gold tint) show the current look. 00176-00177 and 00180-00181 show the rejected bright-light and cone attempts.

## Scene measurements

Saved/DragonsWager-layout.json is the earlier editor survey from inspect_dragons_wager.py; Saved/DragonsWager-cover-map.txt is this session's line-of-sight map.
- PlayerStart_IslaKeep: (15,-700,92), facing +Y. Actual possessed capsule center settled near Z=98.15.
- Isla feet: (15,-250,0) — she wanders and dances around the hall.
- Pedestal: (-180,-380,0), visible on arrival (to the right), E works from the start spot (-40,-560).
- Landing floor at (0,1150): Z=500.37. Crown default: (15,1150,650).
- Main stairs bounds center (0,529.02,309.64), extents (364.82,492.32,306.99).
- Dragon_01 (-306.75,50,275.38); Dragon_2 (315.48,40,274.07); SM_Dragon_a_01.
- Gaze sources (-310,-205,460), (315,-215,460).
- Jewels (-600,-50,90), (600,-50,90), (-180,350,304), (180,550,402), (-150,750,501), (150,950,590): approximately 90 cm above measured surfaces. A line along x=15 avoids them all.
- Walking speed ~600 cm/s: the climb from the pedestal to the crown takes about 3.5 s.
- Mirror around Y=1767; Fafnir's Keep is World Partition.

## Reliable build/test commands

Run from the project with Unreal Editor/Live Coding CLOSED.

```powershell
New-Item -ItemType Directory -Force Saved/WagerBuildTemp | Out-Null
$env:TEMP = 'C:\Users\wpark\projects\sibelius-game\Saved\WagerBuildTemp'
$env:TMP = 'C:\Users\wpark\projects\sibelius-game\Saved\WagerBuildTemp'
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' SibeliusGameEditor Win64 Development '-Project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject' -WaitMutex -FromMsBuild -NoUBTMakefiles > Saved/DragonsWager-build.log 2>&1
Get-Content Saved/DragonsWager-build.log -Tail 20
& Tools/Tests/run_dragons_wager_tests.ps1
```

Astra's sandbox also needed -NoUBA -NoUBALocal -DisableDebugInfoModules=SibeliusGame (UBA could not write ProgramData; D8050 from debug records). Outside that sandbox the command above builds normally with UBA.

## Editor/bridge continuation

Read Tools/ue_bridge/README.md. Python Remote Execution (ue_bridge) runs without the editor's "Execute PYTHON Script?" prompt that UnrealClaude's execute_script raises; use UnrealClaude's open_level tool to load the map. Close the editor cleanly with `ue_bridge exec-python "import unreal; unreal.SystemLibrary.quit_editor()"` after checking no packages are dirty. Do not run bridge commands during build/cook/Live Coding.

Normal PIE (the run-pie wrapper instead starts SIMULATE):
```powershell
Tools\ue_bridge\ue_bridge.cmd exec-python "import unreal; unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()"
```

After PIE starts:
```python
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
p = unreal.GameplayStatics.get_player_pawn(w, 0)
g = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DragonsWager)[0]
print(g.get_wager_state())
p.do_interact()                                                     # a real E press
unreal.SystemLibrary.execute_console_command(w, "sib.Wager.Key B")   # K, B, T or X
```

To walk, register a Slate post-tick callback that calls `pawn.add_movement_input(direction, 1.0)` each frame toward waypoints and `set_control_rotation` on the controller. Running the editor in the background throttles it to ~3 fps; for the session set EditorPerformanceSettings `bThrottleCPUWhenNotForeground` False on the CDO (not saved). `GameplayStatics.set_global_time_dilation` slows the game for photographs; restore 1.0 afterwards. Stop via editor_request_end_play(). The bridge `screenshot` verb captures the PIE viewport.

## Preserve unrelated work

Pre-existing and untouched:
- Modified Content/Environments/Heightmaps/HM_Meadow.uasset
- Untracked AI-City-Handoff-Plan.md
- Untracked AncientTreasures.png
- Untracked SourceArt/SlotCabinet/SM_CelestialCabinet.blend1

Do not revert/delete/stage those as part of this feature. Read repository instructions. The retired forest feature is unrelated; obey docs/FORESTS_DO_NOT_USE.md.

## Handoff copies and checkpoint archive

Authoritative project copy: docs/DRAGONS_WAGER_HANDOFF.md (this file).
C:\Users\wpark\Documents\Codex\2026-10-27\i\outputs\ holds Astra's session-2 copy of this handoff and DRAGONS_WAGER_CHECKPOINT.zip. Both are now OLDER than the project files (they predate every session-3 change above); treat them as reference only and never restore them over this checkout.

## Ready-to-paste continuation prompt

Continue The Dragon's Wager in C:\Users\wpark\projects\sibelius-game. Read docs/DRAGONS_WAGER_HANDOFF.md first. The current build compiles, the rules tests pass, and a scripted player has played every mechanic in normal PIE; the gaze is now drawn as a traced ring. Next: get Walt's verdict from a hands-on play, then settle the cover question and round-1 difficulty, verify physical K/B/T/X keys and O-travel, profile, and cook. Use editor-closed native builds, preserve unrelated work, update the handoff, and do not commit or publish without Walt's go-ahead.
