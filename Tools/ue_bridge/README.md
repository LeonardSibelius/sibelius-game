# ue_bridge — Cowork/Code ↔ live Unreal Editor bridge (SIB-45)

Runs `unreal.*` Python inside the **running editor** over UE's first-party Python Remote Execution
and returns JSON, so Claude Code can read/manipulate the live level directly instead of relaying
through Walt ("go read me BookRain's Z and come back").

## ⚠️ EDITOR-OPEN ONLY — never collides with the build hook
This is the **editor-open** loop. It complements, never replaces, the **editor-closed** pipeline
(`Build.bat`, cook, smoke-test gates). **Never run the bridge while a cook/build/Live-Coding session
has the project** (a second process mid-cook fails — PK15).

No port collision with the port-3000 build hook:
- **UnrealClaude MCP** = TCP **3000** (`UnrealClaudeConstants.h: DefaultPort=3000`).
- **Python Remote Execution** = UDP multicast **239.0.0.1:6766** + a loopback command port — disjoint.
- Both are editor-*open* services (coexist on different ports). By the time a cook runs, the editor
  and both listeners are gone, so the bridge can't be active during a build.

## One-time enable (already committed)
- `.uproject`: `PythonScriptPlugin` + `EditorScriptingUtilities` enabled, **Editor-only**
  (`TargetAllowList: ["Editor"]`) so they never ship in the cooked game.
- `Config/DefaultEngine.ini`:
  ```
  [/Script/PythonScriptPlugin.PythonScriptPluginSettings]
  bRemoteExecution=True
  RemoteExecutionMulticastBindAddress=127.0.0.1
  RemoteExecutionMulticastGroupEndpoint=239.0.0.1:6766
  ```
- **Restart the editor** after enabling. Confirm the editor log shows Python Remote Execution
  listening on startup. If discovery fails, allow `UnrealEditor.exe` through Windows Firewall
  (the loopback bind above usually avoids the multicast trap).

## Run it
No standalone Python required — use the engine's bundled interpreter via the wrapper:
```
Tools\ue_bridge\ue_bridge.cmd <verb> ...
```
or directly:
```
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" Tools\ue_bridge\ue_bridge.py <verb> ...
```
Every verb prints a JSON object (parseable + readable).

## Verbs (v1 = READ)
- `list-actors [--filter SUBSTR]` — actors in the live world (`{label, class, path}`).
- `get-transform LABEL` — **world AND relative** location/rotation/scale (relative-vs-world baked
  in; June 15 lesson).
- `get-property LABEL PROP` — read an actor UPROPERTY (e.g. `BookRain ScaleRange`).
- `get-component-property LABEL COMPONENT PROP` — read a component UPROPERTY.
- `read-log [--lines N] [--grep STR]` — tail `Saved/Logs/SibeliusGame.log` (pure file read; works
  mid-PIE). e.g. `read-log --grep "[BookRain]"`.

### WRITE (target the EDITOR world / persistent placement)
- `set-actor-transform LABEL [--location x,y,z] [--rotation pitch,yaw,roll] [--scale x,y,z]`.
- `set-property LABEL PROP VALUE` — coerced to the property's existing type; supports array
  elements, e.g. `set-property BookRain "SourceLocations[0]" "(0,0,750)"`.
- `set-component-property LABEL COMPONENT PROP VALUE`.
- `place-actor CLASS_PATH [--location] [--rotation] [--scale] [--label]` — e.g.
  `place-actor /Script/Engine.PointLight --location 0,0,200`.
- `duplicate-actor LABEL`.
- `delete-actor LABEL --confirm` — **requires `--confirm`**.

### ACT
- `run-pie` / `stop-pie` (simulate via `LevelEditorSubsystem`).
- `exec-console COMMAND` — any console command.
- `screenshot [--res WxH] [--timeout S]` — fires `HighResShot`, polls `Saved/Screenshots/`, returns
  the new PNG path (async write handled). **Closes the visual loop** — the PNG can then be viewed.
- `save-level --confirm` / `save-asset PATH --confirm` — **require `--confirm`** (don't clobber
  hand-work).
- `exec-python "SNIPPET"` — raw escape hatch.

All verbs verified live against `L_AI_Temple` (the full book-rain session — read transforms, set
ScaleRange/SourceLocations, run sim, scrape `[BookRain]` log, screenshot the falling books, stop —
reproduced with no human relay). Writes use the editor world, so they take effect on the next PIE.

## Editor-vs-PIE world (baked in)
Reads resolve the **PIE/game world while playing**, else the **editor world**
(`UnrealEditorSubsystem.get_game_world()` vs `get_editor_world()`), so a falling book's live
transform reads the PIE instance, not the stale editor original. PIE transforms are single-moment
snapshots — call repeatedly to watch motion.
