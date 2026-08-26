# Cinematics — MetaHuman Animator lip sync (2026-08-25)

The opening cutscene: **Kaia's face, speaking**, with real lip sync solved from her
ElevenLabs recording. This document is the recipe and, more importantly, the reason it
works when the runtime attempt did not.

---

## THE LESSON: inputs, not outputs

We spent an evening failing to move a MetaHuman's mouth from C++ at runtime. The driver
was correct — 563 updates per shot, mouth value 0.67 from the live audio envelope, morph
target resolved — and her face never moved. The cause, from engine source:

```cpp
// USkeletalMeshComponent::PostAnimEvaluation
UpdateMorphTargetOverrideCurves();                         // our SetMorphTarget lands
if (PostProcessAnimInstance) { ...
    PostProcessAnimInstance->UpdateCurvesPostEvaluation();  // RigLogic replaces the array
```

**RigLogic owns every blendshape weight on a MetaHuman face and replaces the array rather
than merging.** We were writing its *outputs*.

MetaHuman Animator writes its **inputs** — the `CTRL_expressions_*` control curves
RigLogic is built to consume. Same rig, opposite end of the pipe. That is the entire
difference, and it is why one approach is impossible and the other is routine.

> If you ever find yourself fighting RigLogic, you are on the wrong end of it.

---

## Verified result

`/Game/Cinematics/AS_MHP_Kaia_Intro_Face`, solved from `kaia_intro` (23 s of speech):

| | |
|---|---|
| Skeleton | `Face_Archetype_Skeleton` |
| Length | 787 frames / 26.23 s |
| Curves | 260 total, **251 `CTRL_expressions_*`** |
| `CTRL_expressions_jawOpen` | min 0.03, max 0.54, **766 keys** |
| Speech curves animated | **79 of 162** |

Not a jaw flap — a phoneme solve. `mouthLipsTogether` DL/DR/UL/UR fire for *m*/*b*/*p*;
`tongueIn`, `tongueTipUp`, `tongueTipDown`, `tongueWide` and `tonguePress` are all
animated; `mouthLipsPurse` and `mouthLowerLipDepress` shape the vowels. Her tongue moves.

---

## The recipe

### 1. Record the line

ElevenLabs, in the character's established voice (Kaia = **Ivanna - Seductive &
Intimate**, Eleven v3, Natural stability). Save to `Tools/Audio/<name>.mp3`.
See `docs/DANCER_VOICE.md` for the full ElevenLabs walkthrough.

### 2. Import it as a SoundWave

Add the clip to `CLIPS` in `Tools/Scripts/import_cinematic_audio.py`, then, editor closed:

```
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" -run=pythonscript -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_cinematic_audio.py" -AllowCommandletAudio -unattended -nopause -nosplash -stdout
```

**`-AllowCommandletAudio` is load-bearing** — without it every SoundWave import dies on
`Decoder for AudioFormat 'BINKA' not found` and leaves an empty folder that looks like
success.

### 3. Create the Performance asset

Scriptable — no clicking. `unreal.MetaHumanPerformance` and
`MetaHumanPerformanceFactoryNew` are both exposed to Python:

```python
perf = tools.create_asset(NAME, "/Game/Cinematics",
                          unreal.MetaHumanPerformance,
                          unreal.MetaHumanPerformanceFactoryNew())
perf.set_editor_property("input_type", unreal.DataInputType.AUDIO)
perf.set_editor_property("audio", eal.load_asset(AUDIO))
```

**The audio path needs ONLY a SoundWave.** With `InputType = Audio`, the
`FootageCaptureData`, `MetaHuman Identity` and `Camera` fields are all hidden
(`EditCondition` in `MetaHumanPerformance.h`). No identity, no depth footage, no iPhone.

### 4. Process and export (editor, by hand)

Open the Performance asset → **Process** → **Export Animation**.

- The first Process loads the Speech2Face neural models; give it a minute.
- The exporter names the result `AS_<PerformanceName>_Face`, **not** whatever you typed.
- Do not judge success by the A|B viewport — with no **Control Rig** set under
  *Visualization* it shows nothing useful. Verify the curves instead (below).

### 5. Verify the solve, objectively

Do not squint at a preview. Load the Anim Sequence and check that the mouth curves are
actually animated — and check the **mouth** ones specifically. Curve names are
alphabetical, so a capped scan finds brows and blinks and stops long before `jaw`/`mouth`,
which looks like success and proves nothing:

```python
lib = unreal.AnimationLibrary
names = [str(n) for n in lib.get_animation_curve_names(a, unreal.RawCurveTrackTypes.RCT_FLOAT)]
speech = [n for n in names if any(k in n.lower() for k in ("jaw","mouth","lip","tongue"))]
# then get_float_keys(a, n) and confirm max - min > 0.001
```

`CTRL_expressions_jawOpen` moving is the single check that matters.

---

## Prerequisite

The **MetaHuman Animator** plugin (`"Name": "MetaHuman"` in `SibeliusGame.uproject`) —
distinct from `MetaHumanCharacter`, which was already on and is *not* enough.
`MetaHumanSpeech2Face` lives in the Animator plugin and is **Editor-only, Win64**. Nearly
every module in it is Editor type, so the runtime and cook cost is close to nil.

---

## What lives where

| Thing | Path |
|---|---|
| Source recording | `Tools/Audio/kaia_intro.mp3` (LFS) |
| SoundWave | `/Game/Audio/Cinematics/kaia_intro` — **gitignored**, regenerable |
| Performance | `/Game/Cinematics/MHP_Kaia_Intro` |
| Face animation | `/Game/Cinematics/AS_MHP_Kaia_Intro_Face` |
| Import script | `Tools/Scripts/import_cinematic_audio.py` |

---

## The opening line (script A, locked 2026-08-25)

> *[soft] [intimate]* Hello, Leonard. I am Kaia. I am an AI agent. For forty years you
> built everything by hand... every line, every table, at a desk like this one. For people
> like Mrs. Hall. *[pause]* That is over now. *[warm]* Come upstairs. I have powers to
> give you, and you will never build the old way again.

**Why she uses his name.** `MrsHallLines.h` carries a locked voice note: *"Mrs. Hall never
uses the protagonist's name (he earns 'Leonard Sibelius' over the game; she refuses it).
She addresses him only as 'Programmer.'"* So the first words of the game are an AI
granting him the identity his employer denies. That is the scene, not the introduction.

---

## Still open

- **The shot.** Dark background (Walt, 2026-08-25) — a cutscene portrait, not the office.
- **The render.** Sequencer + Movie Render Queue → MP4.
- **Playback.** An `AVideoCue` actor: full-screen, input locked, HUD blanked via
  `ASibeliusHUD::HoldCinematic`, skippable, once per save. Video files must be staged
  **non-UFS** like `WebGame`/`Journal`/`Data`, or the cutscene works in PIE and is silently
  absent from the shipped build.
- **Runtime lip sync, reopened.** This Anim Sequence is exactly what RigLogic wants, and
  the dancers' Face components are `leaderpose=no` — they can evaluate their own
  animation. Playing a per-line face anim during the talk close-up is now a real
  possibility rather than the dead end recorded in `docs/DANCER_VOICE.md`. Unproven, but
  no longer ruled out.
