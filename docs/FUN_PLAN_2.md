# FUN_PLAN_2 — the verbs need jobs, the city needs a banner, the game needs an end

*Written 2026-09-05 by Claude (Fable 5.1) at Walt's request, the day 1.4.0 shipped and
the game got its ending. This is the handoff copy for Opus 5 (or any model). **Nothing here
is ratified.** Same process as `docs/SPINE.md`: doc, ratify item by item, then implement.
The sequel to `docs/FUN_PLAN.md` (2026-07-12), which was written before the cathedral
finale, the meadow, the city, the spaceport or Grok existed.*

**How this was produced, so the next model knows what to trust.** Every claim below about
what the game DOES was checked by reading the shipped code and docs, with file anchors.
Nothing was checked by playing — Walt's playtests are the only play data in this project,
and they are quoted where they exist. Read the anchors before building on a claim.

**REVIEWED 2026-09-05 by Claude (Opus 5), before any of it was built.** Every claim in
Part A and Part B was re-checked against the code. All of them stand except two, corrected
in place below and marked **CORRECTION (Opus 5)**:

- **A4 was too soft.** She does not merely linger after the launch — she goes on giving an
  instruction that is no longer true, and the fix is a stage rule rather than a hide call.
- **B1's build order was wrong.** The Deploy ticket was put first on a cost guess that the
  code contradicts in a comment. Compile goes first, and decision 6 dissolves.

The two items the first draft flagged as read-not-walked (A4, A5) are both answered in code
now. A4's walk-back is confirmation, not investigation.

---

## 0. Handoff — read this before doing anything

**State of the build.** 1.4.0 is live on itch (build #1950662, pak 6.71 GB, all 30 cook
checks green). The route is playable end to end: office → cathedral → meadow → city →
spaceport → portal → Grok → Nyra's apology → *"Good job, Leonard."* Then nothing.

**Walt's focus is Steam.** Store page must be public by **11 September** for a
**25 September** release (`docs/STEAM_PLAN.md` §1b). KYC is out of everyone's hands.
Screenshots (≥1920×1080, 16:9) are his to capture. Anything in this doc marked *before the
25th* competes with that for his time — it should be small, and it should be things a
stranger will hit in the first hour.

**Walt.** 71, learning Unreal, wants small numbered steps and the *why*, pastes screenshots
back, budget-constrained. Never `git add -A` — stage exact files. He says "closed" when the
editor is closed for a build.

**The standing rules** (each one was paid for; the file that records it is named):

- Build with the editor **closed** for any reflection change; Live Coding patches bodies
  only and is **lost on editor restart** — follow a runtime fix with a full build.
  `& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" SibeliusGameEditor Win64 Development -project="C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" -waitmutex`
- Gates: headless commandlets, `-run=<Name>` with `UnrealEditor-Cmd`, names in
  `Source/SibeliusGameEditor/*Commandlet.h`. Run the relevant ones after every change;
  all of them before a ship. Quote `"-run=$g"` if you loop them in PowerShell.
- **`L_Office_v02` is only ever saved by a healthy interactive editor**, never via the
  save-on-close prompt (twice corrupted `URefactorableComponent`s). Recovery is
  `git checkout -- <umap>` + a gate re-run.
- **Soft references do not cook.** Gitignored vendor content (`Content/PortalVFX/`,
  `Content/Elite_AlienPack_04/`, `Content/MorroMotion`, the MetaHumans) reaches the pak
  ONLY through a hard reference (`FObjectFinder` on a CDO) or a level's own references.
  A soft path works in PIE and is missing from the shipped build — that is the v0.7.4
  invisible spaceport, and it nearly happened again on the way to Grok.
- **Only maps in `MapsToCook` ship** (`Config/DefaultGame.ini`). `OpenLevel` on a missing
  map is a silent no-op.
