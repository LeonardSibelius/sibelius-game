# THE FLOOR REPORT — measuring what the machine actually did

*Design doc for v0.9. **Nothing is built yet. All decisions locked with Walt on
2026-08-05** — the 🔒 blocks record what was chosen and why. Ready to implement. Follows
`docs/PAR_SHEET_PANEL.md`, which this extends.*

**One line:** the `[T]` panel states what the machine *should* return. Add the page that
shows what it *did*, and whether the difference means anything.

---

## Why this feature

The panel currently asserts 95.567% and asks the player to believe it. Nothing in the game
ever counts a real spin.

That leaves the single hardest idea in gambling maths untaught: **theoretical return is a
long-run average, and a session tells you almost nothing about it.** A player who loses
forty spins in a row concludes the machine is broken or rigged. A player who wins big
concludes they found a loose one. Both are reading noise.

This page is the instrument that settles it — and it is the half Walt actually built for a
living. He ran the data warehouse that archived play and produced the accounting reports;
this is that report, sitting beside the par sheet it is checking. The par sheet is the
claim, the floor report is the audit, and **deciding whether a drifting machine is broken
or merely unlucky was his warehouse's job.**

> Corrected from the earlier draft of `PAR_SHEET_PANEL.md`, per Walt (2026-08-05):
> *"I actually never made a slot machine and only did something called the data warehouse
> which archived play and produced accounting reports."* That is the connection this
> feature is built on, and it is the stronger one.

---

## Where it lives

**A third page of the existing `[T]` panel**, opened with **M** — for **METERS**, which is
the real word. No new actor, no new level, no new widget class.

The panel already has this shape: a main page and a help page toggled with `H`, both
rendered through `BodyText` inside `BodyScroll`. A third page is a third compose function
and one more key.

Keys already taken inside the panel: `H`, `R`, `T`, `Esc`, arrows, `WASD`, `PageUp/Down`.
**`M` is free.** (`M` is the Status menu in the world, but the panel holds exclusive
keyboard focus while open, so there is no collision.)

> 🔒 **LOCKED — `M` opens the page, as a toggle. Not `Tab` cycling.**
>
> `Tab` cycling was the alternative. Rejected: `H` is already a toggle, so cycling would
> mean two different navigation idioms in one panel, and a cycle makes the player press a
> key an unknown number of times to reach a known page. A named key per page is how the
> panel already works and it stays legible as pages are added.

---

## The vocabulary is the lesson

Real cabinets carry two sets of counters, and the distinction is regulatory, not cosmetic:

| Term | What it is | Resettable |
|---|---|---|
| **Hard meters** | Lifetime totals. The regulator's number. Tamper-evident by law. | **Never** |
| **Soft meters** | The current session. What the attendant reads on a service call. | Yes |

The page shows both, side by side, and uses those words. A player learns the actual terms
of the trade, and the fact that **the lifetime meter cannot be reset** is authentic *and*
removes a feature — no reset key, nothing to explain, nothing to get wrong.

> 🔒 **LOCKED — no reset key at all. Neither meter can be cleared by the player.**
>
> A soft-meter reset was the alternative. Rejected on two grounds. First, hard meters are
> *interesting precisely because* they cannot be reset — a player who wants a clean reading
> and cannot get one has just learned what tamper-evidence is for. Second, a reset key is a
> destructive action one keystroke from a page full of numbers, and the panel's other
> destructive key (`R`, revert dials) already lives there.
>
> The session meters do clear, but only as a consequence of changing par — see below.
> Never on demand.

---

## What the page shows

```
  METERS                                      CELESTIAL FORTUNE

                              SESSION            LIFETIME
  Spins                           312             14,908
  Coin in                      46,800          2,236,200
  Coin out                     41,955          2,141,880
  ------------------------------------------------------------
  Measured return              89.65 %            95.78 %
  Theoretical (par)            95.57 %            95.57 %
  Difference                   -5.92 pts          +0.21 pts

  Paid anything                 29.8 %             31.4 %   (par 31.2 %)
  Bonus triggered                  4                 187    (par 1 in 78)
  Biggest win                   4,500              27,000

  ------------------------------------------------------------
  AFTER 312 SPINS, ANYTHING BETWEEN 39 % AND 152 % IS NORMAL.
  You are inside that. This machine is behaving exactly as designed;
  you have simply had a bad run.

  To measure this machine to within 1 %, you would need about
  1,000,000 spins. That is why a session can never tell you.
```

The last block is the entire point. Everything above it is bookkeeping.

---

## The maths behind the verdict

`FSlotParSheetReport` already carries `Volatility` — the standard deviation of per-spin
return in units of total bet, measured by `MeasureBySimulation`. That is all this needs.

After **N** spins, the measured return has a standard error of:

```
SE = Volatility / sqrt(N)
```

So the "normal" band at ±2 SE is `RTP ± 2·Volatility/sqrt(N)`, and the spins required to
pin the machine down to within a tolerance **t** (as a fraction, so 1% = 0.01) is:

