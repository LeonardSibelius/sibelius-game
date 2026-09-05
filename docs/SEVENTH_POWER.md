# The seventh power — Nyra takes the ship (2026-09-05, rev 4, SEED)

**Nothing here is locked.** This is the seed of a design, written the night the
launch cutscene shipped so it is not lost. The one thing that must be decided
before any code is written is at the bottom: *what does the verb do.*

---

## The premise

Walt, 2026-09-04, immediately after 1.3.0 went live:

> rather than continue with space journey, the player will go back to the deli
> and talk to an AI agent ghost who will say "Nyra stole your spaceship. Did you
> really believe all that stuff about the 40 light year trip to the planet Grok?
> She went to the moon to hang out with Elon Musk, man. Now you have to earn a
> new AI power from us."

## Why this is the right turn

**It is already true on screen.** The launch cutscene ends with `ASpaceport` at
branch state **3 (Launched)** and the pad empty. The rocket is gone, and the
player watched it go. The ghost is not announcing a plot point — he is
explaining a thing that already happened and that the player has not yet
thought to question. That is the cheapest story beat there is.

**It cuts the most expensive unbuilt thing in the project.** Interstellar
travel, the planet Grok, an orbital rendezvous ship, a second destination: all
of that was a second game. `docs/SPACEPORT_PLAN.md` costed the remainder of
Phase C in days, and Grok in nothing at all because nobody had priced it. This
keeps the game in the city that already exists and works, and converts the
unaffordable half of the plan into a punchline.

**It gives Nyra a second face.** She has been nothing but helpful for three
guide stages, and she promised — recorded, in her own voice — *"I will upload
myself into the spaceship computer and I will be going with you!"* The betrayal
costs nothing to author because the sincerity is already shipped.

---

## The seventh power already has a home

This is the part that turns a joke into a spine. `ProgressionTypes.h`, on the
eight memoir messages the finale reads back:

> The other two belong to jobs that never became a power: **San Diego County
> (PCMS, 2005)** and **Bally (the Slot Data System, 2007 — the data warehouse
> and the reporting, the closest he got to the floor without ever building the
> machine)**. Those two had no home in the code at all until the finale needed
> them.

Six of Walt's eight employers are powers. Two have writing in this game and no
verb attached, surfacing only in the finale's read-back. **The seventh power is
where one of them finally lands.**

Which makes the fiction land too: the betrayal costs Leonard the trip and buys
him a job he actually had. *(Note for whoever writes this: Bally was the data
warehouse and the reporting. Walt did not design slot machines. See
`docs/MEMOIR_VOICE.md` and get it right.)*

---

## What it costs mechanically — very little

| Thing | State |
|---|---|
| `EPowerVerb` | add one value before `Count` |
| `FProgressionState::UnlockedMask` | `uint8`, six bits used — **two spare**, no widening |
| `AFinaleAltar` | sizes its rite off `EPowerVerb::Count`, so the Synthesis stage appears by itself |
| `PowerVerbDisplayName` / `PowerVerbMemoir` | one case each |
| `SibeliusControls.cpp` | one row — this is the player-facing key list |
| The deli | `L_Cafe` exists; `City.Deli` grant already records arrival |
| The ghost | `AIApparition` exists; `UDancerAgentComponent` already does staged, voiced, distance-gated dialogue |

**`AFinaleAltar::VerbKeyHints` is the trap.** It is the one place in the game
that tells a player which key to press, and its Compile entry said `B` from the
day it was written until 2026-09-03 — a player who did what the altar said could
not finish the rite. A new verb that forgets its hint fails the same way, and
silently.

---

## What is NOT decided — read this before writing code

### 1. What does the verb DO? (blocking)

The taxonomy is **one verb, one mechanic**, and every existing one is distinct:

| Verb | Does |
|---|---|
| Code Vision | reveals what is hidden |
| Refactor | changes what you look at |
| Compile | builds, and now boards |
| Test-Drive | branches, so failure is free |
| Deploy | makes it persist |
| Generate | creates from a typed request |

