# Sibelius Game — One-Room MVP Spec

*Source-of-truth specification for the validation prototype. Used by Claude Code to build the v0.1 one-room demo. Maximum polish, maximum visual quality. Build budget: 6-8 hours.*

**Project root**: `~/projects/sibelius-game/`
**GitHub**: `github.com/LeonardSibelius/sibelius-game` (public from day 1)
**Engine**: Unreal Engine 5.5+ (latest at time of build)
**Target platform**: macOS (Apple Silicon, M2 Pro)
**Linear branch**: `feat/v0.1-mvp` on Linear ticket **SIB-1**

---

## The pitch in one sentence

A 60-second walkable third-person scene set in Walt's home office, where the player learns the **Code Vision** mechanic by holding a key to reveal a hidden door behind a bookshelf, walks through it, and sees an "End of demo — Chapter 1: What I Have Stopped Seeing" card.

That's the entire MVP. Six to eight hours of work. If this is fun to build and fun to play, the game project is real and scopes up to v1. If it's not fun, we know cheaply.

---

## Why this MVP, not something bigger

Every indie game that fails dies of scope creep at the prototype stage. The temptation is to build "just a little more" before validating. We resist that here. **The single question this MVP answers**: does the Mac mini M2 Pro + Claude Code + Unreal Engine 5 + Megascans pipeline produce a playable polished scene in one focused session? If yes, the rest is more of the same. If no, we learn what's broken before committing months.

---

## The scene

**Setting**: Walt's home office. Same room as the Lennie video (the one currently embedded on leonardsibelius.com). Desert mountains visible through a large window. Dual monitors on a desk showing faintly visible code. A leather chair. A bookshelf against the wall opposite the window.

**Hidden detail**: There is a small wooden door behind the bookshelf, only visible when the player activates Code Vision. The door leads to "the next chapter" (in MVP, just a fade-to-black end-screen).

**Time of day**: Golden hour — warm sunlight streaming through the window, long shadows, atmospheric.

---

## The character

**Identity**: Leonard — a 71-year-old grey-haired distinguished man. Approximates Walt Parkman's appearance. Build via **MetaHuman Creator** (free, integrated with Unreal). Reference: the character in `Lennie.mp4` already on leonardsibelius.com.

**Clothing**: Light blue button-up shirt, dark trousers. Simple and professional. No need for elaborate costuming at MVP stage.

**Animations**: Use Unreal's free **MetaHuman Animator** sample rig and Mixamo's free animation library for walking, idle, and the door-open interaction.

**Camera**: Third-person over-the-shoulder. Distance ~2 meters behind character, height slightly above shoulder. Allow mouse look to orbit. **NOT first-person** — we want the player to see Leonard so chapter 7's transformation is visually meaningful.

---

## The Code Vision mechanic

This is the heart of the MVP. Get this one mechanic right and the rest is decoration.

**Trigger**: Player holds the **E key** (configurable).

**Effect**: While held, a translucent green/cyan post-process overlay appears across the entire screen. Hidden objects (those tagged `bHiddenWithoutCodeVision = true`) become visible. Already-visible objects gain a faint code-text overlay on their surface, suggesting their "true name" or "type."

**Release**: Player releases E. Overlay fades over 0.3 seconds. Hidden objects revert to invisible — *except* objects the player has already interacted with at least once. **Once a hidden object has been revealed, it stays visible** even without Code Vision active. This rewards mechanic use without making it tedious.

**Visual design**:
- Post-process material with a green/cyan color tint, slight chromatic aberration, and a subtle scanline effect
- Floating text labels on objects (rendered via UMG widgets in 3D space) showing terms like `Wall_Drywall`, `Chair_Office`, `Bookshelf_Wood` — like exposed type metadata
- The hidden door, when revealed, shows the label `Door_Hidden_Ch1Exit`
- A subtle electronic chime sound on activation, a fade-out on release

**Implementation hint for Claude Code**: Use a `PostProcessVolume` with a custom material, blended in at 100% intensity when Code Vision is active, faded to 0% when inactive. Hidden objects use a custom actor component `UCodeVisionComponent` that toggles visibility based on a global game-mode flag.

---

## The hidden door

**Location**: Wall behind the bookshelf. Bookshelf sits against the wall; without Code Vision, the door is hidden behind it. With Code Vision, the door is visible through/around the bookshelf as if the bookshelf has become semi-transparent.

**Interaction**:
- When Leonard walks within 2 meters of the door (with Code Vision having revealed it at least once), an interaction prompt appears: **"Press F to open"**
- Pressing F triggers a door-open animation (~1 second)
- The screen fades to black over 1.5 seconds
- End-of-demo card appears

**End-of-demo card** (full-screen UMG widget):
- Background: black with faint platinum-gold particle effect
- Centered text:
  > **End of Chapter 1**
  > *What I Have Stopped Seeing*
- Subtitle text below:
  > Leonard Sibelius — One-Room Prototype, v0.1
- "Continue" button (does nothing — just stays on screen until player closes window)

---

## Audio

Keep audio minimal but present. Maximum polish doesn't mean maximum content; it means every element has been thought through.

