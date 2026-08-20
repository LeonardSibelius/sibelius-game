# THE SPINE — making the game cohere

*Design doc, 2026-08-19. **Nothing is built yet.** Written after Walt asked the right
question: "It seems like a bunch of stuff is tacked together in this game and the plot is
not strong." Decisions are proposals; the list at the bottom is what wants his sign-off.
Same process as `docs/FLOOR_REPORT.md` — doc, ratify, then implement.*

**The finding in one line:** the plot is not weak. It is **not staged**. Almost everything
this document proposes already exists in the repository, written down, and unused.

---

## What the game already has

`docs/NARRATIVE.md` is a real spine, and a better one than most games get:

- An aging programmer who **integrated the systems around the magic and watched other
  people build the magic itself.**
- An employer, **Mrs. Hall**, who keeps him on an archaic legacy system at low pay and
  **forbids him the one thing that would free him.**
- Six powers that *are* AI assistance made literal — see, change, build, try, ship, ask.
- A theme every mechanic actually serves: **legibility**.
- An ending where he finally builds the machine he spent a career adjacent to.

`docs/MEMOIR_VOICE.md` holds the emotional payload: eight messages to eight former
employers, forty years, written in one sitting. It is the best writing in the project.

None of that is a content problem. It is a **staging** problem.

---

## What the player actually gets — traced in code, not assumed

### 1. The antagonist appears in one chapter, as an error handler

`MrsHallLines` and `MrsHallMessageWidget` are referenced by exactly one gameplay class:
`UGenerateComponent`. Her voice clips are `mrshall_nomatch`, `mrshall_ambiguous`,
`mrshall_overbudget`.

She is, functionally, **a validation failure message in Chapter 6 of 7.**

For the first five chapters nothing opposes the player. That is the whole reason the game
reads as a tour. Walt described his own game as *"you go play some poker, you click a book
and get some sauce, then you go to the second floor and all kinds of stuff is going on."*
That is a list, not a story — and it is a list because **nothing in it wants anything.**

### 2. The premise is not in the game

The pitch: kept on a legacy system, low pay, boring work he hates, forbidden to use AI.

There is no job in this game. No ticket, no deadline, no legacy system, no one waiting.
The single most relatable thing in the premise — and the thing Walt actually lived,
maintaining a dead language kept alive by one elderly man in Britain — never appears.

### 3. The payload is spent as loose change

The eight memoir messages fire as **12-second HUD text** at power unlocks, one at a time,
scattered, unrecoverable. Six of the eight are wired
(`SibeliusHUD.cpp::MemoirLineForVerb`); two are level placards. Nothing collects them.

### 4. ~~The journal hands the player the design document~~ — WRONG, corrected 2026-08-19

The first draft of this document claimed pressing **J** shows the player the lore bible.
**It does not.** `RefreshFromNarrative()` loads `Content/Journal/HOW_TO_PLAY.md` — a proper
player guide, staged as a loose file so it ships — with `docs/HOW_TO_PLAY.md` as the
editor-time fallback. Walt changed that at some point and the panel has been right ever
since.

What was stale was the **class comment**, which still described the old behaviour, and I
read the comment instead of the function. That is twice in one session that a stale comment
has produced a wrong conclusion (the other being `Pad()`'s claim that it aligns text in a
proportional font). Both comments have been corrected in place.

The journal being already correct made Move 3 *smaller*, not larger: the record has a good
home to go into rather than a bad one to replace.

---

## The spine that is already written and never dramatized

From `MrsHallLines.h`, a voice rule Walt set months ago:

> **CRITICAL voice note:** Mrs. Hall never uses the protagonist's name (he earns "Leonard
> Sibelius" over the game; she refuses it). She addresses him only as "Programmer" — her
> clipped "You. Programmer."

And from `NARRATIVE.md`: he *"starts the game as a nameless, aging programmer"* and
*"becomes a new entity, Leonard Sibelius."*

> 🔒 **THE ARC IS THE TITLE.** The game is about **earning a name.** You begin nameless.
> Your employer refuses to call you anything but "Programmer." Every power you take is
> something she forbade. At the end you build the machine you were never allowed to build,
> and you take the name.

**Correction (Walt, 2026-08-19).** An earlier draft of this document claimed the game
never dramatizes this. That was wrong, and the error mattered: **it already does.** Nine
lines ship in `Content/Data/MrsHallLines.csv`, spoken in an ElevenLabs voice —

> *"You. Programmer. We don't keep that here."*
> *"That's enough of that. What am I paying you for, Programmer, if you lean on it all day?"*

The device works. It is **wired to the wrong trigger.**

Every one of those nine lines is a **refusal reason**: `NoMatch`, `Ambiguous`,
`OverBudget`, `Unsafe`. So the only time the player is denied his name is when the
**Generate catalog missed** — a content limitation, not a story beat. He typed "a pine
tree," the game did not have one, and his boss called him Programmer for it.

