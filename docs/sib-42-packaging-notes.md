# SIB-42 — Win64 packaged build (the ship-v0.1 milestone)

Goal: a packaged Windows build a stranger can download (itch.io now, Steam
later) and play start to finish: office bang → powers → cathedral → slot coda.

## What changed for packaging (June 11)

- **GameDefaultMap → L_Office_v02** (was the Lvl_FirstPerson template map).
  The shipped game opens on the AI apparition, as designed. Editor startup
  map unchanged.
- **SlotCabinet::ResolveWebGameURL()** — staged-first, dev-fallback:
  `Content/WebGame/index.html` if present, else the EditAnywhere dev URL.
- **DefaultGame.ini**: `DirectoriesToAlwaysStageAsNonUFS=(Path="WebGame")`
  (Chromium cannot read out of a .pak — the web game must ship as a loose
  file) + `MapsToCook` = L_Office_v02 + L_Cathedral only (keeps the Fab packs'
  demo maps out of the cook).
- `Content/WebGame/index.html` staged (copy of celestial-fortune
  `dist/index.html`, the 1.24 MB single-file build). RE-COPY AFTER ANY WEB
  GAME REBUILD — it's a snapshot, not a link.

## PK ledger — predicted before the first package

- PK1 CHROMIUM + PAK. The web game must be NonUFS-staged; a pak'd html is
  invisible to CEF. (Solved by design above.)
- PK2 MAP REFERENCES. L_Office_v02 references template Blueprints
  (BP_FirstPersonCharacter/GameMode) — cooked automatically by reference.
  If the packaged game opens on a black screen, check the cook log for the
  default-map line first.
- PK3 WEBBROWSER PLUGIN IN SHIPPING. WebBrowserWidget ships CEF subprocess
  binaries (UnrealCEFSubProcess.exe); verify they appear in the staged build.
  If the cabinet screen is black in the package but fine in PIE, this is the
  first suspect.
- PK4 FIRST-COOK WALL. First cook compiles every shader for SM6 — expect a
  LONG first package (possibly 1h+ on the 5070 Ti box) and warm caches after.
- PK5 DEBUG SCAFFOLDING IN SHIPPING. AddOnScreenDebugMessage prompts and
  [BUILD DBG] spam: Development packages show them, Shipping strips
  GEngine->AddOnScreenDebugMessage? (It does NOT strip automatically —
  audit before the public build; the HELP overlay and prompts must survive,
  the per-tick debug logs should go.)
- PK6 UNRELATED TEST MAPS. L_RefuserTest / L_CodeVisionTest / SymbolStudio
  maps stay uncooked via MapsToCook — confirm none are referenced by the
  shipped maps.
- PK7 SAVE DIR. Deploy saves write to Saved/SaveGames under the packaged
  build's user dir (AppData/Local/SibeliusGame) — works by default; just
  don't hardcode project paths anywhere new.
- PK8 PACKAGED ≠ PIE INPUT. Full-screen packaged build changes focus/cursor
  behavior; re-test the SC1 input transitions (E open, Esc close, Space spin)
  in the package specifically.
- PK9 MISSING COMMANDLETS OK. Smoke-test commandlets are editor-only; they
  don't stage. Fine — gates run pre-package, not post.
- PK10 STALE SNAPSHOT. The staged index.html will silently lag the web repo.
  Loose end: a pre-package checklist item (re-copy dist → Content/WebGame).
