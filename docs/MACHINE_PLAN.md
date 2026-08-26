# Machine Plan — building software as the plot

*Drafted 2026-08-20, from Walt:*

> "nah. no forest. AI doesn't build forests, you use AI to build machines, and software is
> a machine. I tried to be a programmer for many years. what if you could make building
> software the plot of the game?"

**§8 is ratified (Walt, 2026-08-21): he stopped on GRADER.** The rest of this document
— seven escalating tickets, the cathedral as the machine you ship — is still a proposal.
This supersedes `docs/OPEN_WORLD_PLAN.md` as the direction. That document's §7 test still
stands as a method, but its subject was wrong. A forest costs 10 GB and says nothing. A
machine costs a few meshes and says everything.

---

## 1. The finding: the plot already exists, and there is nothing to point it at

The game's first line of dialogue, delivered in the opening minute, is a **software
maintenance ticket**:

> *"You. Programmer. The legacy system threw again overnight. Fix it before I have to
> explain it to anyone. And you do it by hand — I'm not paying a senior developer to ask a
> machine."*

Now grep the project for what she is talking about:

```
$ grep -rin "legacy" Source/ Content/Data/
Content/Data/MrsHallStory.csv:2   ...the legacy system threw again overnight...
SibeliusGameCharacter.cpp:178     // ...a ticket against the legacy system...
```

**The inciting incident of the game refers to an object that does not exist.** There is no
legacy system. It is named once, in her mouth, and never appears — and five minutes later
the player is watching dancers and playing slot machines. The opening writes a cheque the
game does not cash, and that gap is exactly the boredom Walt described.

The fix is not new systems. It is **giving the six powers something to be about.**

---

## 2. The six powers are the development lifecycle, in order

This is not a metaphor that could be applied. It is what `EPowerVerb` already is:

| Power | What it already does | What it is |
|---|---|---|
| **Code Vision** | reveals the true names of things | reading the code you inherited — what does this *actually* do |
| **Refactor** | changes properties, materials, weight | changing it without breaking it |
| **Compile** | builds new things from gathered parts | building it from parts |
| **Test-Drive** | branch reality ([6]), keep ([7]) or discard ([8]) | *"predict the bug, write the test, write the code"* — the Ch4 cutscene's own words |
| **Deploy** | makes changes stick permanently | shipping to production |
| **Generate** | ask in plain language, it appears | the AI |

The lifecycle was built as a magic system and then pointed at furniture and doors. Point
the same six verbs at a machine and there is a plot.

> 🔒 **PROPOSED: the powers stop being trophies and become tools.** They are currently won
> at slot machines and then rarely needed again — which is why the game reads as
> *"dancers, slots, powers, cathedral, another slot."* A power should be the thing that
> lets you finish the ticket you are holding.

---

## 3. The one rule that makes it playable: never show code

> 🔒 **PROPOSED, and everything else depends on it: SHOW MECHANISM, NOT SOURCE.**

The legacy system is a **machine the player can walk around and open up** — a conveyor, a
sorter, pipes, a thing that stamps something and drops it in a bin. It runs while you watch
it. A bug is **visible**: a jam, a part doing the wrong job, output going to the wrong bin,
a gauge climbing when it should not.

Code Vision does not print source. It **labels the parts with their true names** and shows
which ones are lying about what they do.

This one rule is what separates this from a programming puzzler:

- It is buildable by this project (a machine is a handful of meshes; a code editor is a
  year and a parser).
- It keeps the powers feeling like powers rather than keystrokes.
- It fits the world already shipped — an office, an attic, a cathedral — instead of
  fighting it.
- It is legible to a player who has never programmed, which is most of them.

**If it ever starts to feel like homework, the drift is always toward a text editor. Walk
it back to the machine.**

---

## 4. The four mechanics that carry it

**1. Debugging as detective work.** The log says it threw at 03:00. Code Vision shows
eleven parts. One of them is wrong. The player reasons about *which*, from evidence — not
from a quest marker. This is the actual pleasure of the job and almost nobody has made a
game of it.

**2. Test-Drive is TDD, and it already ships.** Branch reality, apply the candidate fix,
watch whether it holds, then merge or discard. **Predicting the bug before fixing it** is a
mechanic no other game has, and the chapter's cutscene already states the discipline.