The name should be denied when he **takes a power she forbade**, which is a story event,
and it currently never is.

Everything below is in service of that single line.

---

## The four moves

None of these add rooms, mechanics, or zones. All four stage material that exists.

### Move 1 — Give the player a job in the first two minutes

v0.9.3 already spawns the player in a home office. Mrs. Hall messages him there: a ticket
against the legacy system, and a reminder that he is not to use AI for it.

That single message converts the whole building from a museum into a workplace with
someone waiting. Every room afterwards has a reason to be entered.

**Cost:** one scripted message at spawn, using the widget that already exists.

### Move 2 — Let her speak from Chapter 1, not Chapter 6

> 🔒 **RATIFIED (Walt, 2026-08-19): "yes, mrs hall should speak from chapter 1."**
> Design below is locked; implementation is the next thing built.

#### What already works and moves unchanged

`UMrsHallMessageWidget` is better than it needed to be and is fully reusable: a styled
office memo, **never takes focus**, `HitTestInvisible`, ZOrder 50, auto-dismissing after
six seconds, with a toast fallback when there is no player controller so headless runs
stay green and silent. None of that changes.

The problem is only that its *lifecycle* — widget ownership, the dismiss timer, the clip
playback — lives inside `UGenerateComponent`, so nothing else can speak as her.

#### The channel

Extract to **`UMrsHallSubsystem : UWorldSubsystem`**, matching the project's habit of
putting shared systems in subsystems.

```
void Say(FName Reason);                            // pick a line from the CSV, show, speak
void SayLine(const FString& Line, FString Audio);  // authored one-off (the opening ticket)
```

World scope, not GameInstance: it owns a viewport widget and a world timer, and a
GameInstance subsystem would outlive a level load and leak the widget. The rotating
selection counter resetting per level is fine — the smoke-test discipline requires *no
RNG*, not persistence.

`UGenerateComponent` then calls `Say("NoMatch")` and keeps behaving exactly as it does.

#### When she speaks — the beat is FIRST USE, not the grant

> 🔒 **She reacts to the ACT, not the acquisition.** You claim Code Vision at the shrine;
> that moment is yours — the banner and your memoir line to a former employer. Later, the
> first time you actually hold **V** and look through a wall, the memo arrives. She found
> out.
>
> **Rejected: speaking at the power grant.** Three messages would land at once — banner,
> memoir line, and her memo — which is the "one channel, two speakers" collision already
> recorded in `SibeliusHUD.h` when the cauldron stomped the Presence's greeting. Delaying
> her past the memoir's 12 seconds would fix the overlap and lose the drama; reacting to
> first use fixes both, because it happens somewhere else entirely.

Implementation is one line per power, at each activation site:

```cpp
if (Progression->ClaimOneTimeGrant(TEXT("Hall.FirstUse.CodeVision")))
{
    MrsHall->Say(TEXT("Power.CodeVision"));
}
```

`ClaimOneTimeGrant` already exists, is already saved, and already returns true exactly
once ever — so she catches you once per power, per save, with no new state.

#### What she says — escalation is the point

One `Reason` per power so the arc is authored rather than shuffled. She gets worse as he
gets stronger, and **every line uses "Programmer"** — that is where the name denial
belongs, at a story beat.

| Reason | Beat | Register |
|---|---|---|
| `Ticket` | opening, Move 1 | the job, the legacy system, no AI |
| `Power.CodeVision` | first V | dismissive — *he is imagining things* |
| `Power.Refactor` | first R | irritated |
| `Power.Compile` | first C | suspicious — where did the parts come from |
| `Power.TestDrive` | first branch | warning |
| `Power.Deploy` | first deploy | threatening — this is her authority |
| `Power.Generate` | Ch6 | the existing refusals; her strongest, already written |
| `Final` | Synthesis | she loses |

Adding these is **CSV rows**, not code. Voice clips follow: the system is silent when a
clip is missing, so the text ships first and Walt's ElevenLabs pass lands after.

#### Gate

`GenerateSmokeTest` gains a check that **every `Reason` the code asks for has at least one
row in the table.** A missing Reason makes her silently say nothing — no error, no log,
exactly the failure class that hid the interaction prompts for eight releases.

This is the highest-leverage change in the project, and it is mostly wiring already owned.

`MrsHallLines` is a **CSV-backed DataTable** with deterministic selection by a rotating
counter — explicitly no RNG, to preserve smoke-test discipline. Adding what she says is
**data, not code.**

What changes: promote the refusal channel to a general narrative channel any chapter can
call, keyed by new `Reason` values (`PowerTaken`, `Ticket`, `Escalation`, `Final`), and
have her react each time the player takes a power.

She should get **worse** as he gets stronger. She is most threatened by **Generate** —
"ask and it appears" — which is where her existing lines already live, so the escalation
lands where the content already is.

> 🔒 **She stays disembodied.** The README's rule holds: she is a *system*, not a person —
> messages, refusals, block walls. Do not give her a body. The Presence already occupies
> the "embodied voice" slot, and two speaking figures would compete.