- PK21 (HIT, FIXED — supersedes PK20's approach; the REAL PK19/PK20 cure).
  PK20's staged CSVs shipped fine but the packaged log confessed: "[Generate]
  ... CSV fallback is editor-only" — UDataTable CSV PARSING DOES NOT EXIST in
  cooked builds. Loose CSVs can never work at runtime; the loaders require
  real DataTable ASSETS at /Game/Data. Fix: Tools/Scripts/
  import_generate_tables.py imports the 3 CSVs with their exact row structs
  (GenerateCatalogEntry / MrsHallLineRow / GenerateBlocklistRow; expected
  6/9/8 rows, verified by the script) + /Game/Data added to
  DirectoriesToAlwaysCook (runtime-loaded, nothing references them — PK16's
  trap pre-dodged). The PK20 staged-CSV plumbing stays (harmless; editor
  still reads CSVs; assets win when present). LESSON: the editor/runtime
  capability split bites a FOURTH way — not just files (PK17/20) and
  reflection (PK12), but whole ENGINE FEATURES (CSV import) vanish in
  Shipping; in-editor success proves nothing about a package until the
  packaged LOG says so. Diagnosis ritual: builds\...\SibeliusGame\Saved\Logs\
  SibeliusGame.log is the packaged game's testimony — copy and grep it FIRST.
- PK20 (HIT, FIXED — and it SOLVES PK19). Packaged Generate answered EVERY
  request "Absolutely not," still voiceless; PIE fine. Root: ALL Generate
  data is loose CSVs in <ProjectDir>/Data (GenerateCatalog, MrsHallLines —
  which carries the AudioKeys — MrsHallBlocklist), never staged → empty
  catalog fails closed to the harshest refusal with no AudioKey → silence.
  THE SAME ROOT AS PK19's "missing" voice: the clips were cooked fine; the
  LINES file that names them never shipped. Fix: staged-first path resolution
  in GenerateCatalog.cpp + MrsHallLines.cpp (×2), CSVs copied to
  Content/Data/, +DirectoriesToAlwaysStageAsNonUFS=(Path="Data").
  THE LAW (three strikes — WebGame, Journal, Data): ANY FFileHelper/CSV read
  from ProjectDir is a packaging bug waiting; audit grep `FPaths::ProjectDir`
  before every release.
- PK19 (HIT, OPEN — deferred to v0.2, Walt's call). Mrs. Hall's 9 voice clips
  (/Game/Audio/MrsHall, confirmed on disk, work in PIE) stay SILENT in the
  package even after +DirectoriesToAlwaysCook included them in a fresh cook.
  Playback is a soft LoadObject by path with LOAD_Quiet (GenerateComponent::
  PlayMrsHallClip) so failures are invisible by design. NEXT DIAGNOSTIC:
  smoke-test assert that loads all 9 keys the way the component does, and/or
  temporarily drop LOAD_Quiet in a dev package to see the real load error;
  also verify the wavs' .uassets aren't stale imports (asset name vs key).
- PK18 (HIT, FIXED). Packaged game CRASHED at runtime: assert
  Texture->LevelIndex != INDEX_NONE (TextureStreamingBuild.cpp:700) — the
  known-flaky per-level texture streaming metadata, likely tripped by the
  runtime-MID'd glyph cards. v0.1 fix: r.TextureStreaming=False in
  DefaultEngine.ini (16GB VRAM swallows the whole project; whole crash class
  gone). Revisit only for low-VRAM targets.
- PK16 (HIT, FIXED). PACKAGE PLAYED — slot machine worked (PK1+PK3 proven)
  but the AI intro was SILENT: the placed apparition's VoiceLine property was
  None; PIE healed it via runtime LoadObject, but the cooker ships only
  REFERENCED assets → S_ai_intro never staged → AP3 silent-fallback (worked
  as designed, no hang). Fix: assign Voice Line on the placed actor (by hand
  + script now does it on spawn) + DirectoriesToAlwaysCook for /Game/
  AIApparition + SymbolSprites + SlotFactory/Materials (everything we
  LoadObject at runtime). LESSON: every runtime LoadObject path needs a
  cooked-reference story.
- PK17 (HIT, FIXED). Journal (J) reads docs/NARRATIVE.md from the project
  dir — editor-only, not staged; packaged build showed the (gracefully
  written) "Journal unavailable" screen. Fix: staged-first/dev-fallback in
  RefreshFromNarrative (Content/Journal/NARRATIVE.md → docs/), NonUFS dir
  "Journal", staged copy created. NOTE: like the web game, the staged copy
  is a SNAPSHOT — re-copy when NARRATIVE.md changes (pre-package checklist
  with PK10).
- PK15 (HIT, FIXED). Fourth package: cook ran 6m47s, said "Done!", then
  FAILED with the only Error in the log: UnrealClaude's MCP server couldn't
  bind 127.0.0.1:3000 (the open editor holds it; the cook is a second editor
  instance) — and ANY logged Error fails a cook. Fix: in the plugin's
  StartupModule, skip StartMCPServer() when IsRunningCommandlet(). LESSON:
  a cook fails on any LogXxx: Error: anywhere — grep the Cook-*.txt, the
  last error before exit is rarely the cause; the ONLY error is.
- PK14 (HIT, FIXED). 'SibeliusGameEditor' module "could not be loaded" at
  editor start despite a clean link — GetLastError=4551 = Windows SMART APP
  CONTROL blocking the fresh unsigned DLL (also retro-explains the morning's
  'SibeliusGame' load failure that a rebuild "cured"). Fix: Windows Security
  → App & browser control → Smart App Control → Off (one-way switch; no
  whitelist exists; Defender AV unaffected). Dev machines must have it off.
- PK13 (HIT, FIXED). Editor-module link errors: IInteractable's Execute_
  thunks needed SIBELIUSGAME_API export (same-module code never needed it);
  commandlets also directly reference EnhancedInput/NavigationSystem/AIModule
  classes → added to SibeliusGameEditor deps. LESSON: extracting code into a
  module surfaces every implicit same-module assumption as a linker error.
- PK12 (HIT, FIXED). Second package died in the commandlets' UHT .gen.cpp:
  the nine smoke-test commandlets lived in the RUNTIME module with their
  classes inside `#if WITH_EDITOR` — but UHT generates reflection
  unconditionally, so the first-ever game-target build couldn't see the
  classes. Fix: new editor-only module `SibeliusGameEditor` (Build.cs +
  IMPLEMENT_MODULE boilerplate), all 9 commandlet pairs moved in, added to
  SibeliusGameEditor.Target.cs ExtraModuleNames + .uproject Modules
  (Type=Editor). Gate invocations (-run=...) unchanged — commandlet class
  names didn't move namespaces. LESSON: editor-only UCLASSes belong in
  editor-only modules; WITH_EDITOR guards around a UCLASS are a trap.
- PK11 (HIT, FIXED). First package died in UBT compiling the GAME target:
  error C2971 'TAtArgPos' / FormatStringSan in checkf() inside engine
  MovieScene headers (MSVC 14.44 + C++20; editor target unaffected). Fix:
  `bValidateFormatStrings = false;` in SibeliusGame.Build.cs (dev-time lint
  only). Diagnosis path that worked: copy the newest
  %APPDATA%\Unreal Engine\AutomationTool\Logs\...\Log.txt into the repo and
  grep it — the editor's "Packaging failed!" toast tells you nothing.

## The runbook (first package)

1. Close editor → Build.bat (ResolveWebGameURL change).
2. Reopen → verify Edit→Project Settings→Maps & Modes shows Game Default
   Map = L_Office_v02 and Packaging shows the NonUFS dir + maps list.
3. Platforms (toolbar) → Windows → Binary Configuration: **Development** for
   the first test package (Shipping for the public itch build later).
4. Platforms → Windows → **Package Project** → pick an output folder
   (suggest C:\Users\wpark\builds\sibelius-v01-dev).
5. Wait out PK4. Then run SibeliusGame.exe from the output folder and play
   the full loop: bang → office → attic → cathedral → E → spin → Esc.

## Release runbook (recurring — butler push to itch)

Recorded from the v0.5.1 push (2026-07) so it's not a scavenger hunt next time:

1. Bump `Config/DefaultGame.ini` → `ProjectVersion=<x.y.z>`.
2. Package: `Tools/Scripts/package_v051.ps1` (copy per version; edits the
   archivedirectory to `C:\Users\wpark\builds\sibelius-v<x.y.z>` + log path).
   Run: `powershell -ExecutionPolicy Bypass -File ...\package_v0xx.ps1`.
   Success = `EXITCODE=0` and `builds\sibelius-v<x.y.z>\Windows\SibeliusGame.exe`.
3. **butler lives at `C:\Users\wpark\butler\butler.exe`** (NOT on PATH — call by
   full path). itch target = **`leonardsibelius/leonard-sibelius:windows`**.
   - Status:  `& "C:\Users\wpark\butler\butler.exe" status leonardsibelius/leonard-sibelius:windows`
   - Push:    `& "C:\Users\wpark\butler\butler.exe" push "C:\Users\wpark\builds\sibelius-v<x.y.z>\Windows" leonardsibelius/leonard-sibelius:windows --userversion <x.y.z>`
   - butler block-diffs vs the last build → an 8.9 GB build uploads as a few MB.
4. Verify version on https://leonardsibelius.itch.io/leonard-sibelius after it
   finishes processing.

NOTE (v0.5.1): shipped maps are `MapsToCook` only (L_Office_v02, L_Cathedral,
L_AI_Temple, L_Poplar_Forest). The Elsewhere anchors work lives in the dev
sandbox **L_Elsewhere_Dev**, which is NOT cooked — so 0.5.1's playable content
matches 0.5.0. To ship the forest, promote it into a cooked map / MapsToCook.