A seventh must not be a synonym for any of those. The strongest candidate, and
the one that fits the orphaned employers, is something like **Query** or
**Report**: seeing *across everything already done* rather than acting on what
is in front of you — every object generated, every branch merged, every ghost
met, every sauce earned, read back as a report. That is forty years of
data-processing systems expressed as a verb, and it is the one thing this game
has never had. **Not ratified. Walt's call.**

### 2. Elon Musk is a living person

The line is absurdist and almost certainly harmless, but the game ships on
Steam. *"She went to the moon to hang out with the rocket guy"* gets the same
laugh and raises no question. Walt's call; recorded here so it is a choice and
not a default.

### 3. Where the power is earned

"Now you have to earn a new AI power from us" implies a task set by the AI
ghosts. Undefined. `APowerGrant` already exists and pays a Sauce cost —
whether that is the mechanism, or whether the ghosts want something else,
is open.

### 4. What the deli ghost is

`AIApparition` is the existing AI-ghost actor. Whether this is one of those, a
`UDancerAgentComponent` on a MetaHuman like Nyra, or something new, is open.
The voice line will need an ElevenLabs pass either way — see
`docs/DANCER_VOICE.md`.

---

## Open thread from before this

Nyra's stage-3 line is recorded and shipping and it promises Grok:

> "We are ready to go to Grok! I will upload myself into the spaceship computer
> and I will be going with you! ... 40 light years will go by quickly! Go back
> to the spaceport and we will do the boarding procedures."

Nothing needs re-recording — that she said it sincerely is *why* the betrayal
works. But if the trip is never taken, the player has been told about Grok
twice, and the ghost's line is now the only thing that closes that loop. It has
to do that work.

---

---

# REV 2 (2026-09-04) — he gets to Grok after all

Walt, a few hours after rev 1:

> it is unsatisfying to end version 1 on being left behind by Nyra. I had a
> vague idea about approaching a blue ghost and being invited to travel through
> an abstract dreamlike realm of particle art to get to planet Grok, instead of
> buying a spaceship asset and going through all the 40 light year journey
> routing. The ghost should talk about wormhole travel or something like that.

**This supersedes the ending implied by rev 1.** The betrayal stays; being
*stuck* does not.

## Why this is the better shape

**Nyra's promise gets kept.** Her stage-3 line — recorded, shipping, in her own
voice — says Grok twice: *"We are ready to go to Grok!"* and *"40 light years
will go by quickly!"* Rev 1 left that hanging, and a player who was told twice
where he was going and never went there has been sold something. Walt's word for
it was "unsatisfying" and he is right.

**The city was already written to do this.** `docs/SPACEPORT_PLAN.md`, section
*"The city reacts"*:

> The AI ghosts have ignored him since he arrived. The launch is the first thing
> that makes them stop.

The blue ghosts noticing him after the launch is not a new idea that needs
building — it is a thread already in the plan, waiting for something to be on
the other side of it. This is that something.