- **Travel loads from disk.** An unsaved level edit is invisible to a playthrough that
  arrives by travel. `Tools/Scripts/place_grok_arrival.py` saves `L_Grok` itself for this
  reason. `L_Grok.umap` is gitignored (647 MB of Velarion's terrain).
- **Two editors on one project** lock each other's assets and take Save All down. Check
  Task Manager before blaming the SSD.
- `unreal.Rotator` in Python is `(roll, pitch, yaw)` — **always kwargs.** Python has no
  `/* */` comments; grep for it before handing a script over.
- **Edit these docs with the Write/Edit tools, never PowerShell regex** — it mangles the
  em-dashes.
- Release runbook: `docs/sib-42-packaging-notes.md` §Release; mirror scripts
  `Tools/Scripts/package_v*.ps1`; butler channel
  `leonardsibelius/leonard-sibelius:windows --userversion X.Y.Z`. Work is not done until
  `git status -sb` shows no ahead/behind and the build is verified.

---

## 1. What the player actually gets — the route, traced in code

**Act 1 — the office** (`L_Office_v02`)

1. Kaia's cutscene: *"Hello, Leonard."* An AI grants the name Mrs. Hall will refuse all game.
2. Ticket 1. The legacy system is throwing. Hold **V**, read the parts, one is lying
   (GRADER: *"grade B or better passes"*). **R** is not yours — find the AI agent, press
   **E**, win a Celestial Fortune trial (2100 → 2250 credits, ~62% per attempt, a miss
   deals a fresh stake — `PowerGrant.h:125–142`), take the power, come back, fix the liar.
3. Ticket 2. Intermittent. Needs Test-Drive: another agent, another trial; **V** to find the
   word that changed, **R**, **6** branch, **E** test, **7** merge. Ticket closed.
4. Around that: poker through the glass, books (5 sauce), slapping Refusers (2 sauce),
   the cauldron shop (`SauceShop.h`: powers 150, budget 40, slap 60), the Carousel of Fates
   (stake 50), the AI temple, video poker, the menagerie (R turns furniture into animals),
   the clue terminals.
5. Books → the attic key orb → Elise gives Compile → **C** builds the ladder → attic →
   cathedral door.
6. Cathedral. Altar: show the six verbs in order. Wall falls. Mrs. Hall's `Final` line. The
   eight memoir messages, ~12 s each (`FinaleAltar.cpp` `AdvanceClosingSequence`). The
   Celestial Fortune, free play. 5,000 credits coin-out opens a door that was never there.

**Act 2 — the meadow** (`L_Meadow`)

7. 150–400 Architects on the ridge. *"You gave us somewhere to stand. Here is a body that
   can reach them."* Battle form: the first time you see Leonard. Slap them all; enough of
   them for long enough and you are OVERRULED, not killed. Victory: *"AI has set you free.
   More adventures coming soon."* Then `[O] the office / [>] somewhere you have never been`.

**Act 3 — the city** (`L_City`, `L_Cafe`, `L_uFoods`, `L_Grok`)

8. Nyra in the plaza: the deli is behind her. In, out; she is outside the door:
   *"Stand on the grass, and Generate."*
9. **G**, type `spaceport`, a launch complex assembles 160 m out. She is at the lawn's
   edge: go to uFoods for supplies.
10. uFoods: 1,956 meshes, one **E** at the counter (`SupplyCounter.h` — Walt chose this
    over shopping, rightly). Out; she is on the sidewalk: *"I will upload myself into the
    spaceship computer and I will be going with you!"*
11. Back to the spaceport. **C** boards (a camera in the cabin; the pawn never moves).
    **C** launches. Cutscene. *"THE SHIP IS AWAY. NYRA WILL CALL FROM GROK."* Seven
    seconds later (`Spaceport.h:543`), among the blue ghosts: *"A WAY HAS OPENED WHERE THE
    SHIP STOOD — PRESS C TO GO THROUGH."*
12. **C** → `L_Grok`. Wormhole arrival (Niagara, fade from black, 7 s, skippable). Nyra on
    the hillside. **E**: the apology, in her voice, lips moving. *"Good job, Leonard."*
    End — see A2.

Playtime for a stranger, guessing from the route: 45–90 minutes if they are not lost.
Whether they are lost is Part A.

---

## 2. The finding

> **The verbs are won, not used.** Walt's own sentence in `docs/OPEN_WORLD_PLAN.md` §1 is
> still the truest one about the game: *"the six most interesting verbs in the project are
> things you win and then rarely use."*

Count the uses after ticket 2: Code Vision finds hidden doors. Refactor is a toy (the
menagerie — a good toy). Compile builds one ladder and boards one rocket. Test-Drive is
used once, on ticket 2, and the HUD comment says so: *"a key with nothing to do since
Chapter 4."* Deploy is **[0] at the altar and nowhere else.** Generate spawns seven catalog
rows (`Content/Data/GenerateCatalog.csv`), one of which is the spaceport, which is the only
time Generate is awe.

**The legacy machine is the exception, and it is the model.** Ticket 1 and ticket 2 are the
most fun ten minutes in the build because there, V, R and 6/7 are *diagnostic tools* on a
subject that misbehaves in front of you — not keys, not trophies, not items on a checklist.
`docs/MACHINE_PLAN.md` §5 already lays out seven escalating tickets. Two are built.

Everything else the game does well is **story**, and it is genuinely good: the name arc;
the slap (no damage — her enforcer falls over); seeing yourself for the first time as the
army comes; a word that builds a launch complex; *"A rocket is a body's way of going
somewhere. I am not a body."* The play under the story is mostly *go where she says, press
E.*

**What works and must not be touched:** Kaia's opening; the two tickets and the three
instruments; Mrs. Hall speaking from Chapter 1 (SPINE Move 2, ratified and built); the
slap; the menagerie; the Architects and OVERRULED; the spaceport assembly; the boarding as
a camera, not a capsule; the portal among the ghosts (*"it implies that ghosts use it"*);
the wormhole; the speech. The Celestial Fortune and the trials stay — see B3.

---

## 3. Part A — before the 25th

Five items. Each is where a stranger stops having fun, and each is small. They are in
order of value.

### A1 — The city has no objective banner

**Now.** `ASibeliusHUD::ComputeObjective()` (`SibeliusHUD.cpp:539`) returns an empty
string at line 654 the moment `Finale.Synthesis` is claimed — *"Synthesis done: free play,
no nagging."* That was correct when the game ended at the cathedral. There are now four
levels after it, and in every one of them the only guidance is a 7-second voice clip
(`GreetingSeconds`) and transient toasts. Walt, on his own game, 2026-09-05: *"the 'C' to
board message popped up and disappeared too quickly... I had to remember to press 'C'
because there was no prompt."* He had the developer on call. A stranger has nobody.

`docs/APPEAL_PLAN.md` point 2 fixed exactly this for the first five minutes and Walt said
the game started to feel better the same day. This is the same fix for the last fifteen.

**Change.** A location branch at the top of `ComputeObjective`, BEFORE the all-powers
early-out. The function already strips the PIE prefix and matches map names (`Cathedral`,
`AI_Temple`), so the pattern exists. Every condition below is state that already exists —
no new saved bools; "does a spaceport stand" is `TActorIterator<ASpaceport>`, the
`AHintVolume` rule (right after a load, after a discard, and on New Game — a bool gets all
three wrong).

| Where | Condition | Banner (draft — Walt's words win) |
|---|---|---|
| `L_City` | no `City.Deli` grant | *Nyra is in the plaza — [E] to talk. The deli is behind her.* |
| `L_City` | `City.Deli`, no `ASpaceport` in the world | *The empty lawn across the street. Press [G] and ask for a SPACEPORT.* |
| `L_City` | spaceport stands, no `City.Supplies` | *Supplies first. uFoods is down the block — [E] at the counter.* |
| `L_City` | `City.Supplies`, not aboard | *Back to the spaceport. [C] to board.* |
| `L_City` | aboard, not launched | *[C] to launch.* |
| `L_City` | `HasLaunched()`, portal not open | *The ship is away. She said she would call.* (see A5) |
| `L_City` | `IsGrokPortalOpen()` | *A way has opened among the ghosts. [C] to go through.* (A5c changes this) |
| `L_Grok` | talk not yet given | *Nyra is waiting. [E].* |
| `L_Grok` | after the talk | empty |

Walt's rule from the boarding playtests, verbatim: *"please don't say Press C at the pad
to board... just say Press C to board."* The pad is 22 m from the apron and a fence is in
the way. Never name the pad.

`ASpaceport::FindForPlayer`, `HasLaunched()` (`Spaceport.h:325`) and `IsGrokPortalOpen()`
are public. `ASibeliusGameCharacter::HasVisitedDeli` exists. The meadow needs nothing —
`ABattleArrival` draws its own text.

**Cost.** Half a day. **Gate.** None exists for the HUD; the check is a fresh **NN**
playthrough by Walt, reading only the gold line. **The 4K rule applies** — `DrawObjective`
already scales; keep the lines short enough to fit at 1.3×.

### A2 — The game does not end; it stops

**Now.** On Grok, `UDancerAgentComponent::EndTalkShot()` (`DancerAgentComponent.cpp:778`)
releases input after her line, and that is the last thing the game does. `docs/SEVENTH_POWER.md`
rev 5 designed the ending — *"She says 'Good job, Leonard.' Then the messages roll, 1988 to
2022"* — and it is unbuilt. There is no credits screen anywhere in `Source/`. The five
artists are credited on the itch page and the Steam draft and never in the game.

A game that stops gets *"is that it?"* reviews. This one has an ending written and not
staged, which is the SPINE finding all over again.

**Change.** When the stage-4 talk ends, ONCE (claim a grant, `Grok.Ending`, so a second
**E** on her does not roll the credits twice), run a closing sequence:

1. Her line finishes; hold a beat on her face.
2. The eight messages, one at a time — `AllMemoirMessages()` (`ProgressionTypes.h:59`)
   through `HUD->ShowMemoir`, exactly as `AFinaleAltar::AdvanceClosingSequence`
   (`FinaleAltar.cpp` ~229–272) already does, with the same dwell constants. That code is
   proven and it is the same eight strings.
3. A credits card: the title; *Walt Parkman, with Claude*; the five artist lines from
   `docs/STEAM_PLAN.md` §3 (xAndrei, Morro Motion, Jacob Norris / PurePolygons, QuadArt,
   PackDev — and Velarion, Dr.Game, Ultima Store, twins-creators, EasyBiomes belong here
   too; check `README.md` Credits and `docs/VENDOR_PACKS.md`).
4. The two doors the meadow already offers, reused: `[O] the office` and **NN** new game.

**Where.** A small placed actor (`AGrokEnding`), placed by `place_grok_arrival.py` next to
the wormhole, NOT another branch inside `UDancerAgentComponent` — that file is about
talking and is already the size it is. The component needs one thing it lacks: a multicast
`OnTalkEnded` fired from `EndTalkShot`, so the ending can listen. The HUD's memoir and toast
drawing can carry the whole sequence; the finale proves it. No new widget class.

**Cost.** One day. **Decision needed:** does the altar's read-back stay as well? Rev 5 asked
(*"two showings may be one too many"*). Recommendation: **keep both.** Many players will
stop at the cathedral; the altar is the Bally ending, Grok is the life ending, and the
words are the same because the life was.

### A3 — "More adventures coming soon" plays at the midpoint

**Now.** `BattleArrival.h:99`, `ComingSoonLine`: *"More adventures coming soon."* Written
when the battle was the end. It now promises a sequel from the middle of the story, and
the store page will be read against it.

**Change.** The line that follows it already does the work — `[>] somewhere you have never
been`. Either drop the second sentence or make it true: *"AI has set you free. There is a
city."* **Cost.** Minutes. **Decision:** the words are Walt's.

### A4 — Nyra is in two places at once, and the second one is still giving orders

> **CORRECTION (Opus 5, 2026-09-05).** The first draft said she lingers, and proposed
> hiding her. Both halves were too soft. It is not that the code forgot to remove her — the
> stage function has no concept of the launch at all, so she keeps *working*.

**Now.** `UDancerAgentComponent::GuideStage()` (`DancerAgentComponent.cpp:995`) returns
**3** whenever `ASupplyCounter::HasSupplies` is true (`:1031`), and **nothing in the
function looks at whether the ship has gone.** Stage 4 is the Grok level; stage 3 is the
supplies; the launch is invisible to both. `ASpaceport::EndLaunch` toasts and opens the
portal, `UDancerAgentSubsystem::RestageGuides()` is called only from `OnGeneratedFresh`
(`Spaceport.cpp:738`) and never from the launch, and there is no hide or destroy path
anywhere tied to launching.

So after the ship is away, walking back to the uFoods sidewalk finds her dancing and saying
`GuideLine4`:

> *"We are ready to go to Grok! I will upload myself into the spaceship computer and I will
> be going with you! ... Go back to the spaceport and we will do the boarding procedures."*

That is not a continuity nit. It is a **stale instruction pointing at an empty pad**, in
the one moment the betrayal is supposed to land, spoken by the character who committed it —
and the player most likely to walk back is the one who is lost, which until A1 ships is
every player. He is told to go and board a rocket that is gone.

**Change — a stage rule, not a hide call.** `GuideStage()` gains a launch test above the
supplies test. Find the spaceport the way `AHintVolume` finds things
(`TActorIterator<ASpaceport>`, **never a saved bool** — right after a load, after a
Test-Drive discard, and on New Game, all three of which a bool gets wrong), and ask
`HasLaunched()` (`Spaceport.h:325`), which is public and lives on the branch state, so it
survives the reload of `L_City` that every door causes. Grok's Nyra is a separate placed
actor in a different level and is untouched by any of this.

What that stage DOES is the decision, and there are two candidates:

- **She is gone.** Hidden, collision off, talk disabled. An empty pad and an empty sidewalk
  are the betrayal on screen with no words needed. Cheapest, and it is what rev 1 described.
- **She is gone and the street says so.** The same, plus A1's banner reading *"The ship is
  away. She said she would call."* — which is also A5(a)'s trigger, so those two items want
  building in the same session.

**Cost.** Small, and smaller alongside A5(a). **Gate.** Hand test: launch, walk back.

### A5 — Let the betrayal breathe, and let V open the last door

**Now.** The beat is: *"THE SHIP IS AWAY. NYRA WILL CALL FROM GROK."* (6 s) → seven seconds
→ *"A WAY HAS OPENED..."* The player never has a moment of being left. Rev 1's deli-ghost
line (*"Nyra stole your spaceship"*) was designed and dropped; `docs/SPACEPORT_PLAN.md`
"The city reacts" — *the AI ghosts have ignored him since he arrived; the launch is the
first thing that makes them stop* — was planned and never built. The portal ended up among
the ghosts by accident and Walt liked it there. The pieces are on the table.

**Change — three parts, separately optional, recommended in the order c, a, b.**

**(c) The way is invisible to a body.** The portal Niagara component is hidden until Code
Vision is held: `UCodeVisionComponent::OnCodeVisionChanged` (`CodeVisionComponent.h:43`),
the exact pattern `AHiddenDoor` uses (`HiddenDoor.cpp:126`). Once seen it STAYS seen — a
player who must hold V while pressing C is a bug, not a puzzle. Banner: *"Hold [V]. The
way the ghosts travel is not visible to a body."* **Why this one first:** Code Vision opened
the first door in the game (the glass at spawn, `Tutorial.Vision`), so it should open the
last; it is the single cheapest way to make a verb *matter* in Act 3; and it rhymes with
her line — *he came the way a body cannot see.* One trap: `PreflightCompile` gates the
travel on `GrokPortal != nullptr`, so a spawned-but-hidden portal would still answer C —
gate it on a `bGrokPortalRevealed` as well.

**(a) The way opens when he has looked for her.** Not on a timer. After the launch the
banner says *"She said she would call."* He walks to where she stood (the `uFoodsStreet`
PlayerStart — the same marker the guide uses, one `TActorIterator`), finds nobody (A4), and
as he turns back the way opens. Proximity to the marker is the trigger; the 7-second timer
becomes the fallback if he never goes.

**(b) One ghost turns.** At the moment the way opens, the nearest blue ghost faces him.
`UDancerAgentComponent` already has freeze-and-face (the talk shot's yaw); if the ghosts
are not dancers, it is a yaw. No recorded line before the 25th — a toast is enough:
*"THE GHOSTS HAVE NOTICED YOU."*

**Cost.** Half a day each. **Decision:** which of the three.

---

## 4. Part B — after 1.0

### B1 — Tickets 3, 4, 5: the verbs get jobs

This is `docs/MACHINE_PLAN.md` §5, beats 3, 5 and 6 — not a new idea, the ratified one
finished. It is the structural answer to the finding in §2, and it needs no open world, no
new level and no vendor pack: the subject already exists and works, with per-part faults,
intermittent faults, branch/merge/discard and deploy persistence composing onto it *"without
modification"* (`LegacyMachine.h`, the header's own result).

**Build them in MACHINE_PLAN's own lifecycle order: Compile, Deploy, Generate.**

> **CORRECTION (Opus 5, 2026-09-05).** The first draft put the **Deploy** ticket first as
> the cheapest, and added "check first whether merge already persists the machine's fix."
> It does — **deliberately, with the reason written down.**
> `ALegacyMachine::MaybeRestoreClosedTicket` re-applies the fix on BeginPlay keyed off the
> ticket grant: *"THE JOB STAYS DONE without Deploy"* (`LegacyMachine.cpp:436`) and *"A
> closed ticket stays closed across a reload even without Deploy, **which the player may
> not own yet**"* (`:990`). That is an anti-soft-lock decision, and a regression ticket has
> to fight it.
>
> Not fatal: the restore loop already discriminates faults by `ArmedByGrant` (`:1001`), so a
> ticket-4 fault can opt out of the restore while tickets 1 and 2 keep it. But that is
> surgery on a deliberate rule rather than the cheap win it was billed as, and the ticket
> cannot ship before Deploy is reliably obtainable or it soft-locks exactly as the comment
> warns. **Compile collides with nothing and goes first.**
>
> The happy result: cost order and MACHINE_PLAN's lifecycle order now agree, so **decision 6
> in §6 is no longer a decision.**

| Ticket | Verb | Mrs. Hall | Why |
|---|---|---|---|
| **3** | **Compile** | *"A part is missing. Find one."* | The workpiece falls through a gap where a stage should be. An `ABuildSite` on the row, paid in books — the ladder's mechanism, on the machine. Proven, and it fights nothing. **Start here.** |
| **4** | **Deploy** | *"You fixed it yesterday, Programmer. It threw again at 03:00."* | The fix does not survive the night unless it is **Deployed [0]** — the only ticket that uses a mechanic used NOWHERE but the altar, and the funniest line in the set. Costs the `MaybeRestoreClosedTicket` surgery above: this ticket's fault opts out of the grant-keyed restore, and the ticket must not arm until the player can actually own Deploy. |
| **5** | **Generate** | *"We don't keep that here."* | The part is not in her inventory. He types its name; she refuses in her own voice (the shipped `NoMatch` lines); the AI agent supplies the catalog row. This is MACHINE_PLAN §4.4's *temptation mechanic* and SPINE Move 3.5 (*the refusal IS the content*) finally landing on the plot. The climax, so it goes last. |

**What this does to the slot trials.** They stay as the acquisition — but the ORDER flips.
Today the trial comes first and the need never arrives. With a ticket open, the player goes
to the agent because he needs the verb *right now*; the trial reads as *go get the tool*,
not *gamble for progress*. That is lock-and-key, and it is what `OPEN_WORLD_PLAN` §4 wanted,
without the world.

Rows go in `Content/Data/MrsHallStory.csv` (Reasons `Ticket`, `Ticket.Closed`,
`Ticket.Livestock`, `Ticket.Architects`, `Ticket.Cathedral` already exist — add
`Ticket.Regressed`, `Ticket.Missing`, `Ticket.NotStocked`). Objective lines in
`ComputeObjective` alongside the existing ticket block (`SibeliusHUD.cpp` ~575–630).
Grants beside `Ticket.Legacy.Closed` / `Ticket.Legacy.Intermittent.Closed`
(`LegacyMachine.cpp:20–21`).

**Cost.** 2–3 days each. **Gate.** The machine's existing commandlet, extended per ticket —
the gate-inside-the-fiction rule (MACHINE_PLAN §6).

### B2 — The altar rite is a checklist

Show six verbs in order. It is a key sequence; the machine shows what it could be. **Not
proposed for change before 1.0** — after B1, the rite could ask for the six verbs *on the
machine*, but that is a rewrite and it is parked.

### B3 — The trial is ceremony, and that is fine

2100 → 2250 credits, ~62% per attempt, a miss deals a fresh stake. It is a delay, not a
decision. It is also the thesis object, Walt's own history, and the reason a Steam reviewer
will find the game strange in the good way. **Do not remove it.** Once B1 makes the player
want the verb, the ceremony has a reason to precede it.

### B4 — Sauce is a score

50 to start (`ProgressionTypes.h:140`), books 5, slaps 2, powers 150, Carousel stake 50,
supplies priced not to block. One real sink. Acceptable for a game of this length; not a
fun problem worth a session.

### B5 — A second word for Generate in the city (optional)

The spaceport proved what Generate is: a *word* that builds a *system*, with a guide's
reaction. The `ActorClass` column in `GenerateCatalog.csv` (Phase A) makes a second word
plumbing-free. The cost is assets, and every vendor pack is a cook risk — only if something
suitable is already on disk. Do not buy for this.

---

## 5. What NOT to do

- **No new levels, packs, or mechanics before the 25th.** Part A is text, one small actor,
  and wiring to things that exist.
- **Do not put the ending inside `UDancerAgentComponent`.** One delegate out; the ending
  listens.
- **Do not remove the trials or the cathedral machine** (B3; `OPEN_WORLD_PLAN` says the
  same).
- **Do not revive the forests or the open world** for this. `docs/FORESTS_DO_NOT_USE.md`
  is in force; `OPEN_WORLD_PLAN` §8's own warning stands — *finish the spine, ship 1.0.*
- **Do not name the pad** in any banner (A1).
- **Do not trust a timer for the betrayal** if (a) is built — the beat is him looking.

---

## 6. Decisions needed — Walt's

1. **A1 wording.** Nine lines drafted above; his words win. Ratify the chain, then the words.
2. **A2: keep the altar read-back as well as the Grok roll?** Recommendation: keep both.
3. **A3: the replacement line.** *"AI has set you free. There is a city."* or drop the sentence.
4. **A4: what does stage 3 become after the launch?** She is simply gone, or gone with the
   banner saying *"The ship is away. She said she would call."* Recommendation: the second,
   built with A5(a), which needs the same trigger. The code question is settled; the
   walk-back is confirmation.
5. **A5: which of (c), (a), (b).** Recommendation: all three, (c) first.
6. ~~**B1 order.**~~ **Resolved by the Opus 5 correction** — cost order and MACHINE_PLAN's
   lifecycle order agree once the Deploy ticket's real cost is known: **Compile, Deploy,
   Generate.** Nothing left to decide.
7. **Does anything in Part B start before the 25th?** Recommendation: **no.** Screenshots
   and the store page are the critical path and they are his.

---

## 7. Implementation order and cost

| # | Item | Cost | Ships in |
|---|---|---|---|
| A1 | City objective banner | ½ day | 1.4.x — before the Steam build |
| A3 | "More adventures" line | minutes | with A1 |
| A4 | Nyra's stage after launch | small | with A1 |
| A2 | The ending: memoir roll, credits, doors | 1 day | 1.5.0 |
| A5c | V reveals the way | ½ day | 1.5.0 |
| A5a | The way opens when he looks for her | ½ day | 1.5.0 (with A4) |
| A5b | One ghost turns | ½ day | 1.5.0 |
| B1-3 | Ticket 3 — **Compile** | 2–3 days | after 1.0 |
| B1-4 | Ticket 4 — **Deploy** (+ the restore surgery) | 3–4 days | after 1.0 |
| B1-5 | Ticket 5 — **Generate** | 2–3 days | after 1.0 |

A1 + A3 + A4 together are one session and they are the difference between a stranger
finishing the game and a stranger quitting on a sidewalk in Trans Human City. Do those,
then screenshots, then A2.

---

## Related

`docs/FUN_PLAN.md` (the first one, 2026-07-12) · `docs/SPINE.md` (the process this follows)
· `docs/MACHINE_PLAN.md` §5 (the tickets — B1 is that, finished) · `docs/OPEN_WORLD_PLAN.md`
§1 (the diagnosis) · `docs/SEVENTH_POWER.md` rev 5 (the ending, and what is still open) ·
`docs/SPACEPORT_PLAN.md` "The city reacts" (A5b) · `docs/APPEAL_PLAN.md` point 2 (A1's
precedent, and the handoff rules) · `docs/STEAM_PLAN.md` (the dates)