- **Ambient**: subtle desert wind through window, low-volume room tone (Pixabay free SFX or Freesound)
- **Whisper line** (optional, recommended): At game start, after ~3 seconds of Leonard standing idle, play a faint ElevenLabs-generated voice line in the platinum-Cowork voice (female-coded, calm, intelligent):
  > *"There's another way to see this..."*
- **Code Vision activation**: subtle electronic chime, ~0.4 seconds
- **Code Vision deactivation**: subtle inverse chime
- **Door open**: classic wooden door creak (free SFX library)
- **No music** for MVP — silence is appropriate for the intro chapter feel. Music comes later.

---

## Visual polish targets

The "maximum polish" choice means these are non-negotiable for v0.1:

1. **Lumen** for real-time global illumination (set in Project Settings → Rendering)
2. **Nanite** for high-poly meshes (enable on imported Megascans assets)
3. **Bloom and exposure** for golden-hour cinematic feel
4. **Color grading**: warm shadow tones (gold/amber), slightly cool highlights, color palette matching leonardsibelius.com's gold/platinum/copper
5. **Depth of field**: subtle, only when Code Vision is active (focuses attention on revealed objects)
6. **Hair groom** on the MetaHuman (already excellent out of the box)
7. **30+ fps target** at 1080p on M2 Pro — this is the performance bar

---

## File structure

```
sibelius-game/
├── README.md                      # Project overview, build instructions, screenshots
├── .gitignore                     # Unreal-standard (excludes Saved/, Intermediate/, Binaries/, DerivedDataCache/)
├── docs/
│   ├── design-doc-v0.md           # Copy of the v0 design doc
│   ├── mvp-spec.md                # This file
│   └── screenshots/               # Build progress shots, MVP demo stills
├── Content/                       # Unreal game assets
│   ├── Characters/
│   │   └── Leonard/               # MetaHuman files
│   ├── Environment/
│   │   ├── Office/                # The room
│   │   └── Megascans/             # Imported Quixel assets
│   ├── Mechanics/
│   │   └── CodeVision/            # Post-process material, components
│   ├── UI/
│   │   ├── InteractionPrompts/    # "Press F to open"
│   │   └── EndOfDemoCard/         # End screen
│   └── Maps/
│       └── L_Office_MVP.umap      # The MVP level
├── Source/                        # C++ code
│   └── SibeliusGame/
│       ├── SibeliusGame.Build.cs
│       ├── SibeliusGame.h/cpp
│       ├── Characters/
│       │   └── LeonardCharacter.h/cpp
│       ├── Components/
│       │   └── CodeVisionComponent.h/cpp
│       └── GameMode/
│           └── SibeliusGameMode.h/cpp
└── Config/                        # Unreal config files (DefaultEngine.ini etc.)
```

---

## Predict-bugs preflight (8 expected issues)

Identify these before building so guards are in place from commit one.

1. **MetaHuman LOD pop-in** — Leonard looks great close-up but switches to a lower-poly version at distance, jarring. *Guard*: lock LOD bias to LOD0 for MVP since we never see Leonard from far away. Documented in `LeonardCharacter.cpp`.
2. **Third-person camera clipping** — camera passes through walls when Leonard backs into a corner. *Guard*: use Unreal's standard `USpringArmComponent` with collision testing enabled. Smooth interpolation on collision.
3. **Code Vision shader breaks on transparent objects** — the post-process material misbehaves on the window glass or screen displays. *Guard*: add a separate `Translucent` rendering pass exception in the material; test on the window early.
4. **Real-time Lumen performance on M2 Pro** — frame rate dips below 30fps at 1080p. *Guard*: target high-quality Lumen at 1080p, fall back to Screen Space GI if needed. Profile in editor.
5. **Door interaction prompt appears at wrong distance** — "Press F" shows up from across the room. *Guard*: explicit `InteractionRadius` parameter on the door actor, default 2 meters, with falloff curve.
6. **Hidden object flicker** — when player rapidly toggles E, hidden objects flash on/off. *Guard*: add a small fade-in/fade-out timer (0.15s) on visibility transitions. No instant toggle.
7. **Audio cuts at edge of trigger volume** — the whisper line cuts off if Leonard walks out of the trigger before the line finishes. *Guard*: play the whisper as a non-positional 2D sound from game start, not a 3D trigger.
8. **MetaHuman walking animation glitch on turn** — the body twists awkwardly when the player changes direction rapidly. *Guard*: use Unreal's stock `ALSv4` or built-in third-person blueprint locomotion. Don't roll our own animation blend.

Each guard ships as a small test or documented constraint in code comments.

---

## Acceptance criteria

Same shape as the leonsheet acceptance criteria — clear, binary, verifiable.