```
N = (2 · Volatility / t)^2
```

I guessed volatility near 5 when writing this, which would have put that figure near a
million spins. **Measured, it is 3.119, and the answer is about 389,000 spins for ±1
point.** Less shocking than the guess, still the most valuable single line on the page —
at a spin every four seconds that is well over a fortnight of continuous play to learn one
machine's return to within a point.

**Minimum sample (added at step 4, not in the original spec).** Two standard errors is a
*normal* approximation, and a slot's per-cycle return is violently skewed — mostly nothing,
occasionally a bonus worth hundreds. Below **50 wagered spins** the page therefore quotes
no band at all and says *"far too few to tell you anything"*. At 19 spins the arithmetic
produces "0 % .. 234 %", which is both statistically meaningless and useless to read.
Refusing to answer is the honest result and the sharper lesson: the interesting fact about
a short session is not that it falls within range, it is that **no range exists yet**.

**The verdict line** is then just a comparison — measured inside the band reads *"behaving
as designed"*, outside reads *"this is unusual — but see how wide the band is at low spin
counts"*. Never *"the machine is broken"*: at these sample sizes it almost never is.

> 🔒 **LOCKED — lead with the band, and state it before the verdict.**
>
> *"After 312 spins, anything between 39% and 152% is normal"* is the headline; *"you have
> simply had a bad run"* follows it. That order matters. Stated the other way round it
> reads as the machine making an excuse for itself — the house explaining why the player's
> losses are fine. Stated band-first, the number does the arguing and the sentence merely
> reports where the player landed in it.
>
> Two rules for the wording, so it never becomes an excuse:
>
> - **The band is computed and shown, never asserted.** The player can see it is wide
>   because 312 is a small number, not because the machine says so.
> - **A hot session gets the same treatment as a cold one.** Running at 152% must read
>   *"also normal, also meaningless"* — not a congratulation. If the page only reaches for
>   the confidence band when the player is losing, it is spin, and the lesson dies.

---

## Where the counters live

**On `USlotGameModel`, incremented inside `Spin()`.**

The model is the machine's pure brain — no UI, no actors, deterministic, headlessly
simulatable. It is also the only thing that knows what a spin actually paid. Putting the
meters anywhere else means re-deriving that from presentation state.

This buys a property worth having: `MeasureBySimulation()` already spins a real model ten
thousand times. **The meters become self-testing for free** — spin a model, then assert its
own meters against `Analyze()`'s closed-form answer. See "Verification" below.

Proposed fields (session-scoped on the model; lifetime accumulates into the save):

| Field | Type | Note |
|---|---|---|
| `MeterBaseSpins` | `int64` | Paid spins |
| `MeterFreeSpins` | `int64` | Free spins consumed — wager nothing, pay plenty |
| `MeterCoinIn` | `int64` | Total wagered |
| `MeterCoinOut` | `int64` | Total paid |
| `MeterPayingSpins` | `int64` | Spins returning > 0 → measured hit frequency |
| `MeterBonusTriggers` | `int64` | → measured trigger rate |
| `MeterBiggestWin` | `int64` | |

`int64` throughout because lifetime coin-in on a machine played for hours will pass
`int32` sooner than feels plausible, and the cost of the wider type is nil.

**Free spins are the subtle one.** They wager nothing and pay normally, so they belong in
coin-out but never coin-in. Getting this backwards is exactly how a real floor report ends
up disagreeing with par — worth a comment in the code.

---

## Persistence

Additive `UPROPERTY(SaveGame)` fields on `FProgressionData`, following the existing
pattern. Unlike the dial fields, these need **no negative sentinel** — zero is the correct
initial value, and old saves default-filling to zero is exactly right.

---

## The wrinkle: the player can change par mid-life

The dials mean lifetime meters can span several different machines. A lifetime measured
across a 95.6% sheet and a 91% sheet will legitimately disagree with whichever par is
loaded now — and that is not a bug, it is the reason real floors take a meter reading
before and after any par change.

> 🔒 **LOCKED — a dial change clears the session meters and footnotes the lifetime block.**
>
> 1. **Turning any dial clears the session meters**, and the page notes *"par changed —
>    session meters cleared."* A fresh baseline, which is exactly what an attendant would
>    take.
> 2. **Lifetime meters never clear.** Once the par has ever been edited, the lifetime block
>    carries a standing footnote: *"played across more than one par sheet — compare with
>    care."*
>
> That single footnote is why the before-and-after reading exists on a real floor, and it
> costs one bool. Note it reuses `FProgressionData::HasEditedParSheet()`, which already
> exists for exactly this question.
>
> Rejected: stamping each meter with the par it was earned under. Correct, and far more
> machinery than a teaching page needs — it would mean versioning the meters against a
> sheet hash and explaining the result. The footnote conveys the same caution honestly.

---

## Verification

`SlotSmokeTest` gains two checks, so this ships gated rather than eyeballed:

1. Spin a model 200,000 times; assert its own `MeterCoinOut / MeterCoinIn` lands within
   tolerance of `Analyze()`'s exact `RtpPercent`. This proves the meters count the same
   game the closed form solves.
2. Assert `MeterPayingSpins / MeterBaseSpins` matches the report's simulated
   `HitFrequencyPercent`.

Tolerance should be derived from the same standard error the page displays, not picked by
hand — the gate and the feature then rest on one formula, and a bad tolerance shows up as a
bad player-facing band.

**Final gate count: 27 → 34.** Three at step 1 (below), one at step 3 for the monospace
body font's path (`FObjectFinder` fails silently, so a wrong path would misalign every
column with nothing in the log), and three at step 4 for the confidence maths:
`WageredVolatility > Volatility`, the band narrowing as exactly 1/√N, and
`SpinsToMeasureWithin` inverting `ConfidenceHalfWidth` to within 0.01 points.

**Built as three checks at step 1, not two — 27 → 30.** The third is an internal-consistency
check (the meters against the million-spin loop's own independent tally) which turned out
to be free: the loop already accumulates every total, so it costs one comparison. It
catches the two mistakes most likely to be invisible — crediting a free spin as wagered,
or counting a retrigger twice — either of which would leave the machine playing perfectly
while the report quietly lied.

---

## ✅ RESOLVED at step 4: `Volatility` was not the number the band needs

Found while implementing step 1. `SlotParSheetMath::MeasureBySimulation` skips free spins
entirely — it `continue`s before accumulating, so **free-spin winnings are excluded from
the variance**, not merely their (zero) stake. `FSlotParSheetReport::Volatility` is
therefore the standard deviation of base-spin return *counting only base-spin payouts*.

That is the right figure for the "feel" word the panel already shows. It is the **wrong**
figure for the confidence band, and wrong in the dangerous direction: the bonus round is
the single largest source of variance in the machine, so excluding it makes the band **too
narrow** — the page would tell a player their perfectly ordinary session was unusual. That
is exactly the failure this feature exists to prevent.

**Fixed as specced:** a separate field, `WageredVolatility`, measuring the SD of return per
unit *wagered*, with each base spin credited with the free-spin winnings it went on to
produce. A new field rather than a changed one, so the panel's existing volatility word —
shipped in v0.8.9 — keeps its meaning and nothing already on screen moved.

**It was not a rounding-error concern.** Measured on the shipped sheet:

| | |
|---|---|
| `Volatility` (per spin, bonus excluded) | **1.288** |
| `WageredVolatility` (per cycle, bonus included) | **3.119** |

The old figure was **2.4× too small**, so the band would have been 2.4× too narrow. On
Walt's own 19-spin test session at 68.42% against a 90.91% par, the wrong number gives a
range of 31.8%–150.0% and the right one gives 0%–234%. Not enough to flip that particular
verdict, but the same error at a few hundred spins flips it easily — and always toward
calling an ordinary session unusual, which is the one thing this page must never do.

A gate check now asserts `WageredVolatility > Volatility` permanently.

---

## Scope

**In:** the meters page, the counters, persistence, the confidence band, two gate checks.

**Out:** the Carousel and video poker keep their own models and are untouched. No graph, no
history, no per-symbol actual-vs-expected breakdown — that is a fourth page and a later
version if it earns one.

---

## Locked decisions, in one place

*Ratified by Walt, 2026-08-05.*

| # | Decision | Rejected alternative |
|---|---|---|
| 1 | **`M` opens METERS**, a third page of `[T]`, as a toggle | `Tab` page cycling |
| 2 | **No reset key.** Neither meter is player-clearable | A soft-meter reset |
| 3 | **A dial change clears session meters** + footnotes the lifetime block | Per-meter par stamping |
| 4 | **Band first, verdict second** — and a hot session reads as meaningless too | Verdict-led wording |

Plus the choices that follow from the code rather than from taste, recorded so they are not
re-litigated at implementation time:

- Counters live on `USlotGameModel`, incremented in `Spin()` — the only class that knows
  what a spin paid, and already the thing `MeasureBySimulation()` drives.
- `int64` throughout; free spins count toward coin-out and never coin-in.
- Persistence is additive `UPROPERTY(SaveGame)` on `FProgressionData`, no sentinel needed.
- The confidence band reuses `FSlotParSheetReport::Volatility`; nothing new is derived.
- Two new `SlotSmokeTest` checks, gate 27 → 29, tolerance derived from the same standard
  error the page displays.

---

## Implementation order

Each step is separately buildable and separately verifiable — the same shape that worked
for the panel itself.

1. **Counters on the model** + the two gate checks. No UI. The gate proves the meters
   count the same game `Analyze()` solves before anything renders them.
2. **Persistence** — the `FProgressionData` fields, lifetime accumulation, the
   dial-change clear.
3. **The page** — `M`, the two columns, the bookkeeping rows.
4. **The band and the verdict** — the part that matters, built last, on numbers already
   proven by step 1.
