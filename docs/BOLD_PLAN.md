# BOLD_PLAN — five half-concepts become one game

*Written 2026-07-16 at Walt's request, the day he cut the forests. For Walt,
for Opus, and for Raymond when Walt shows him the build. Companion to
APPEAL_PLAN.md (which is about reaching strangers; this is about what the
game IS).*

## The diagnosis Walt made himself

Raymond's GhostCam works because it is ONE loop, polished. Sibelius had five
half-concepts stapled together: a memoir walk, casino machines, a sauce
economy, toy verbs, and the Many Worlds. The forests were the first thing cut
because they were the half-concept with the least reason to exist — big
beautiful places with no purpose. The question Walt asked: what bold concept
saves the rest?

## The thesis: THE CASINO OF LEONARD SIBELIUS

Stop treating the machines as a side room. **The game is the casino of one
man's memory.** A 71-year-old slot designer built his memoir as a house where
every hidden door leads to a machine, and every machine is a chapter of his
life. That is the pitch, the structure, and the ending in one sentence — and
it is TRUE, which is why it beats any invented concept.

The three doors Walt says "make some kind of sense" already ARE this:

| Door | Machine | The chapter it embodies |
|---|---|---|
| Cathedral | **Celestial Fortune** (his real slot, real par sheet) | The career. Luck as a craft. |
| Attic | **Carousel of Fates** (roguelite slot, stake/quota/shop) | Risk. What the floor taught him about people. |
| Kitchen | **THE WORKSHOP** (see below — the bold new room) | Making. The designer's bench. |

## The five half-concepts, made bold

### 1. The memoir walk → THE FLOOR
The house IS the casino floor of a life. Books, six powers, the journal, the
Synthesis finale — unchanged as the spine; it already works and gates
everything. **Bold move (cheap):** each power shrine gets ONE true sentence
from Walt's life when claimed. Six sentences total. The player learns the man
while collecting the verbs. No new systems — text on existing grant events.

### 2. The machines → THE HOUSE ALWAYS REMEMBERS
Both machines exist and pay into one wallet. **Bold moves:** present them as
chapters (a placard by each door, Walt's words, two sentences); the daily
carousel run (fixed seed, one stake, a record to beat — APPEAL point 5b);
keep unifying the furniture (the black-marble sibling cabinets — done).

### 3. The sauce economy → THE ONE CHIP
Already right: earn (books, slaps, curios, temple fountain), spend (cauldron),
risk (carousel). One currency, one wallet, lifetime RECORDS tab. **Bold move:**
nothing new — resist adding a second currency forever.

### 4. The toy verbs → THE COMEDY BETWEEN MACHINES
Slap, wild refactor, the menagerie, corkboard summons, the temple pour. These
are the game's laughter and its GIFs. **Bold moves:** finish APPEAL-6 juice
(death voice lines, P_Death_Gideon particle, MF_DeathFade dissolve, camera
shake — assets on disk, one code point); the zebra-slap GIF goes at the top
of the itch page.

### 5. The Many Worlds → THE WORKSHOP  ★ the bold new concept
The forests are gone and the procedural dream with them. What replaces the
kitchen door is the one room only Walt Parkman can design: **a slot-machine
workshop where the PLAYER builds a machine.**

- Pick symbols from a shelf (SlotFactory symbol sprites exist).
- Set reel weights with physical levers; a live **par sheet** on the wall
  updates hit frequency / RTP as you pull them — the game TEACHES what a par
  sheet is, through play, from a man who wrote real ones.
- The house enforces house rules (RTP bounds) — push greed too far and the
  machine is "unlicensed" and won't run; too generous and the house buys it
  from you cheap. The tension IS the lesson.
- Then you PLAY your own machine, with your sauce.
- Assets: `SlotFactory/L_SymbolStudio` exists as the seed level; the slot
  sim (FCarouselSim / USlotGameModel) is headless-tested and reusable.
- The kitchen door (ex-Many Worlds, deck now empty) is the future Workshop
  door. Until built, it stays retired.

Nobody else on earth ships this room. Webfishing cannot. This is the clever
gameplay concept Walt was looking for, and it was his biography all along.

## What was cut (2026-07-16, `ea54e884` + `4a891d91`)

Four baked forests + Poplar + the Elsewhere winter forest, EasyBiomes and
four PN foliage packs (~19 GB working tree; ~5 GB off the next download),
Shinbi companions (code + pack). Curio/Cabinet + Elsewhere builder code
remains, dormant — the Workshop or a future Elsewhere may revive it.

**SHIP BLOCKER:** the office kitchen SauceDoor still lists deleted forest
maps in its TravelTargetLevels deck — it must be retired/disabled in
L_Office_v02 before the next cook, or E on it soft-crashes a packaged build.

## DONE 2026-07-17: dress up the native slot face (Walt-approved 2026-07-17)

*Shipped as speced below, all in SlotScreenWidget — plus: Space mid-spin
slam-stops the reels, Esc mid-spin settles the result before closing (a win
can never be eaten), sounds are PROCEDURAL C++ PCM (no slot sound assets
existed; synthesized whir/tick/sting/fanfare can never be missed by the
cooker), and PowerGrant now holds 1.6 s after a trial win so THE MACHINE
YIELDS banner + fanfare land before the claim ceremony.*

The native Celestial Fortune (USlotScreenWidget, the plain sprite grid) gets
the showmanship of the web build. This face runs the shrine TRIALS today and
is the only possible face for the WORKSHOP later — polish here serves both.

Build, in order (all in SlotScreenWidget.cpp — model untouched, "reels are
theater, model is law"):
1. **Spinning reels**: each reel = a vertical strip of the sprite art
   (/Game/SlotFactory/SymbolSprites) scrolling fast with eased deceleration,
   staggered stops left-to-right, small bounce on stop. Replace the current
   dim-then-reveal.
2. **Win presentation**: payline glow overlays, credits COUNT UP instead of
   jumping, win text pulse; free-spins banner moment (x3 celebration).
3. **Sound**: spin whir, per-reel stop tick, win sting, bonus fanfare — check
   /Game/Audio and the SlotFactory for existing assets before importing.
4. Keep TRIAL mode intact (SetTrial/OnTrialWon, hint lines, THE MACHINE
   YIELDS) — it must survive the facelift; SlotSmokeTest guards the model.
Reference for the target feel: the web build in the cathedral cabinet.

## The order to build it (when budget exists)

1. ~~Retire the kitchen door~~ DONE (ship blocker cleared, `d01af837`).
2. ~~Ship v0.7.8~~ DONE 2026-07-17 (build #1804147): the small download + the
   memoir voice + the shrine trials with the dressed-up native slot face.
3. ~~Six memoir sentences at the shrines + two placards~~ DONE (MEMOIR_VOICE.md).
4. Slap juice (APPEAL-6 remainder: death voice lines, P_Death_Gideon,
   MF_DeathFade dissolve, camera shake — assets on disk, one code point).
5. Daily carousel (fixed seed, one stake, a record to beat).
6. THE WORKSHOP — the big rock, its own design conversation. Show Raymond
   this document first; his GhostCam instincts sharpened on exactly this
   question of one-loop clarity. The native slot face it needs is now built.