**3. Deploy carries consequences.** Ship a bad fix and it breaks in production and Mrs.
Hall finds out. That is the game's stakes, and it needs no combat to work. Deploy already
persists to the save (`BranchSmokeTest`), so the machinery exists.

**4. Generate is the forbidden shortcut — and this is the whole theme.** You *can* ask for
the answer. It is faster. She told you not to. `UGenerateComponent` already has a
`RemainingBudget` (10) and Mrs. Hall already refuses over-budget requests in her own voice.
That is a temptation mechanic sitting fully built and currently spending itself on potted
plants.

> **A game about whether to use AI to do your job, made by a man using AI to make a game.**
> That is where plot, theme, and the fact of this project's own existence fuse — and Walt
> is the only person positioned to write it.

---

## 5. The plot shape: escalating tickets, ending in a machine of your own

| Beat | The ticket | The power it teaches |
|---|---|---|
| 1 | *It threw overnight. Find out why.* | Code Vision — read what you inherited |
| 2 | *Fix it without breaking the rest.* | Refactor |
| 3 | *The part you need does not exist.* | Compile — build from parts |
| 4 | *Prove it before it goes out.* | Test-Drive |
| 5 | *Ship it.* | Deploy — and live with it |
| 6 | *(she forbids this one)* | Generate |
| 7 | **Build one of your own.** | all six at once — the Synthesis |

And the machine built in the last chapter is **the Celestial Fortune, which already
exists** — real par sheet, measured 95.567% RTP, lifetime hard meters, a technician's
panel of closed-form maths (`docs/PAR_SHEET_PANEL.md`, `docs/FLOOR_REPORT.md`).

Today the cathedral is where the player *plays* it. **It should be where the player ships
it.** The [T] panel stops being a curiosity and becomes the proof the machine works.

Which makes the memoir line the ending rather than a placard:

> *"Hey Bally, you could have used this AI skill on the Slot Data System in 2007. I built
> the warehouse and the reports. I never got to build the machine."*

In this game you get to. That is not a theme applied to a career — it is forty years with
the ending fixed.

---

## 6. Demand diagnostics: the gate comes inside the fiction

`docs/NARRATIVE.md` already claims this and the game has never delivered it:

> *"The smoke tests that gate every chapter aren't just QA — in-fiction they're the 'demand
> diagnostics' principle: a thing isn't real until it's been made to prove itself. The way
> the game is built IS what the game is about."*

Make the player run the gate. Their machine has to pass its own test before Deploy will
take it. The fifteen-gate sweep run at the terminal and the check the player runs on their
machine become **the same act**, which is the most complete version of this project's own
thesis that it will ever get.

---

## 7. Nothing built so far gets thrown away

| Already shipped | Its job in this game |
|---|---|
| The six powers | the lifecycle — unchanged, finally used |
| The dancers / AI agents | the pair programmers who hand you each verb |
| Mrs. Hall | the manager, the tickets, the prohibition — she gets *more* to do |
| Refusers | her enforcement, now with something to enforce |
| The Celestial Fortune + [T] panel | the machine you build in Act 7, and its proof |
| Deploy persistence | shipping to production, with consequences |
| Generate's budget + refusals | the forbidden shortcut and its cost |
| Sauce + `FSauceShop` | budget, or story points, or tooling you buy |
| The memoir + Journal | unchanged — it is already the ending |

The cathedral, the office, the attic, the door, the save system, the gates: all keep their
jobs. **This is a re-pointing, not a rebuild.** What is missing is one thing: an object for
the verbs to act on.

---

## 8. The test — one machine, one bug, one day

> 🔒 **RATIFIED (Walt, 2026-08-21):** he held V, looked down the row, and stopped
> on GRADER without hunting. The one-day test passed. The First Ticket (opening
> job in the living room, Kaia for Refactor, ACCEPT closes it) is the next build.
> The seven-act rewrite in §5 is still not this pass — land 1.0 before
> restructuring the chapters.

Do not design seven acts. Do not restructure the chapters. Build **one broken machine**:

1. Something in the office that visibly does the wrong thing while you watch it.
2. **Code Vision** names its parts. One of them is lying.
3. **Refactor** fixes that part.
4. **Test-Drive** proves the fix holds — branch, watch, merge.
5. **Deploy** ships it, and it stays fixed in the save.
6. Play it for ten minutes.

**The question it answers:** is finding a fault in a machine, with your own powers and no
quest marker, satisfying with no story attached? If yes, the whole game reorganises around
it and section 7 says almost everything keeps its job. If no, it cost a day.

This is the same method as `OPEN_WORLD_PLAN.md` §7, pointed at a better subject — and it is
cheaper, because the meshes already exist.

---

## 9. Built: the three instruments (2026-08-21)

§8 passed, and then the machine sat there proving that *watching* is not the same as
*diagnosing*. The first build rejected at the END of the row whatever was broken, ran on
its own timer whether the player was ready or not, and kept no history — so the only
evidence in the level was five label pairs to compare by eye. That is a
spot-the-difference, and it does not scale: five parts is a squint, the eleven this
document asks for in §4 would be a search, which is the exact failure mode listed under
*Risks*.

So the machine reports on itself now, three ways:

| Instrument | What it does | Why |
|---|---|---|
| **It dies where it breaks** | the workpiece stops dead at the first misbehaving stage, that stage's fault lamp lights, and it is diverted to REJECT from there | behaviour finally carries information — a broken INTAKE and a broken GRADER no longer look identical |
| **The line has a transport** | **E** halts it, then steps it one beat at a time (a leg, the jam, the drop); parked between pieces, **E** hands it back | a debugger with no code in it. Also fixes pacing: a free-running cycle was ~7 seconds and the player was a spectator for all of it |
| **It keeps a run log** | the last six cycles with the stage each died at, **pre-filled with the overnight history** | §4 opens with *"the log says it threw at 03:00"* and there was no log. Now the housing shows 03:41–03:46 all failing at the same stage, before the player watches a single cycle |

> 🔒 **PROPOSED — WHERE IS FREE, WHY IS EARNED.** (The rule this build follows. Not
> ratified: Walt has not played it yet.) The lamp and the log
> name the stage and need no power at all; they are the machine reporting on itself, which
> is what Mrs. Hall is reading when she says it threw overnight. Neither ever says what is
> *wrong* with that stage. GRADER's plaque promises "grade B or better passes" and sounds
> completely reasonable — the contradiction is visible only under Code Vision, and
> realising that nothing is better than an A is still the player's own.
>
> Narrowing the search is what a log is FOR. Doing the thinking is not. Every future
> instrument on these machines is measured against that line.

**Consequence for §5.** The row can now get longer without getting tedious, because the
player follows the piece to where it stops instead of auditing every box. That is the
precondition for the escalating tickets — and the transport is the precondition for
ticket 4, because an intermittent fault is the first bug that cannot be confirmed by
watching one cycle, and that is where Test-Drive stops being a debug key and becomes a
power.

---

## 10. Built: ticket 2, the intermittent fault (2026-08-21)

§9 gave the machine instruments. This gives **Test-Drive a job** — the first one it has
ever had. The verb has worked on this machine since the day it was built (a refactor is
`IBranchable`, so branch/merge/discard applied for free) and has never been worth
pressing, because a deterministic fault is confirmed by watching one piece.

**The second job is the same machine, later.** Measured first: `probe_office_floor.py`
found no clear footprint anywhere in the office for a second line — the nearest space
that fits is fifteen metres away and outdoors. Which turned out to be the better answer
anyway. The same legacy system throwing again, differently, is what maintenance actually
is; a second identical crate row in the living room is set dressing.

| | ticket 1 — GRADER | ticket 2 — STAMP |
|---|---|---|
| The fault | a logic error: wrong every cycle | a reliability bug: wrong 34% of cycles |
| The lie | "grade B or better passes" vs *"passes only what is better than A"* | "marks **the** passing blanks" vs *"marks **most** of the passing blanks"* |
| The evidence | a wall of REJECTED AT GRADER | a **mix** in the run log |
| Confirming the fix | watch one piece | you cannot — three good pieces is what luck looks like 30% of the time |
| The verb it teaches | Code Vision, then Refactor | **Test-Drive** |

**Three rules make it work, and each one is a trap avoided:**

