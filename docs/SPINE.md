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

### 4. The journal hands the player the design document

`UJournalWidget::RefreshFromNarrative()` loads `docs/NARRATIVE.md` and displays it.

Pressing **J** shows the player the lore bible — including the meta sections about Claude,
Fable 5, EasyBiomes credits and the company creed. That is the "plot lives in a document"
problem made literal: the game's answer to *what is going on* is to show you the notes.

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
>
> This is already the design. It is written in two documents and one header comment. The
> game never once dramatizes it — the player is never called "Programmer," never denied a
> name, and never given one.

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
2. **Does the player get called "Programmer" out loud?** It is the sharpest way to stage
   the name arc, and it costs one word per Mrs. Hall line — but it means she must speak
   often enough for it to register.
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
