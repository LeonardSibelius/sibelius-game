# THE TECHNICIAN'S PANEL — editing the par sheet in-game

*Design doc. Nothing is built yet. **All decisions locked with Walt on 2026-08-05** — the
🔒 blocks record what was chosen, what was rejected, and why. Ready to implement.*

**The constraint that outranks everything else:** this feature must **teach**. Walt has
never designed a slot machine — he ran the data warehouse that archived play and produced
the accounting reports — and he wants the panel to teach him as well as new players. When
a choice is between "more powerful" and "more legible", legible wins.

**One line:** the cathedral slot machine has a service door. Give the player the key.

---

## Which machine (settled, because the docs disagreed)

**`ASlotCabinet` in `L_Cathedral`.** Verified by reading the levels, not the docs:

| Level | Machines |
|---|---|
| **L_Cathedral** | **`SlotCabinet`** ← this one · `FinaleAltar` |
| L_Carousel | `CarouselMachine`, `PokerMachine` |
| L_Office_v02 | `PowerGrant` ×4 — no slot machine at all |

Three things this does **not** touch:

- **The Carousel of Fates** runs `CarouselSim`, a different model with different math.
- **Video poker** runs `PokerGameModel`.
- **The office powers** come from `PowerGrant` actors. There is no slot machine in the office.

> Two stale docs found while checking. `CHANGELOG.md` v0.7.2 says "Celestial Fortune
> moved into the Carousel of Fates library"; the level says `SlotCabinet` is in
> L_Cathedral and always has been. `SlotCabinet.h`'s header comment says "cathedral
> apse", which agrees with the level. Fix the changelog line sometime.

---

## What this is not

`BOLD_PLAN.md` §5 describes THE WORKSHOP: a new room, a shelf of symbols, physical
levers, build-your-own-machine. **That is dropped.** Walt (2026-08-05): *"It was Claude
Opus 4.8 that suggested all those workshop plans and I really had nothing to do with
creating those. What I would like is a simple way to adjust the most important parts of
the par sheet from within the game and see the results in real time."*

This doc is that, and only that. No new room, no new level, no symbol shelf. A panel on
a cabinet that already exists in a level that already ships.

---

## The fiction

Real machines have a locked service door. Turn the attendant's key and the machine drops
out of play into setup: tower light on, reels stopped, numbers on the screen.

> **Walt's actual relationship to this (2026-08-05, correcting an earlier draft of this
> doc and the itch page's marketing voice):** *"I actually never made a slot machine and
> only did something called the data warehouse which archived play and produced accounting
> reports."*
>
> That is not a lesser connection — it is a **better** one. "Par sheet" is commonly glossed
> as **Probability Accounting Report**: the theoretical version of exactly the reports Walt
> spent years producing from real play data. He built the actual; this panel shows the
> theoretical. The two should agree, and when a real floor's numbers drifted from par,
> finding out why was his warehouse's job.
>
> **So this feature must TEACH, not merely allow.** Walt: *"I actually want something in
> the game that teaches me as well as new players."* That is the design constraint that
> outranks the others — see "Showing the work" below.

- **E on the front** → play the machine (exists today)
- **E on the side panel** → open the technician's panel (new)

The panel is not a settings menu. It is a place in the world, on a cabinet, that the
player walks up to.

---

## The three knobs

The current par sheet is a 40-stop strip and an 8-row paytable
([SlotGameModel.cpp:20](../Source/SibeliusGame/SlotGameModel.cpp)), returning **95.43%**:

```
Star 9   Moon 8   Galaxy 7   Saturn 5   Mars 5   Crown 2   Earth 2   Wild 1   Seven 1
```

Nobody edits a forty-character strip in a game. Three numbers:

### 🔒 LOCKED (Walt, 2026-08-05): PAYS × / WILDS / JACKPOT

| Knob | Range | Par sheet effect | What it TEACHES |
|---|---|---|---|
| **PAYS ×** | 0.7–1.3 | Scales the whole paytable | **The pure dial.** RTP moves proportionally; volatility and hit frequency barely move. The one clean cause→effect in the panel |
| **WILDS** | 0–3 stops | Wild count on the strip (now **1**) | **The entangled dial.** Moves RTP *and* hit frequency together. Teaches that par sheet knobs are not independent |
| **JACKPOT** | 250–4000 | Seven's 5-of-a-kind pay (now **1000**) | **The feel dial.** Large volatility swing, small RTP effect. Teaches that two machines with identical RTP can feel nothing alike |

An earlier draft proposed WILDS / JACKPOT / **BONUS** (Earth count). Revised because that
set was weak *for teaching*: two of the three moved several outcomes at once, so a learner
could not tell which knob caused what. The locked set is deliberately one pure lever, one
tangled lever, and one feel lever — three different lessons.

**BONUS is dropped from the first build.** Free spins are the fiddliest math in the model
(positional scatter probability plus a retrigger series) and their effect is the least
legible — you feel rhythm, not numbers. Revisit at step 4, once the loop is proven.

