# The seventh power — Nyra takes the ship (2026-09-04, rev 2, SEED)

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

## Related

`docs/SPACEPORT_PLAN.md` (Phase C, the launch cutscene, branch state 3) ·
`docs/MEMOIR_VOICE.md` (the eight employer messages, verbatim) ·
`docs/SPINE.md` (where a new Move fits) · `docs/NARRATIVE.md` ·
`docs/DANCER_VOICE.md` (the ElevenLabs pass for the ghost's line)