**Cost:** a `Reason` enum widening, a handful of CSV rows, one call site per power grant.

### Move 3 — The memoir becomes a collection, and the collection becomes the ending

Three changes to material that already ships:

1. Each message, when earned, is **recorded** rather than only flashed.
2. **J** shows the collected messages — the player's own accumulating record — instead of
   dumping `NARRATIVE.md`.
3. The finale plays them **as a sequence.** Forty years one grudge at a time, then all at
   once. That is the climax.

> 🔒 **The eight messages are the ending, not the rewards.** They are currently spent as
> scattered 12-second toasts and cannot be re-read. This is the strongest writing in the
> project being thrown away in ones.

This also fixes the shipping bug noted in `JournalWidget.h` itself: it reads from `docs/`,
which is not staged into a packaged build.

**Cost:** a saved array of earned message ids, a journal rewrite, a finale sequence.

### Move 3.5 — Make the catalog's poverty deliberate

Walt, 2026-08-19: *"The 'G' key puts junk into the scene."*

He is right, and the numbers say why. `Content/Data/GenerateCatalog.csv` holds **six
objects**: a tree, a plant, a lamp, a crate, a chair, a key.

Chapter 6 is the climactic power. `NARRATIVE.md` says Generate is *"ask for what you need
in plain language and have it appear"* — the most AI-like power of all, and the one Mrs.
Hall fights hardest because it is the deepest threat to the small controllable box she
keeps him in.

In play, it spawns a potted plant. Ask for anything outside those six and you are refused
— which, given the space of things a person might type, is most of the time.

**Do not fix this by growing the catalog.** That is an arms race against an infinite input
with a finite mesh library, and it can only ever be lost.

> 🔒 **PROPOSED: the refusal IS the content.** Six objects is not a limitation to hide —
> it is *her inventory*. She says it in the shipped line already: **"We don't keep that
> here."** *"That's not in our inventory. This is a workplace, not a wish."*
>
> The meagreness becomes characterization. The player asks the world for what he needs and
> is told, again and again, that his employer does not stock it. That is the whole premise
> of the game expressed as a mechanic — and it costs nothing, because it is what already
> happens. It only needs to be **framed as intended rather than as a miss.**
>
> Which sets up the ending: at the Synthesis, the catalog stops being hers.

This reframe is what makes "junk" the point instead of the problem, and it is the reason
Move 2 matters — she has to have been refusing him all game for the last refusal to land.

### Move 4 — Say out loud what the slot machine is

`NARRATIVE.md` calls the Celestial Fortune coda *"the whole game's thesis in one object."*
The player is never told this.

The cathedral placard should say what Walt says: *I served that floor for years and never
built the machine. I built this one.* The **[T]** panel — closed-form maths, hard and soft
meters, a confidence band — is the evidence standing right there.

**Cost:** placard text. The system is already the most developed thing in the game.

---

## What NOT to do

> 🔒 **No new rooms, mechanics, zones, or machines.** There are already three gambling
> machines, six powers, a cathedral, a retired forest and a parked carousel door. The
> problem is not a content shortage. **Cohesion comes from connecting and cutting.**

---

## Decisions needed

1. **Do the Carousel and video poker serve the thesis?** The slot machine earns its place
   — probability made legible, and Walt's own history. The other two are less clearly
   load-bearing, and poker just became part of the opening. Keep both, fold one into the
   spine, or cut one?
2. ✅ **RATIFIED — Mrs. Hall speaks from Chapter 1.** Design locked in Move 2; she reacts
   to first USE of each power, and every new line uses "Programmer", which puts the name
   denial on story beats instead of only on catalog misses. The existing refusal lines
   keep theirs too.
2b. **Is the six-object catalog her inventory, or a gap to fill?** See Move 3.5. Framing it
   as hers costs nothing and turns the weakest-feeling system into the premise made
   mechanical. Filling it instead is an arms race against arbitrary text input.
3. **Is the ending the memoir sequence, or the power gate?** Currently Ch7 is designed as
   a seven-stage power-gate puzzle. Move 3 proposes the memoir is the emotional climax and
   the gate is the mechanical one. They can co-exist — gate, then messages, then machine —
   but the order needs deciding.
4. **Does the journal keep the lore bible at all?** Proposal is no: the player's journal
   becomes the player's record. The creed and credits move to the itch page, where the
   audience for them actually is.

---

## Implementation order

Each step is separately shippable and separately verifiable.

1. **Move 2 first** — Mrs. Hall speaks from Chapter 1. It is the cheapest change with the
   largest effect, and every other move reads better once someone is applying pressure.
2. **Move 1** — the opening ticket. Small, and it depends on her channel existing.
3. **Move 4** — the placard. Text only.
4. **Move 3 last** — the collection and the finale sequence. The biggest piece, and it
   wants the other three in place to land.