1. **The second fault is DORMANT until the first ticket closes** (`ArmedByGrant`). Not
   just the fault — the plaque/true-name disagreement too. A second liar standing in the
   row from the first minute would destroy ticket 1's "find the ONE part that is lying",
   which is the puzzle Walt ratified in §8.
2. **A ticket closes on `AreAllFaultsCleared()`, never `IsHealthy()`.** The first means
   every armed fault has been refactored; the second only means nothing misbehaved on
   *this* cycle. Against a one-in-three fault those differ two cycles in three, and
   closing on the second would hand the player a completed job for a lucky roll.
3. **The fix has to be PROVEN, not just applied.** Ticket 2 will not close without a
   clean test batch. Without that rule the player refactors, gets a good piece, and the
   job closes — and Test-Drive is a key nobody pressed, again.

> 🔒 **PROPOSED — YOU DO NOT TEST IN PRODUCTION.** The test batch runs twenty pieces as
> pure arithmetic and posts one verdict row to the run log, and **E only offers it inside
> a branch**. The reason is diegetic: the tally is Mrs. Hall's production record, and
> twenty experimental pieces down the live line is twenty rejects on it. So the discipline
> is not explained to the player, it is the shape of the only door available.
>
> This is the closest the game has come to §6's promise — the gate coming inside the
> fiction. The player runs a measured check and reads a pass count, which is what the
> fifteen-gate sweep is. Not ratified: Walt has not played it.

**The gate proves itself.** The self-test measures the fault rate over 4000 trials
against a fixed seed, asserts a refactored part fails zero of 500, and asserts the
per-cycle verdict is *latched* — `IsBehaving()` is asked several times inside one cycle,
and a roll living inside it would make the piece jam at a stage the log then denied.
Forcing `FaultChance` to 1.0 was confirmed to turn the gate red with the right sentence.

**Known gap:** Mrs. Hall says the same `Ticket.Closed` line for both jobs. She needs her
own words for the second one — a new row in `Content/Data/MrsHallStory.csv` *and* a
reimport of the `/Game/Data/MrsHallStory` DataTable asset, or she is silent in the package
(the v0.9.4 lesson).

---

## 11. Built: the line is a joke, the labels are not (2026-08-26)

Walt's note, and it is a tone correction to this whole document: *"I want those
machines to look like a joke, to look like a failure. Mrs. Hall is an archaic failure
who oversees projects that are comically bad."* He is right, and the living room was
the last place in the game still playing her straight — Gideon, her demonic enforcer,
already goes down to one slap.

**THE RULE THIS SETTLES.** The machine is a joke. The labels are not in on it. Duct
tape, an overflowing reject crate, a stage that is visibly the wrong equipment — and
bolted to it, a perfectly straight-faced plate reading `GRADER / grade B or better
passes`. Deadpan labels on absurd hardware is funnier than comic labels, and it is the
only version that survives §8: the plaque has to read as *reasonable* or Code Vision
exposing it as a lie is not a discovery. A plaque that is already winking cannot be
caught lying.

**THE BINS ARE THE VERDICT AT A GLANCE.** The tally and the run log are text on a
housing; you have to walk up and read them. Two identical empty crates said nothing.
Now REJECT overflows — heaped on the crate and scattered across the carpet — and
ACCEPT holds a film of grime and nothing else. That is `ACCEPTED 0 / REJECTED 47`
readable from the doorway, and it invents no fiction: the pile IS the overnight throw
Mrs. Hall's ticket is about. The spilled boxes copy their mesh and scale off the
Workpiece component at dress time, so the heap is literally what the machine rejects
and re-dresses itself if `WORKPIECE_MESH` ever changes.

**THE TITLE, AND WHOSE VOICE IT IS.** Walt wanted a big demeaning title —
*Mrs. Hall's Crap Factory*. The trap is voice: Mrs. Hall would never call her own line
that, so a title card saying so is the *game* editorialising over her, and this room
already has one narrator too many. So it is a sheet of card, hand-lettered and taped
crooked over the official brass plate, with both ends of the real name
(`HALL DIVISION - MATERIALS RECLAMATION LINE 4`) still showing past it. Same joke,
except somebody made it: a programmer sat in this chair before you, felt what you are
about to feel, and defaced the sign.

