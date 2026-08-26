# The AI agents speak — dancer talk voice (2026-08-25)

Press **E** on a dancer who has nothing left to give and she says, out loud:

> "I have granted you a power.  Use it wisely."

Every agent says the same line — Kaia, Nyra, Isla, Aisling, Elise, and the
dancer the finale altar summons. They all did the same thing to you, so they
all say the same thing about it.

**The HUD goes completely dark for the length of the shot.** That is the other
half of this change. The close-up frames her face at 38° of FOV, and the HUD
was drawing the crosshair through her eye, the objective across her forehead,
the sauce counter over her shoulder, the Test-Drive hint under her chin, and
her own greeting subtitle across her mouth. A portrait that is half text is not
a portrait. The line is spoken now, so nothing needs to be written.

---

## Decisions

- **Spoken, not written.** The old greeting ("Hi. I am AI Agent Kaia. Wanna
  Fight?") was a HUD subtitle. It is gone from the close-up entirely.
- **One recording, all agents.** `dancer_power` is shared. A per-agent file
  (`dancer_power_kaia`) overrides her, and needs no code change — the
  component looks for the per-agent name first, then falls back.
- **Silence is a valid state.** No clip on this machine → the close-up still
  runs, silently, with one Warning naming the file it wanted. Never a
  soft-lock (the apparition's AP3).
- **The dance crawls instead of freezing.** `TalkDanceSpeed = 0.1` — 10 %
  speed. A hard freeze made her a waxwork for seven seconds; a crawl keeps
  her breathing. Set it to `0` for the old freeze.
- **The hold follows the clip.** Seven seconds by default, or the clip's
  length plus `TalkTailSeconds` (1.4 s) if the recording is longer.

---

## The cast (2026-08-25)

Every agent has her own recording, and says her own name:

> "I am AI agent **Kaia**.  I have granted you a power.  Use it wisely."

Five **Ivanna** voices from the Voice Library — the same actress, five different
reads, which is what an agent lineage should sound like.

| Agent | Voice | Asset |
|---|---|---|
| Kaia | Ivanna - Seductive & Intimate | `dancer_power_kaia` |
| Nyra | Ivanna - Sultry, Fun and Captivating | `dancer_power_nyra` |
| Isla | Ivanna - Expressive, Articulate and Bold | `dancer_power_isla` |
| Aisling | Ivanna - Young, Versatile and Casual | `dancer_power_aisling` |
| Elise | Ivanna - Candid, Peppy and Genuine | `dancer_power_elise` |
| *(anyone else)* | Ivanna - Seductive & Intimate | `dancer_power` — **nameless** |

**The shared clip stays nameless on purpose.** It is what a dancer without her own
take falls back to — including the one `AFinaleAltar` summons at the cathedral — and a
fallback that confidently announces the wrong name is worse than one that announces
none.

| Setting | Value |
|---|---|
| Model | Eleven v3 |
| Stability | Natural (middle of three stops: Creative / Natural / Robust) |
| Prompt | `[soft] [intimate] I am AI agent <name>. I have granted you a power... Use it wisely.` |
| Output | MP3 44.1 kHz 128 kbps, ~5-6 s each |

Two takes come back per generation on v3; Generation 1 is what shipped in each case.
All takes stay in ElevenLabs **History** if you want to revisit them.

**Names to listen for.** `Aisling` (ASH-ling) and `Isla` (EYE-luh, silent s) are the two
a TTS model is most likely to get wrong. If either is mispronounced, respell it
phonetically in the prompt — the spelling only exists to produce a sound, so
`Ashling` is a legitimate fix — regenerate, and drop the new file in over the old one.

The line lives in code as `UDancerAgentComponent::TalkLine`, with `{0}` standing in for
her name (`GetSpokenLine` does the substitution). The string is only used for the log
when a clip is missing — the recording is the real line.

## Making the voice in ElevenLabs

<https://elevenlabs.io/app/speech-synthesis/text-to-speech>

### 1. Pick the voice

Two ways, and the second is the better one for this:

- **Voice Library** (browse) — filter **Gender: Female**, then search the
  library for `sultry`, `seductive`, `breathy`, `intimate`, `ASMR`, or
  `whisper`. Community voices carry those tags. Add the one you like to *My
  Voices* so it stays on the dropdown.
- **Voice Design** (describe it) — type what you want in plain words and
  ElevenLabs generates three candidate voices to choose from. For this
  character something like:

  > *A low, breathy female voice in her late twenties. Warm, unhurried,
  > slightly amused. Close-mic intimacy, as if speaking to one person a foot
  > away. Neutral American accent.*

  This is the one to use if the library is not giving you what you want —
  you are describing a character, not shopping.

Of the built-in premade voices, the softer/warmer female ones are the right
neighbourhood; the crisp "narrator" and "newsreader" voices are not. Audition
with the actual line, not the sample text — the line is nine words long and
some voices only come alive over a paragraph.

### 2. Settings

| Setting | Value | Why |
|---|---|---|
| Model | Multilingual v2 (quality) or v3 (expressive tags) | v3 understands `[whispers]`-style direction |
| **Speed** | **0.9** | Slightly under 1.0 reads as deliberate. This is most of the effect |
| **Stability** | **~40 %** | Lower = more emotional colour. Too low wanders between takes |
| **Similarity** | **~75 %** | Standard |
| Style exaggeration | 0–15 % | Higher invents artefacts and gains you nothing on nine words |
| Speaker boost | On | Cleaner presence |

### 3. The text

Type it with the pause built in — punctuation is the only timing control
you have:

```
I have granted you a power... Use it wisely.
```

The ellipsis gives the beat between the two sentences. If you are on the v3
model you can direct the read inline:

```
[whispers] I have granted you a power... [softly] Use it wisely.
```

**Generate three or four takes** and keep the best. They differ noticeably at
40 % stability, and this is nine words you will hear a hundred times.

### 4. Download and save it

Download the take (ElevenLabs hands out **MP3** — that is fine, see below) and
save it as:

```
C:\Users\wpark\projects\sibelius-game\Tools\Audio\dancer_power.mp3
```

**No conversion needed.** UE 5.7 imports `.mp3` directly — the engine's sound
factory takes `wav, aif, aiff, ogg, flac, opus, mp3`. The old
`ai-apparition-notes.md` AP4 rule ("ElevenLabs hands out MP3; convert to WAV
first") is obsolete, and there is no ffmpeg on this machine anyway.

### 5. Import it

**Editor closed, one command** (PowerShell):

```
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" -run=pythonscript -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_dancer_voice.py" -AllowCommandletAudio -unattended -nopause -nosplash -stdout
```

It imports every `Tools/Audio/dancer_power*` file it finds into
`/Game/Audio/Dancers/`, clears the looping flag, and saves. Re-running replaces
in place.

> **`-AllowCommandletAudio` is load-bearing.** Leave it off and the import
> fails with `Decoder for AudioFormat 'BINKA' not found` and leaves an *empty*
> `/Game/Audio/Dancers` folder — which looks like success until you go looking
> for the asset. A python commandlet does not register the audio format
> modules, and every SoundWave import needs the BINKA encoder to build its
> compressed data.

*Or from the open editor:* **Cmd** box at the bottom of the main window (mode
dropdown → *Cmd*), then `py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_dancer_voice.py"`.

*Or by hand:* drag the MP3 into the Content Browser at `/Game/Audio/Dancers`,
rename the asset to exactly `dancer_power`, open it, and confirm **Looping** is
off.

### 6. Hear it

PIE in `L_Office_v02`, walk up to Kaia, press **E**. The HUD vanishes, the
camera lands on her face, she speaks. If she is silent, the Output Log says
which file it looked for:

```
LogSibeliusGame: Warning: [Dancer] Kaia has no voice clip - close-up runs silent.
Import the recording of "I have granted you a power.  Use it wisely." as
/Game/Audio/Dancers/dancer_power ...
```

### 7. Per-agent takes, later

Same recipe, different filename — `dancer_power_elise.mp3` in `Tools/Audio`,
re-run the import script, and Elise now has her own voice while everyone else
keeps the shared one. The lookup is `dancer_power_<agent lowercase>` first,
`dancer_power` second. Agent names come from the actor label (Kaia, Nyra, Isla,
Aisling, Elise).

---

## Licensing — read this before shipping

The free ElevenLabs tier is **non-commercial and requires attribution**. This
repo's `.gitignore` already carries the standing rule for AI voice:

> Clear the synthetic-voice commercial license before any shipped build.

That applies to this line the same as to Mrs. Hall's refusals. A paid tier
carries the commercial license; sort it out before the itch/Steam push, not
after.

---

## Where the files live

| Thing | Path |
|---|---|
| Source recording (yours, keep it) | `Tools/Audio/dancer_power.mp3` |
| Imported asset | `/Game/Audio/Dancers/dancer_power` |
| Import script | `Tools/Scripts/import_dancer_voice.py` |
| Code | `Source/SibeliusGame/DancerAgentComponent.cpp` — `PlayTalkVoice` |
| HUD blanking | `Source/SibeliusGame/SibeliusHUD.cpp` — `HoldCinematic` |

**The audio is gitignored** (`Content/Audio/Dancers/`), like all AI voice in
this project — local-only, back it up by hand. Gitignored content reaches a
packaged build **only** because `DefaultGame.ini` lists
`+DirectoriesToAlwaysCook=(Path="/Game/Audio/Dancers")`. That line is the
difference between "works in PIE" and "works in the pak" — the v0.7.4
soft-ref lesson, again.

---

## Mouth movement — settled, and why

**She does not move her mouth, and `bTalkMouthMotion` ships `false`.** This was chased
to the end; the result is a mechanism, not a mystery.

### What was built and proven working

Lip blend shapes driven by the real audio envelope. Measured on Kaia in one close-up:

```
MOUTH DIAG: mouthMotion=ON tickEnabled=yes active=yes face='Face'
            mesh='SKM_MHC_Kaia_FaceMesh' morphs=858 shape=FOUND leaderpose=no audio=yes
shot ended: 563 update(s), peak mouth 0.67
```

563 updates, morph target resolved, mouth value driven to 0.67 by the actual waveform.
Her face did not move.

### Why it cannot work

`USkeletalMeshComponent::PostAnimEvaluation`, in this order, every frame:

```cpp
UpdateMorphTargetOverrideCurves();                        // our SetMorphTarget lands here
if (PostProcessAnimInstance) { ...
    PostProcessAnimInstance->UpdateCurvesPostEvaluation(); // RigLogic writes the array
```

**RigLogic owns every blendshape weight on a MetaHuman face and replaces the array rather
than merging into it.** Our value is applied, then overwritten one line later. Scaling,
smoothing, or picking different shapes changes nothing — the write simply happens after
ours.

Layering on top means being *inside* the anim graph, after the RigLogic node — editing
`ABP_Face_PostProcess`, the rig this project deliberately leaves alone (v0.9.7.2: *"C++
Control Rig and Flite TTS wrecked the portrait"*).

### The three dead ends, for the next person

| Approach | Why it fails |
|---|---|
| `SetMorphTarget` on lip shapes | Overwritten by RigLogic in `PostAnimEvaluation`. Verified in game. |
| `UAnimInstance::OverrideCurveValue` | Writes the post-evaluation map; discarded next evaluation. |
| Jaw control curve from C++ | The jaw is a joint driven by a RigLogic control curve. Unreachable at runtime; 858 morphs on the face mesh and not one named for the jaw. |

### The route that would actually work

UE 5.7 ships MetaHuman Animator's **audio-driven animation** (`MetaHumanSpeech2Face`,
Editor-only, NNE models under
`Engine/Plugins/MetaHuman/MetaHumanAnimator/Content/Speech2Face`). Feed it the same
ElevenLabs clip in the editor and it bakes a face Anim Sequence with the RigLogic curves
solved — jaw, phonemes, the lot. That goes *through* RigLogic instead of fighting it,
which is exactly why it works where this did not.

And it is more tractable than first feared: the diagnostic measured **`leaderpose=no`**,
so these faces are already in the animated configuration MetaHuman uses for performances
(leader pose nulled, Copy Pose From Mesh in the graph). The work is generating the
performance and playing it into a slot — one editor pass per clip, plus careful testing
that the portrait survives. A scoped feature, not a code tweak.

The C++ is kept, switched off, because it is the working half of that feature: the
envelope, the timing and the shape blending are all correct and measured. Only the write
is losable.

---

## Steady framing

`TalkDanceSpeed` is **0**. It shipped at 0.1 for one afternoon on the theory that a
frozen dancer reads as a waxwork. She does — but a dancer who keeps dancing swings her
head clean out of a 38° frame. Walt: *"I was hoping to keep a steady focus on their
faces."*

### The close-up heartbeat

`UDancerAgentComponent` is attached at **runtime** by `UDancerAgentSubsystem`'s scan, and
its `TickComponent` **never fires**. Three fixes failed — `bStartWithTickEnabled`,
always-on ticking, `bAutoActivate` — with the last logging `tickEnabled=yes active=yes`
alongside zero calls. Registered, enabled, active, never invoked. The cause is still
unknown.

So the shot runs on a **60 Hz looping timer** (`TalkTick`) instead. `FTimerManager` has
been reliable in this class from the start. That timer is also what makes the camera
follow her face — `UpdateTalkShot` had never once run before it, which is the real
reason heads used to drift out of frame.

`TickComponent` remains as an observer: if it ever fires it logs once, and the timer can
go.

**Lesson worth keeping: `IsComponentTickEnabled()` is not evidence that anything ticks.**
A counter is. Adding one is what turned three rounds of guessing into an answer.
