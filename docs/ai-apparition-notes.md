# The AI Apparition — opening cinematic (June 11, 2026)

The game opens with the bang NARRATIVE.md promised: "Anthropic AI suddenly
appears and gives the nameless programmer new powers." A ring of the nine
fate glyphs orbits a blazing white-gold core; a god-voice (ElevenLabs, by
hand) speaks:

> "I am AI. I am here to give you power over fate. Find the Carousel of Fates."

The glyphs are the SAME nine sprites that orbit the Carousel of Fates at the
cathedral apse — the ending foreshadowed in the first minute. The quest object
is named before the player takes a step.

## Decisions (AskUserQuestion, June 11)

- Form: ring of glyphs + blazing core (carousel visual language, zero new art)
- Line: Walt's wording as-is
- Voice: ElevenLabs by hand (Walt downloads MP3; Cowork converts/imports)
- Trigger: ~3 s after game start; controls locked during the speech
- Home: L_Office_v02 (the frumpy office). NOTE: GameDefaultMap still points
  at Lvl_FirstPerson — flip later if the game should boot into the office.

## Sequence (all Tick-driven, no Timelines, no BP assets)

Waiting (DelaySeconds 3) → Materializing (1.8 s, scale 0→1 smoothstep, core
glow flares) → Speaking (PlaySound2D; duration from the SoundWave, fallback
9 s; core pulses) → Linger (2 s) → Dissolving (1.5 s, shrink + dim; controls
restored HERE — player gets the room back as the god fades) → hidden, tick off.

## Bug ledger — predicted BEFORE building (the discipline)

- AP1 ROTATOR. All FRotator construction explicit (Pitch, Yaw, Roll) with
  named comment; orientation knobs are UPROPERTYs (the institutionalized
  lesson — wrong-facing glyph is a Details fix, not a recompile).
- AP2 INPUT LOCK LEAK. SetIgnoreMoveInput/SetIgnoreLookInput must restore on
  EVERY exit: one EndCinematic() called from Dissolving entry AND EndPlay.
  Never two restore paths that can disagree (the CloseScreen lesson).
- AP3 MISSING VOICE ASSET. If S_ai_intro fails to load, the sequence still
  runs visually with FallbackSpeakSeconds — never a soft-lock, just silence
  and an Error log.
- AP4 AUDIO FORMAT. ElevenLabs hands out MP3; UE import is happiest with
  16-bit PCM WAV. Cowork converts in sandbox (ffmpeg) before import. Script
  imports Tools/Audio/ai_intro.wav specifically.
- AP5 ORDER OF OPERATIONS. Unlike the altar (script-then-build), this is
  BUILD FIRST, THEN SCRIPT: the script spawns the AAIApparition actor, so the
  class must exist. Script errors loudly if the class isn't found.
- AP6 PLAYERSTART FACING. Script spawns the apparition 350 cm along the
  PlayerStart's forward vector at core height 150 cm. If the PlayerStart
  faces a wall, the apparition is in the wall — it's a placed actor, drag it.
- AP7 AUDIBILITY. PlaySound2D (no attenuation, no spatialization) — a god is
  omnipresent; also immune to "spawned behind your head" volume bugs.
- AP8 M_fate_base DEPENDENCY. Glyph cards reuse M_fate_base + T_sym_* (global
  assets, fine from any level) — but they only exist because the altar script
  ran. BuildRing errors loudly if missing.
- AP9 PIE RE-ENTRY. All state (phase, timers) lives in instance members reset
  in BeginPlay — second PIE run must behave like the first.
- AP10 SHIPPING NOTE. One-shot flag: the apparition runs once per BeginPlay.
  If the office level is revisited mid-game later, gate on a save flag (not
  built yet — out of scope today; logged so future-us knows).

## Files

- Source/SibeliusGame/AIApparition.h / .cpp
- Tools/Scripts/build_ai_apparition.py (M_ai_core + WAV import + placement)
- Tools/Audio/ai_intro.wav (converted from Walt's ElevenLabs MP3; the MP3
  itself also lives there as source-of-truth)