**DEFERRED, THEN BUILT THE OTHER WAY UP (section 12).** The machines are not animals. Walt also proposed each
stage being an animal, the workpiece entering the front and leaving the rear. Held
back for two reasons. It spends Refactor's punchline as wallpaper — turning objects
into animals is the signature power, and if the line is *already* animals then the
goat is not a surprise when the player finally earns it. And the five plaques are the
puzzle: they need a fixed readable height and a fault lamp in a fixed place, which a
row of animals does not have, and §8 warns specifically against the diagnosis becoming
a search.

The version worth building later is the inverse: leave them machines, and let the
*player* refactor a stage into a goat and have **the line keep running**. Box in the
front, out the back, into ACCEPT. Same joke, performed instead of shown, arriving
after the power is earned — and it makes the point about Mrs. Hall's projects far
harder than a sign can. It needs a real answer first for what the run log and the
fault lamp say about a stage that is now a goat.

## 12. Built: a goat can do this job (2026-08-26)

Section 11 parked the animals and named the version worth building: leave them
machines, and let the **player** refactor a stage into a goat with the line still
running. Built that day, once the sign proved the tone worked. Walt chose all three
defaults: gated on ticket 1, random menagerie, and yes she reacts.

**MOST OF IT WAS ALREADY TRUE.** `ALegacyMachine` drives the workpiece to each part's
world LOCATION and never asks what the part looks like. Hide the crate, stand a goat
in its place, and the box travels to the goat, takes its beat and carries on. Nothing
in the machine changed. That is the second time these systems have composed onto a new
subject without modification — section 8 said the same about Test-Drive and Deploy —
and it is worth noticing as a property of the design rather than luck.

**THE GATE IS ONE ACTOR TAG, AND IT ALREADY EXISTED.** `URefactorComponent` skips
interactables when picking a wild target unless they carry `WildRefactorOK`. So closing
ticket 1 tags all five stages, and from then on R rolls the menagerie on them instead
of applying the authored edit. Before that it must not: a stage's authored refactor IS
the GRADER fix, the whole first job. Arm it early and the player can turn the broken
stage into a goat before diagnosing it, leaving the puzzle standing behind an animal.

**ALL FIVE OR NONE.** Gate it per stage — say, only the ones behaving — and the stage
that refuses to become a goat is the answer. That is the same failure section 8 warns
about from the other direction: any signal that separates the faulty stage from the
rest, other than watching it fail, deletes the puzzle.

**THE PLAQUE HAS TO SURVIVE, and that was the only fiddly part.** A stage carries four
static meshes: the crate, plus the three dark plates behind its plaque, true name and
fault lamp. Hiding "the meshes" leaves text floating over nothing. So the crate is
tagged `WildBody` and the transmutation hides only that. *GRADER — grade B or better
passes*, on a brass plate, hanging in front of a goat. Without the plaque there is no
joke, only livestock in the living room.

**THE PAYOFF IS MRS. HALL.** The line keeps producing and she comments on throughput:
*"Something about the line looks different, and I have decided that I do not need to
know what."* Incurious rather than fooled — deliberately. A boss who cannot tell a goat
from a machine is stupid, and a stupid antagonist is not a threat; a boss who declines
to look is the one people have actually worked for. It is her thesis stated by her own
mouth, and the player performed it rather than being told.

Polled from the machine's Tick rather than pushed from the refactor power: R is a
generic verb and has no business knowing this machine exists.

## Risks, honestly

- **Debugging fantasy goes tedious fast.** The mitigation is §3, and the warning sign is
  drift toward text. A player should be able to *point* at the broken part.
- **"Programming game" is a niche shelf.** Mitigated by never showing code — this is a
  game about a machine and a boss, which is a much wider door than TIS-100's.
- **The bug must be findable by reasoning, not by exhaustion.** Eleven parts is a puzzle;
  a hundred is a search. Keep the machines small and the evidence real.
- **Do not build a code editor, a parser, or a scripting language.** That is the failure
  mode this document exists to prevent.
- **This is a re-point of a 0.9.5 game with an alpha in reach.** Same sequencing advice as
  before: run §8 whenever — it costs a day and touches nothing — but land 1.0 before
  restructuring the chapters around it.