1. **Project boots cleanly** — `~/projects/sibelius-game/` opens in Unreal Editor without errors
2. **Player can walk Leonard around the office in third-person** — WASD movement, mouse look
3. **Code Vision activates on hold E** — green/cyan post-process visible while held
4. **Code Vision reveals the hidden door** — door is visible behind bookshelf in Code Vision
5. **Hidden door stays visible after first reveal** — door visible without Code Vision after first encounter
6. **"Press F to open" prompt appears at correct distance** — within 2m of door, not before
7. **Door opens and screen fades to black** — door animates, screen fades cleanly
8. **End-of-demo card appears** — black background, two centered text strings, no errors
9. **Performance: 30+ fps at 1080p on M2 Pro** — verified via editor stat fps
10. **Builds and packages for macOS** — `Project → Package Project → Mac` produces a runnable .app
11. **Repository is on GitHub** — public, with README + design doc + MVP spec committed
12. **Linear SIB-1 marked Done** — issue closed with reference to commit hash

---

## Commit cadence (conventional commits)

Five pause-point checkpoints, each ending with a commit and a review pause.

1. **`feat(scaffold): initialize Unreal project with third-person template`** — empty project, Leonard MetaHuman placeholder, basic level. *Checkpoint 1*.
2. **`feat(env): build the office room with Megascans assets`** — desk, chair, monitors, window, bookshelf, lighting. *Checkpoint 2*.
3. **`feat(mechanic): implement Code Vision post-process and hidden-object component`** — the core mechanic works, hidden door visible in Code Vision. *Checkpoint 3*.
4. **`feat(interaction): door interaction with F-key prompt and animation`** — door opens, player walks through. *Checkpoint 4*.
5. **`feat(ui): end-of-demo screen with chapter title`** — black screen with text, fade transition. *Checkpoint 5*.
6. **`feat(polish): lighting pass, color grading, audio integration`** — visual polish + whisper line + ambient. Final.
7. **`docs: README with screenshots and build instructions`** — documentation.
8. **`chore: tag v0.1.0 and create main branch`** — ship.

Total: ~8 commits matching the conventional-commits scope pattern from leonsheet.

---

## Stack and tools used in this MVP

- **Unreal Engine 5.5+** — engine (free indie license)
- **Quixel Megascans** — free environment assets (built into Unreal)
- **MetaHuman Creator** — Leonard character (free, browser-based, integrated)
- **Mixamo** — free animations
- **ElevenLabs** — Cowork whisper voice line (~$5/mo entry tier, or use free credits)
- **Pixabay / Freesound** — free ambient and SFX
- **Claude Code** — primary engineering partner, using the Claude Code Game Studios 49-agent pattern as applicable
- **Git** — source control, conventional commits
- **GitHub** — `github.com/LeonardSibelius/sibelius-game`, public

---

## What this MVP is NOT

Worth naming explicitly so we don't drift:

- ❌ Not multiple rooms — only the office
- ❌ Not multiple mechanics — only Code Vision
- ❌ Not a save/load system — the MVP is single-session
- ❌ Not networked / multiplayer
- ❌ Not voice acting beyond the one whisper line
- ❌ Not music
- ❌ Not credits screen / main menu — boots straight into the office
- ❌ Not localized — English only
- ❌ Not optimized for any platform other than M2 Pro macOS

All of these come later if v0.1 validates the pipeline.

---

## Hand-off to Claude Code

When Walt is ready to start the build, this spec should be the first thing Claude Code reads. Suggested prompt:

> *"Read `docs/mvp-spec.md` in this Unreal Engine project. We are building the v0.1 one-room MVP for the Sibelius Game. Same predict-bugs-then-pause workflow as leonsheet. Start with Checkpoint 1 (project scaffold), pause before proceeding, and confirm with me before moving to Checkpoint 2. The design doc at `docs/design-doc-v0.md` provides broader context but is not required reading for the MVP itself."*

Claude Code reads the spec, predicts any additional bugs beyond the 8 in the preflight, and we proceed checkpoint-by-checkpoint with explicit review.

---

## Time budget (revised estimate)

Maximum polish target — 6-8 hours, distributed:

| Phase | Time | Output |
|---|---|---|
| Project scaffold + third-person template | 1.0 hr | Walkable Leonard placeholder, empty level |
| Office room build with Megascans | 1.5 hr | Desk, chair, monitors, window, bookshelf, lighting |
| MetaHuman Leonard + animations | 1.0 hr | Final character in place |
| Code Vision mechanic | 1.5 hr | Post-process material, hidden-object component, hidden door |
| Door interaction + end screen | 0.5 hr | F-prompt, animation, fade, end card |
| Audio integration | 0.5 hr | Ambient + whisper + activation sounds |
| Lighting + color grading polish | 1.0 hr | Golden hour atmosphere, palette match |
| Performance profiling + final pass | 1.0 hr | 30+ fps verified, builds clean |
| **Total** | **8.0 hr** | Polished, shippable, demonstrable v0.1 |

---

## Success looks like

Walt comes back from the loan docs, launches Unreal, hands Claude Code this spec, and by the end of the day OR by the next evening, has a 60-second demo he can record on screen, share to LinkedIn, embed in an Outwork piece, or upload to a Steam Greenlight-style "this is what I'm building" page.

If we get there, the game is real. If we don't, we know what's missing and can decide if it's fixable or if the project pivots.

---

*End of spec. Walt's responsibility: review this for anything I got wrong about your vision. Claude Code's responsibility (after lunch): execute the spec, predict bugs, hit the checkpoints.*
