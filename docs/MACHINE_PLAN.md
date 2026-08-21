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