WILDS capped at **3**, not 4: four wilds in forty stops would push RTP far outside any
plausible band and the knob would spend most of its travel in UNLICENSED territory, which
teaches nothing. Re-tune all three ranges once the calculator exists and shows real numbers.

---

## The live readout — and the honest math

Above the knobs, three numbers that update **the instant a knob moves**. This is the
whole feature; a "Simulate" button you press and wait on is homework, not play.

### RTP — computed exactly, no simulation

Five reels draw independently from one strip, so a symbol's probability is its count over
40. Fixed paylines, left-to-right, 3+ matching. Per line, per symbol *s*:

```
P(first k reels show s-or-Wild) × P(reel k+1 does NOT show s-or-Wild) × Pay(s, k)
```

Summed over symbols and k ∈ {3,4,5}, times 15 lines. Microseconds.

**The subtlety, stated up front:** Wild substitutes for everything *and* has its own
paytable row (100/300/1000). A line of five Wilds can be read as "five Wilds" or as "five
of whatever it substitutes for", and the machine must pay the better one. Getting that
attribution wrong is the classic par-sheet bug and it silently shifts RTP. This is exactly
why the cross-check below is not optional.

### Free spins — the fiddly part

3+ Earths anywhere in the 5×3 grid awards 6 free spins at ×3, retriggering for 8 more.
Scatter probability is positional (anywhere in the grid, not on paylines), and the
retrigger is a converging series. Computable in closed form, but it is the piece most
likely to be wrong first.

### Hit frequency — by fast simulation, deliberately

"P(a spin pays anything)" needs the joint distribution across 15 overlapping paylines —
inclusion-exclusion, genuinely messy. A 10,000-spin sim answers it in a millisecond or
two, which is well inside a frame.

### 🔒 LOCKED (Walt, 2026-08-05): RTP exact, hit frequency simulated

Each method does what it is good at rather than forcing one to do both.

RTP has a closed form and must be exact — it is the number the licence lamp judges and the
number the player is being taught to reason about. Hit frequency does not have a cheap
closed form (15 overlapping paylines, inclusion–exclusion), but 10,000 simulated spins
answer it in a millisecond or two, well inside a frame, and the answer is accurate enough
for a readout the player reads as "how often do I win something".

### Volatility

Standard deviation of per-spin return, from the same distribution the RTP pass builds.
Displayed as a word (LOW / MEDIUM / HIGH / WILD), not a number — nobody feels a σ.

### The cross-check that keeps us honest

`SlotSmokeTest` already runs a million spins and reports RTP. Extend the gate to assert
**the exact calculator and the million-spin sim agree within tolerance**, on the shipped
par sheet *and* on a few deliberately odd ones. If they diverge, the calculator is lying
to the player in real time — the worst failure this feature could have.

---

## The house rule

One lamp on the panel: **LICENSED** / **UNLICENSED**.

Take RTP above the ceiling and the machine will not run — you have built a machine no
floor will take. Take it below the floor and it runs, but it is a machine nobody sits at
twice. Every real par sheet lives inside that band, and squeezing a machine you *like*
into a band you *must* hit is the actual job.

### 🔒 LOCKED (Walt, 2026-08-05): the band is `[85%, 96%]`

- **Floor 85% — regulatory.** Below this no jurisdiction licenses the machine.
- **Ceiling 96% — commercial.** The house will not run a machine that does not earn.

An earlier draft said 98%. Raised the floor's importance and *lowered* the ceiling because
98% never bites: the knobs would almost never hit it and the constraint would be
decorative.

The happy accident: the shipped par sheet returns **95.43%** — comfortably inside, but not
lazily so. The default machine is already a real design decision sitting near the
commercial edge, which teaches something true before the player touches a single knob.

### 🔒 LOCKED (Walt, 2026-08-05): UNLICENSED refuses to spin

*"refuse to spin - the house won't take it"*

The machine will not run at all. No spin, no winnings to argue about.

Why this is the better of the two options considered (the other was: let it spin, then
have the house confiscate the winnings):

- **The refusal lands on the MACHINE, not the player.** You did not lose money; you built
  something the floor will not accept. That is the real consequence a slot designer faces,
  and it is a design failure rather than a punishment.
- **It teaches immediately.** The feedback arrives when you move the knob, not after a
  spin resolves. Confiscation would teach the same lesson several seconds later and feel
  like a trick.
- **No sour edge.** Taking winnings away from a player who just watched reels land is the
  one interaction guaranteed to read as the game cheating, however fair it actually is.

Implementation: the licence check gates the spin at `ASlotCabinet` / the screen widget —
the reels never move, and the panel's lamp already says why. The player fixes it by
walking back to the panel, which is exactly where the lesson lives.

---

## Where the branch finally earns its keep

Walt (2026-08-05, earlier the same day): *"I never really understood the benefit of the
branch in the game."*

