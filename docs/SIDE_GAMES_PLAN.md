# SIDE_GAMES_PLAN — filling the library floor

*Written 2026-07-17 at Walt's request ("the library is very large and there is
room for lots of side games in there"). Companion to BOLD_PLAN.md — this plan
EXTENDS the casino thesis, it does not compete with it: the game is the casino
of one man's memory, and a casino floor needs more than two machines.*

## The house rules (every side game obeys these)

1. **One wallet.** Every game stakes SAUCE and pays SAUCE. No third currency,
   ever. In-game score tokens (like the Carousel's chips/coins) may exist
   INSIDE a machine but never leave it.
2. **Raymond's R5 floor.** Every interaction's worst case still pays something
   (the Carousel's consolation-per-round is the model).
3. **Model before face.** Each game is a headless C++ model with a smoke-gate
   commandlet (the regulator's suite) BEFORE any widget exists. Sim thousands
   of rounds; print the house edge; tune in ONE place. This is how the slot
   (95.4% RTP) and the Carousel were built, and it is why they work.
4. **The proven three-part pattern** (exercised 3x — slot, carousel, trial):
   headless model + gate → native C++ widget face → cabinet actor that opens
   it (UIOnly in, GameOnly out, one close path). No Blueprints needed.
5. **Every machine is a chapter.** Each cabinet gets a placard in Walt's
   memoir voice (see MEMOIR_VOICE.md) — the floor tells the life.
6. **Walt places the cabinets.** Agent spawns them near a trusted anchor or
   hands Walt the mouse (the placement lesson). The library alcoves are his.
7. **Trial odds get simulated, not guessed.** The shrine-stake lesson: sim
   100k attempts and print the win rate before Walt ever touches a stake.

## The menu (ranked by cost; S = ~1 session, M = 2-3, L = 3+)

### G1 — The Daily Carousel  (S; already promised in BOLD_PLAN)
Same machine, one fixed seed per real-world day, one stake, a best-score
board on the wall (lifetime RECORDS tab gets a row). The itch devlog hook:
"today's ride" is the same for every player. No new game code — a seed rule,
a record, a second lever on the existing cabinet.

### G2 — The Wheel of Employers  (S; the memoir machine)
A Big Six / Wheel-of-Fortune upright: one lever, a spinning wheel of EIGHT
segments — SAIC, IBM, Seagate, Motorola, Northrop, San Diego County, Bally,
Army Recruiting. Land on an employer: its memoir line plays (already written,
MEMOIR_VOICE.md) and pays by how badly that job ended (the wheel's paytable
IS the resume: Motorola and Seagate pay best — hazard pay). Trivial model
(one weighted wheel), huge character. The most Sibelius machine possible.

### G3 — Payroll Keno  (S)
Classic 70s casino keno board, reskinned as a data-processing batch job:
pick your numbers ("job codes"), the nightly batch draws twenty, matches
pay. Model is a hypergeometric paytable — the simplest gambling math there
is; the gate can verify RTP exactly, not just by sim. A wall board + number
grid face. Walt processed payroll data for decades; now the payroll pays him.

### G4 — The High-Limit Room  (M)
Celestial Fortune's sibling cabinet, SAUCE-STAKED (the cathedral one stays
free-play; the trials stay trials). New par sheet variant (par sheets become
data the model loads — the Workshop needs this plumbing anyway), higher
volatility, per-spin sauce bet with the slam-stop reels already built. The
first machine where the player risks real wallet on the reels themselves.

### G5 — Video Poker: Jacks or Better  (M)  ✅ BUILT 2026-07-18 (out of order — Walt's pick)
*Shipped in `ee5f1dca` + `e7626414`: full-pay 9/6 model (97.3% RTP simple-
strategy sim), PokerSmokeTest gate (the sweep is 15 gates now), text-card
native face, felt-green cabinet on the library's bookshelf wall, kitchen
door PokerDoor_Kitchen with the four-suits sign arriving at the Poker
doorstep. Established the door-per-game pattern: one library level, one
door + tagged doorstep + proximity-scoped screen per machine. Walt parked
the Workshop's future home in the AI Temple's unused hall.*
The other machine on every real floor Walt's data warehouse watched. Draw
poker model (deal 5, hold, draw, paytable) — well-specified math, perfectly
gate-able, and the hold/draw decision gives players real agency the slots
don't. Face is a five-card row + hold markers; input is 1-5 + Space.

### G6 — Blackjack against Gideon  (L; only if the floor still feels thin)
The dealer is a Refuser in a bow tie who mutters refusals as he deals.
Biggest rules surface (splits, doubles), needs dealer banter to be worth
it — personality is the point or it's just math. Park until G1-G5 breathe.

### THE WORKSHOP (unchanged — the big rock, its own conversation)
Still the kitchen-door finale where the player BUILDS a machine under par
sheet rules. Every game above feeds it: G4 makes par sheets data-driven,
and every extra machine teaches players to read a floor. Show Raymond
BOLD_PLAN.md first.

## Recommended build order

1. **G2 Wheel of Employers** — smallest build, biggest soul; the memoir
   voice gets a machine of its own. Prove the "new cabinet" pipeline on it.
2. **G1 Daily Carousel** — one seed rule + a record; ships with G2.
3. **G3 Payroll Keno** — the floor now has five games; ship as v0.7.9
   ("the library opens its floor").
4. **G4 High-Limit Room** — builds the par-sheet-as-data plumbing the
   Workshop needs.
5. **G5 Video Poker** — the floor is full.
6. Reassess: Workshop next, or G6 if the library still has empty alcoves.

## Placement notes

The library (L_Carousel) alcoves flank the Carousel machine. Cabinet spawn
pattern: agent creates the actor at a trusted anchor offset, Walt drags to
taste in the editor (never remote-place in a dense level; Esc out of PIE
before dragging or the move is lost). Each cabinet: mesh of Walt's choice,
placard TextRenderActor above, glow point light in the machine's signature
color so the floor reads at a glance — cyan trials, gold sauce, one new hue
per game.
