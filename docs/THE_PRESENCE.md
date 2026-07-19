# THE PRESENCE — the AI embodied

*Design note, 2026-07-19, from Walt's ramble ("the AI should have an
attractive avatar, a wise presence to give advice and encouragement").
Decisions marked [W] are Walt's, made in-session.*

## The decision

[W] **Archetype: elegant and magnetic** — the Cortana / EDI / Joi lineage:
allure through intelligence, attention, and presence, NEVER through
cheesecake. The standing rule, in one line:

> **Seduction through attention, not skin.** She remembers what you did
> and speaks to you like you matter. That is the pull — and it is also
> the true part of the game's thesis about working with an AI.

[W] **Not Paragon.** Shinbi rejected — the sword reads as threat, and
The Presence must read as welcome.

**Vetoed permanently: waifu/pin-up styling.** It re-shelves the game into
the most ridiculed genre on itch, hands the press the wrong headline
("AI girlfriend casino" would eat the memoir alive), and poisons the
evidence-over-advocacy mission. The memoir's sincerity is the moat.

## The body: MetaHuman (Walt designs her)

- MetaHuman Creator (free, browser): **Walt sculpts her himself** — face,
  age, bearing, hair. A zero-token job that guarantees she is the person
  he means. Import via Quixel Bridge; the project already handles
  MetaHuman meshes (see SlapComponent's leader-pose notes).
- A subtle holographic/translucent shimmer material over the MetaHuman
  = instantly reads as AI, no exposition needed.
- v1 animation scope: ONE idle (retargeted or MetaHuman default), maybe
  a head look-at later. She is a presence, not an action character.

## The voice: she already exists

The game already has a woman's voice: **Mrs. Hall**
(Content/Audio/MrsHall — the Generate verb's judge: "ambiguous",
"no match", "over budget", "unsafe"). The elegant move is EMBODYING the
character the game already has, not adding one. The player's realization
that the voice judging their code was HER all along is a free story
beat. Rename at embodiment if Walt wants; ElevenLabs (one consistent
chosen voice) records her advice lines going forward — and the same
voice can narrate the devlogs (brand character = in-game character).

## Where she lives, what she says

All her lines are hooks into events that ALREADY broadcast — no new
systems, the scattered UI text was her all along:

| Moment | Today | With The Presence |
|---|---|---|
| First contact | AIApparition glyphs + text | She materializes: "I am here to give you power over fate." |
| Shrine trial won | THE MACHINE YIELDS banner | Her voice over the banner |
| Power ceremony | HUD banner + memoir line | She speaks the encouragement; the memoir line stays Walt's text |
| Poker table | "The house suggests..." | Her line — she IS the house |
| Generate verdicts | Mrs. Hall clips | Unchanged — this was her voice all along |
| The temple hall | Empty (future Workshop) | Her home; she is the Workshop's teacher when it's built |

## Build phases (when it earns a slot; Workshop-adjacent)

1. **P1 — the statue that speaks**: MetaHuman + hologram material placed
   in the AI Temple hall, one idle anim, proximity-triggered subtitled
   greeting (HUD-drawn, Shipping-safe). Walt sculpts; agent wires.
2. **P2 — event lines**: subscribe to existing broadcasts (trial won,
   power ceremony, poker advice) and route through her voice + subtitle.
3. **P3 — the Workshop teacher**: when THE WORKSHOP is built, she runs
   it — the par-sheet lessons in her voice. Also the devlog narrator.

## Open questions (Walt's to answer, no rush)

- Her name: stay "Mrs. Hall", or a new name revealed at embodiment?
- Appearance brief for the Creator session (age? era of dress? the
  70s-tech world suggests something timeless rather than futuristic).
- Does she appear ONLY in the temple at first, or also at the cathedral?