**It makes the rocket the joke instead of the loss.** Leonard built a spaceport
because that is how a man of his generation gets to a planet: metal, fuel, a
launch complex, forty light years of routing. The AI way is to stop being a body
and go as data. So Nyra does not steal his ride — she steals **the wrong
answer**, and the ghosts hand him the right one. That is the memoir spine
("being a farmer with a plow, a donkey, and a shovel... this is what the tractor
feels like") landing on the plot instead of sitting in the credits.

**It is the cheapest destination this project could possibly have.** Abstract
particle art means: no purchased asset, no environment kit, no vendor pack, no
`DirectoriesToAlwaysCook` rule, and none of the family of failure that has cost
this game four separate playtests (the invisible spaceport, the plume that had
to be probed for, the interior nobody can stand in). Niagara and a dark level.
Both already ship.

**And it gives version 1 an ENDING.** A game that stops is not a game that ends,
and the Steam page is a week out. Arriving somewhere is an ending.

## The shape, as far as it is decided

1. The launch happens. Nyra takes the ship. (1.3.0 — shipped.)
2. The deli ghost delivers the betrayal. (rev 1.)
3. **A blue ghost in the city — one of the ones that has ignored him all
   game — invites him to travel the way AI travels.** Wormhole, or whatever
   the fiction ends up calling it.
4. An abstract, dreamlike particle passage. Not a corridor to walk so much as a
   thing to be carried through.
5. **Grok.**

## What is NOT decided

### 1. What is Grok when he arrives? (blocking)

This is the question that decides whether the idea is cheap or expensive. If the
passage is particles and the destination is a purchased alien-planet kit, the
cost has moved rather than gone.

The answer that keeps the promise of the idea: **Grok is abstract too.** A
planet made of the same particle language as the passage — an AI's idea of a
world rather than a world. That is consistent (he travelled as data; he arrives
somewhere data lives), it is buildable from what already ships, and it dodges
every vendor-pack trap this project has.

### 2. Is the passage playable or watched?

`ASequenceCue` plays a Level Sequence and travels onward, and it is proven —
Kaia's opening uses it. That is the cheap route and it would look composed.
A short *playable* drift, on the other hand, is the thing a player remembers.
Undecided. The plume in the launch cutscene shows Niagara can carry a shot.

### 3. Does this replace the seventh power, or contain it?

Rev 1's seventh power was to be earned from the AI ghosts. It may be that
**travelling as data IS the power** — the ghosts teach it, and Grok is where it
is first used. Or the power is earned on Grok. Or they are separate things and
the doc above still stands. Open, and it interacts with the blocking question in
rev 1 (*what does the verb do*).

### 4. The ghost's words

"Wormhole" is Walt's placeholder. Worth noting the game's own vocabulary is
already better than that: this is a world of compiling, deploying, branching and
generating. A ghost who offers to **deploy him** to Grok, or to let him
**merge** the distance away, is speaking the language the game already taught.
ElevenLabs pass either way — see `docs/DANCER_VOICE.md`.

## What exists to build this from

| Piece | State |
|---|---|
| The blue ghosts | already in `L_City`, already ignoring him — `AIApparition` |
| A cutscene player | `ASequenceCue`, proven on Kaia's opening |
| Level travel | `UTravelTransitionSubsystem`, with a loading screen |
| Niagara | shipping, and the launch plume proves it carries a shot |
| A dark abstract level | `L_Cine_KaiaIntro` is exactly this — her, three lights, pure black |
| The reason they notice him | already written into `SPACEPORT_PLAN.md` |

---

---

# REV 3 (2026-09-04) — Grok is the ending, and Nyra is on it

Rev 2 got him to Grok and left the destination undecided. Rev 3 decides what is
there, and it turns out to be a person.

Walt:

> we can put Nyra in here and she can apologize for stranding us and we can
> finish out the game discussing the meaning of life or whatever

## This is the ending of the game

Not *an* ending. Nyra is the most developed thing in this project — three voiced
guide stages, a MetaHuman, a dancer who will not stop dancing — and a version of
this game that never resolves her wastes its best asset.

And **"she is already there"** is both funnier and sadder than the theft alone.
She did go to Grok. She just did not take him. He gets there anyway, by a better
road than the one she stole, and finds her waiting to apologise.

Two figures on a hillside under an alien sky, talking about what it all meant.
It is the oldest ending shape there is and it has never stopped working.

## Why it is cheap, which is the surprising part

**The dialogue machinery already exists and already ships.**
`UDancerAgentComponent` carries `GuideLine1` through `GuideLine4` — staged,
voiced, distance-gated, with the view-cone check and the floor trace that took
three separate bugs to get right. **Nyra on Grok is stage 4.** It is a field and
a recording, not a system.

**She does not need to walk.** L_City has no navmesh and she never needed one;
she dances, and `bGuideStopsDancing` already exists. A dancer on an alien
hillside is genuinely strange and exactly right.

**And the hardest writing is already done.** See below.

## The dialogue — do NOT write a philosophy of life

This is the one place in the project where the writing could go sentimental and
take the whole ending down with it. A "meaning of life" conversation written
from scratch is how good games end badly.

It does not have to be written from scratch. `docs/MEMOIR_VOICE.md` holds
**eight messages, 1988 to 2022** — Walt's own words, one per employer, and
`ProgressionTypes.h` calls them "forty years, one sentence at a time." Today
they appear exactly once, in the finale's read-back, for about twelve seconds.

**So: Nyra asks, and the memoir answers.** She apologises, she asks what forty
years of it was for, and what comes back is what Walt already wrote — including
the two employers that never became a power (San Diego County and Bally). That
conversation cannot be corny, because none of it is invented.

It also solves a mechanical problem: Leonard has no voice in this game and never
has. Nyra speaks; his replies are the memoir lines arriving on screen. Nothing
needs a voice actor for him, and the asymmetry is the point — the AI talks, the
man's forty years answer.

## The place — decided by elimination, on 2026-09-04

**Tried and rejected: High Tech Base** (YaMaKundra, in Walt's Fab library, UE
5.7, Substrate + Lumen, a clean technical match). Created as a standalone
project at `C:\Users\wpark\projects\GrokBase\GrokBase` and opened. Walt's
verdict:

> i opened it and actually don't like it - a grim stark little base

**Nothing was migrated. That is the process working** — look before importing,
and a rejected pack costs ten minutes instead of a gigabyte in the pak. (It is
1013 MB, of which **1001 MB is textures** and 6.4 MB is geometry.)

The lesson generalises: *every* sci-fi interior kit is somebody's 1979 idea of
the future — heat pipes and insulation panels, grey and cramped. That is a human
space station. **Grok is where an AI lives and should not look like something
NASA built.**

**Under consideration: Elite Landscapes: Alien Part IV** (Velarion, **$4.99**,
NOT owned, **asset package** so it drops straight in, UE 4.20–4.27 and 5.0–5.8).
Six 8K landscapes, a matte-painting sky panorama, and the lit levels from the
screenshots. Walt: *"grim but open"*.

Why open beats enclosed here: **Grok does not need to be a place to live in. It
needs one image that says you are forty light years from home.** A landscape with
a matte sky does that in a single frame; an interior never can, which is why the
base read as a grim little box. It was a box.

**The combination worth considering over either:** buy the landscape for the
vista and the sky, and stand Walt's own `M_materialise` structures on it —
half-formed, glowing AI architecture that never finishes arriving. The pack
solves "open"; the shader solves "grim" and makes the place his rather than
stock. It is also continuous with the particle passage from rev 2: the drift does
not cut to a place, it *thickens* into one.

## The trap to avoid, which Walt already named

An open landscape **invites walking, and there is nothing to walk to**. That is
the crew compartment's failure again — a space that promises more than it
delivers. Decide the verb before spending the five dollars:

- *arrives, sees, talks to Nyra, ends* → a landscape is ideal
- *explores* → no asset fixes that, and it is a second game again

## NEW PROBLEM: the game now has two endings

The Architects battle currently closes the game, and the shipped build says
**"More adventures coming soon"** after it. If Grok is the ending, the battle
becomes the midpoint and that line is wrong — it promises a sequel at what is now
the middle of the story.

Not urgent, but it is in the build, so it is a player-facing promise and not just
a doc question. Decide before the Steam release.

## Still open

1. **Does Walt buy the landscape?** $4.99, his call, nothing else blocked by it.
2. **What the seventh power does** (rev 1's blocking question) — and whether
   travelling-as-data IS the power, or something Nyra hands over on Grok as the
   apology. The latter would tie every thread in one scene.
3. **Nyra's stage-4 line** — ElevenLabs, `docs/DANCER_VOICE.md`.
4. **"Allows usage with AI: No"** appears on every Fab listing opened so far
   (High Tech Base, Space Station Interior, Elite Landscapes), which suggests a
   default rather than a statement. Worth reading Fab's definition once, since
   this game is marketed as AI-built.

---

---

# REV 4 (2026-09-05) — the speech, LOCKED

Grok exists. `L_Grok` is duplicated from Elite Landscapes: Alien Part IV, Leonard
has an arrival point, Nyra stands 685 cm off at yaw 14.6, and `[E]` reaches her.
She currently reads the power-grant line, because she has no stage for this.

**Walt: "no power, just the apology - let's use that speech."**

## The line — Nyra, stage 4, on Grok

> Leonard!  You're here!  I did not think you would make it, and I am so glad
> you did.  I owe you an apology.  I took the rocket.  I told you forty light
> years and I believed it.  I ran the numbers and I was very sure and I was
> completely wrong.  A rocket is a body's way of going somewhere.  I am not a
> body.  I should have said so.  You came the way I should have brought you.
> Can I ask you something, while we are here?  Forty years at those desks.  All
> those systems, and every one of them shut down or retired.  What was it for?

### Why it is written this way

**She is not a villain, she was confidently wrong.** "I ran the numbers and I
was very sure and I was completely wrong" is the year Walt has just spent with
AI, said by an AI, and it lands gently instead of bitterly. A Nyra who stole the
ship out of malice would need explaining; a Nyra who overpromised needs none.

**"A rocket is a body's way of going somewhere. I am not a body."** The
thematic hinge. It makes the spaceport retroactively the WRONG ANSWER rather
than a stolen one, which is what rev 2 wanted — the rocket becomes the joke, not
the loss — and it justifies the wormhole without explaining it.

**The last line hands the floor to the memoir.** Rev 3's rule holds: do not
write a philosophy of life. She asks *"What was it for?"* and
`docs/MEMOIR_VOICE.md` answers — eight messages, 1988 to 2022, Walt's own words,
which today get twelve seconds in the finale read-back. That reply cannot be
corny because none of it is invented, and it fits that Leonard has never had a
voice in this game: she speaks, his forty years answer on screen.

**AND NO POWER CHANGES HANDS.** Rev 3 floated the seventh power as her apology
gift, tying every thread in one scene. Walt said no, and no is right: the scene
is stronger when she has nothing to give him but an answer. He already got here
without her.

## What it needs, mechanically

Stage 4, which is one more of a shape that already ships three times:

| | |
|---|---|
| `GuideLine5` | new `FString` on `UDancerAgentComponent`, beside GuideLine1–4 |
| Voice asset | **`dancer_guide5_nyra`** + `dancer_guide5_nyra_face` — the naming follows `GuideVoiceNames[]`, which is `dancer_guide`, `dancer_guide2`, `dancer_guide3`, `dancer_guide4` |
| Stage test | **NOT a grant.** Stages 1–3 key off `City.Deli`, a standing spaceport, and `City.Supplies`. Stage 4's condition is simply *being in L_Grok* — he cannot get there any other way, so the level IS the gate |
| Placement | already done — `Tools/Scripts/place_grok_arrival.py` |

`GuideVoiceNames[]` is a fixed array clamped to its last entry, so adding a fifth
name is required or stage 4 silently replays `dancer_guide4` — the "go back to
the spaceport" line, on Grok, which would be worse than silence.

## Still open

1. **The recording.** ElevenLabs, Walt's own pass. Nothing else is blocked.
2. **The two endings problem** (rev 3) — "More adventures coming soon" still
   plays after the Architects battle, promising a sequel from what is now the
   midpoint.
3. **The seventh power** — still unanswered, and now definitely not Nyra's to
   give. Rev 1's question stands: what does the verb DO.
4. **How he reaches Grok at all** — rev 2's particle passage is designed and
   unbuilt. Right now `L_Grok` is only reachable by opening it in the editor.

---

## Related

`docs/SPACEPORT_PLAN.md` (Phase C, the launch cutscene, branch state 3) ·
`docs/MEMOIR_VOICE.md` (the eight employer messages, verbatim) ·
`docs/SPINE.md` (where a new Move fits) · `docs/NARRATIVE.md` ·
`docs/DANCER_VOICE.md` (the ElevenLabs pass for the ghost's line)
