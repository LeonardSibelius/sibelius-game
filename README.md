# Sibelius Game

> Walt Parkman becomes the three-part entity **Leonard Sibelius** — composed of himself, Claude Cowork, and Claude Code.

**v0.1 one-room MVP** — a 60-second walkable third-person scene in Walt's home office that teaches the **Code Vision** mechanic: hold a key to reveal a hidden door behind a bookshelf, walk through it, and reach an "End of Chapter 1" card.

Full specification: [`docs/mvp-spec.md`](docs/mvp-spec.md). Linear: **SIB-1**.

---

## Status

| | |
|---|---|
| Engine | Unreal Engine **5.7** (macOS, Apple Silicon / M2 Pro) |
| Branch | `feat/v0.1-mvp` |
| Current checkpoint | **2 — office room (grey-box)** ✅ (this commit) |

### Build approach: Blueprint-first

The spec describes a C++ project. This MVP ships **Blueprint-first**: a Blueprint Third
Person project that boots in the editor with no compile step. The Code Vision and door
mechanics are authored in Blueprint and can be ported to the C++ structure in `Source/`
later. **Full Xcode 26.5 + the Metal Toolchain component are now installed**, so the
`CompileAllBlueprints` smoke test runs with zero `LogMetalCompilerSetup` warnings and the
`Package → Mac` acceptance criterion is unblocked. (In Xcode 26 the Metal compiler is a
separate download: `xcodebuild -downloadComponent MetalToolchain`.)

### Checkpoint 2 — what shipped

The office is a **grey-box blockout** built from engine primitives by
[`Tools/build_office_blockout.py`](Tools/build_office_blockout.py), saved to
`Content/Maps/L_Office_MVP`: room shell with a window opening, desk, chair, dual
monitors, bookshelf, and the **hidden door** embedded in the wall behind the bookshelf.
Golden-hour directional sun + sky light + atmosphere + height fog + an unbound
post-process volume; Lumen GI is enabled project-wide in `DefaultEngine.ini`. The hidden
door and bookshelf carry `CodeVision.*` tags and custom-depth stencil values so
Checkpoint 3's reveal mechanic has real targets.

**Megascans art is deferred, not skipped.** Free Megascans-via-Fab ended 2024-12-31, so
photoreal props need a sourcing decision (Fab purchase, the legacy Quixel Bridge plugin
while it lasts, or another library). The blockout is dimensioned to the real layout, so
art swaps in without repositioning. MetaHuman Leonard is also deferred — the ThirdPerson
mannequin remains the placeholder until Leonard is authored (interactive, Epic login).

---

## Getting started

### Prerequisites
- **Unreal Engine 5.7** (installed at `/Users/Shared/Epic Games/UE_5.7`)
- **Git LFS** — all `.uasset`/`.umap` binaries are stored via LFS. Run once after cloning:
  ```sh
  git lfs install
  git lfs pull
  ```
- **Full Xcode** — *not* needed to open/run this Blueprint project, but required to
  package a macOS build and to compile the future C++ module.

### Open the project
Double-click `SibeliusGame.uproject`, or:
```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  /Users/boss/projects/sibelius-game/SibeliusGame.uproject
```
Press **Play** — you spawn as the third-person mannequin (Leonard placeholder) and can
walk with **WASD** + mouse look in the template prototyping level.

---

## Layout

```
sibelius-game/
├── SibeliusGame.uproject     # Blueprint project (EngineAssociation 5.7)
├── Config/                   # DefaultEngine/Game/Input/Editor .ini
├── Content/
│   ├── ThirdPerson/          # Template level (Lvl_ThirdPerson), gamemode, character BPs
│   ├── Characters/           # Mannequin meshes/anims  (Leonard/ = MetaHuman, later)
│   ├── Input/                # Enhanced Input mappings (IMC_Default, IA_*)
│   ├── LevelPrototyping/     # Greybox geometry + materials
│   ├── Environment/Office/   # The office room                (Checkpoint 2)
│   ├── Mechanics/CodeVision/ # Code Vision post-process + components (Checkpoint 3)
│   ├── UI/                   # Interaction prompts, end-of-demo card (Checkpoints 4–5)
│   └── Maps/                 # L_Office_MVP                   (Checkpoint 2)
└── docs/
    ├── mvp-spec.md           # Source-of-truth spec
    ├── design-doc-v0.md      # Placeholder
    └── screenshots/
```

The current playable level is the template's `Content/ThirdPerson/Lvl_ThirdPerson`.
The office (`Maps/L_Office_MVP`) is built in Checkpoint 2.

## Checkpoints

1. **scaffold** — Unreal project + third-person template ✅
2. env — office room with Megascans assets
3. mechanic — Code Vision post-process + hidden-object component
4. interaction — door F-prompt + animation
5. ui — end-of-demo screen
6. polish — lighting, color grading, audio
7. docs — README with screenshots
8. ship — tag v0.1.0
