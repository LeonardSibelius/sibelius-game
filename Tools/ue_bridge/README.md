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

Write/act verbs (`set-actor-transform`, `set-property`, `place-actor`, `duplicate-actor`,
`delete-actor`, `run-pie`/`stop-pie`, `exec-console`, `screenshot`, `save-level`, `exec-python`)
are the next phase. `delete-actor` and `save-level`/`save-asset` will require an explicit flag —
Walt's hand-placed work is precious.

## Editor-vs-PIE world (baked in)
Reads resolve the **PIE/game world while playing**, else the **editor world**
(`UnrealEditorSubsystem.get_game_world()` vs `get_editor_world()`), so a falling book's live
transform reads the PIE instance, not the stale editor original. PIE transforms are single-moment
snapshots — call repeatedly to watch motion.