| Key | At the panel |
|---|---|
| **6** Test-Drive | fork the par sheet — edit and play in a sandbox |
| **7** merge | keep it; the machine now runs your math |
| **8** discard | throw the edit away, revert to what worked |

Change the math, run it, keep or revert. That is not a metaphor for a programmer's
workflow — it is literally what a slot designer does all day. The mechanic with nothing
worth branching and the panel Walt wants are the same feature.

### 🔒 LOCKED (Walt, 2026-08-05): branching is NOT required to edit

Editing is free and always reversible — the panel carries **REVERT TO FACTORY**.

Requiring the player to branch before touching the knobs was considered and rejected. It
puts friction between wanting to fiddle and fiddling, at exactly the moment curiosity is
highest. It is also how the branch earned its current reputation: a verb you must perform
rather than one you reach for. **Teaching by forcing is bad teaching.**

Instead the branch earns its place by being obviously useful: it is how you KEEP a variant
you like while trying another. Surface the offer when the player has an unkept change —
that is the moment it is genuinely the right tool, and that is when a mechanic actually
gets learned.

---

## Persistence

`ASlotCabinet` is deliberately **not** `IBranchable` and never enters deploy saves
(`SlotCabinet.h`, CathedralDoor reasoning). A player-authored par sheet is different — it
is progress and losing it would sting.

### 🔒 LOCKED (Walt, 2026-08-05): ONE saved par sheet, in the existing save

Plus the factory default, always available and never overwritten.

Rejected: **session-only** (the learning evaporates overnight — fatal when teaching is the
point) and **a named collection** (inventory management, and scope creep on a feature whose
whole virtue is being small).

**Validate on load and CLAMP, never trust blindly.** A par sheet saved before a range
changes must be pulled into legal bounds on read. An out-of-range value that reaches the
model would produce an RTP no band check anticipated — and, being saved data, it would
survive every restart while looking like a math bug.

---

## Build order

Each step is independently useful and independently gate-able.

**1 — Par sheet becomes data.** The prerequisite, and the only unavoidable refactor. The
strip, paytable and paylines are `static` file-scope data with `static` accessors
(`Strip()`, `PayFor()`), so every machine in the game necessarily shares one immutable
copy. Move them into an `FSlotParSheet` the model *holds*. Current values become the
default, named "Celestial Fortune". ~25 lines of data and four accessors in a 204-line
file. `SlotSmokeTest` must report the identical 95.43% afterwards — that is the proof the
refactor changed nothing.

*Also unblocks G4's high-limit machine, which needs exactly this plumbing.*

**2 — The exact RTP calculator + gate.** No UI. A pure function over an `FSlotParSheet`
returning RTP, per-symbol contribution, and volatility. The gate asserts it agrees with the
million-spin sim on the shipped par sheet *and* on several deliberately odd ones. **Re-tune
the three knob ranges here**, against real numbers rather than guesses.

**3 — The panel.** Side-of-cabinet interaction, the three knobs, the live report
(including per-symbol contribution and HOLD %), the licence lamp, REVERT TO FACTORY, and
the spin gate that enforces UNLICENSED.

**4 — Persistence, branch offer, and BONUS.** The saved par sheet, the branch prompt when
a change is unkept, and the Earth-count knob with its free-spin math — the fiddliest
piece, deliberately last.

Stop after any step and the game is still shippable. Steps 1 and 2 have no player-visible
effect at all, which makes them safe to land whenever.

---

## Showing the work — the part that actually teaches

Everything above is plumbing. **This** is the feature.

### 🔒 LOCKED (Walt, 2026-08-05): the panel IS the par sheet report, live

**Show the arithmetic, not just the answer.** When WILDS goes 1 → 2, the panel must not
merely show RTP jumping — it shows **which symbols' contributions to RTP changed, and by
how much.** Per-symbol contribution is a par sheet's real anatomy. Without it a knob is a
slider; with it, a knob is a lesson.

`SlotSmokeTest` already prints a `PAR SHEET REPORT` (RTP, hit frequency, exposure). **The
panel is that same report, rendered live.** Same numbers, same layout, updating as a knob
turns — so the thing the gate checks and the thing the player reads are visibly one object.
When the player later sees the gate output in a build log, they recognise it.

Also display **HOLD %** beside RTP — the same number said the accountant's way
(`hold = 1 − RTP`). That is the vocabulary Walt's warehouse reports were written in, and
it is how the operator side of the business actually talks.

### 🔒 LOCKED (Walt, 2026-08-05): the panel is open from the start — no key

Considered making the panel a reward gated behind a key the player earns. Rejected once
teaching became the stated goal: a locked panel is a gate between players and the lesson,
and most players would never find the key. Put a visible hint on the cabinet instead.

---

## Still open

Nothing blocking. Ranges for all three knobs are provisional and get re-tuned at step 2,
when the calculator first produces real numbers.
