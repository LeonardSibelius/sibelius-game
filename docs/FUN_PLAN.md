# Making Sibelius More Fun — A Plan

**Written 2026-07-12, after a side-by-side analysis of Sibelius and ProjectGhostCam.**

The headline finding is good news: Sibelius does not need new systems to become fun.
Almost everything a fun game needs is **already built and smoke-tested in this repo** —
it just isn't wired into the game the player actually experiences. The plan below is
mostly *connective tissue*, not new construction.

---

## 1. Why Raymond's game is fun (the principles, not the theme)

ProjectGhostCam is fun for reasons that have nothing to do with ghosts or cameras.
Strip the theme away and five design rules remain. Each one is a rule Sibelius can
adopt without changing its story at all.

**R1 — One tight, repeatable core loop.** Cast → wait → hook → reel → reveal, about a
minute per rep, ending in a small ceremony (the Polaroid reveal). The player always
knows what "one more go" means. Sibelius's chapters are one-time vignettes; once a
chapter is done there is nothing to repeat.

**R2 — Earn by playing, spend deterministically.** Catches convert to **souls**;
souls buy items with *known, guaranteed* effects (a better flashbulb, a bigger catch
zone). Accumulation comes from skill and time; spending is never a dice roll.

**R3 — One stat, one code point.** Every purchasable thing hooks into exactly one
existing mechanical lever. Buying is exciting because the effect is legible.

**R4 — Gambling is seasoning, not the spine.** Raymond's game *does* have a slot
machine — the tasseomancy tea-reading is described in his own docs as "a circular slot
machine wearing authentic tasseography clothes," tuned to ~92% RTP **with the same
million-spin par-sheet culture as Sibelius's Celestial Fortune** (his docs cite it by
name). But it is an optional downtime activity that gambles *currency for currency*.
It never gates powers, progression, or story.

**R5 — Failure is generous.** A failed reel still catches the ghost (at Faded
quality). The player is never punished with nothing; the floor of every interaction is
"you still got something."

---

## 2. Is the AI-sauce slot machine idea silly?

**No — and Raymond's game is the proof.** His tea room is the same mechanic wearing
different clothes, and it explicitly borrowed Celestial Fortune's math discipline. The
slot model in this repo (`SlotGameModel.cpp`, seeded, headless, par-sheeted) is
genuinely good work that another project already imitated.

But the idea as currently framed has one half right and one half risky:

- **"Consume sauce to gain powers" — good.** That is just a shop with a themed spend
  point (the cauldron). It satisfies R2 and R3 perfectly.
- **"Accumulate sauce by playing the slot machine" — risky as the *only* source.**
  If luck is the sole path to power, the player has no agency over progression: a
  lucky player steamrolls, an unlucky player stalls, and neither outcome feels earned.
  This is the one place Raymond's structure differs from yours, and it's the
  difference that matters (R4).

**The fix is a swap, not a deletion:** make *playing the game* the primary way to earn
sauce, and make the slot machine an **optional amplifier** — a place to wager sauce
you already earned, hoping to multiply it. Exactly like Raymond's tea room. The slot
machine stays; it just moves from the spine to the seasoning rack.

---

## 3. What Sibelius already has (the inventory of parts)

| Part | State | File anchor |
|---|---|---|
| Six power verbs (Code Vision, Refactor, Compile, Test-Drive, Deploy, Generate) | Shipped, but **always-on from spawn** — never earned | `SibeliusGameCharacter.h` |
| Sauce Cauldron (feed → blend → complete) | **P0 stub**, unwired | `SauceCauldron.h/.cpp`, `L_AI_Temple` |
| Celestial Fortune slot | Complete, par-sheeted; free-play coda only | `SlotGameModel.*`, `SlotCabinet.*` |
| **Carousel of Fates** — Balatro-like roguelike: shop, 11 Charms, currency, quotas, boss curses | **Fully simulated and smoke-tested, orphaned in a test map** | `CarouselSim.cpp`, `CarouselRun.h`, `CarouselCharm.h` |
| Curio collection loop (Many Worlds) | Complete, smoke-tested, deliberately deactivated | `Curio.*`, `CabinetOfCuriosities.*`, `CurioCollectionSubsystem.*` |
| Inventory (Book/Key only) | Shipped, used once in Ch3 | `InventoryComponent.*`, `CompileTypes.h` |
| Ch7 finale (seven-stage power gate + coda) | **Unbuilt** — the game has no reachable win | cathedral in `L_Cathedral` |
| Player HUD | Dev overlay doubling as HUD | `SibeliusHUD.*` |

The pattern: **the richest agency-and-economy loop in the codebase (the Carousel) is
the one the player can't reach**, and the central metroidvania promise (earn a verb,
open the world) isn't mechanically real.

---

## 4. The plan — seven steps, in order

### Step 1 — Make the powers *earned* (the metroidvania becomes real)

All six power components are attached and active at spawn. Add a simple
`bUnlocked` gate per power component (default false; Code Vision can start true),
plus one `UPowerProgressionSubsystem` that grants powers. Chapter end-triggers call
it today; Step 3 makes the cauldron call it instead/as well.

*Why first:* every later step (sauce spend, finale) needs "power acquisition" to be a
real mechanical event, not a narrative fiction. It's also the cheapest step — a
boolean and an early-out per component.
*Rule applied:* R1 — the loop "reach place → earn verb → verb opens new place" is the
game's stated promise; make it true in code.

### Step 2 — One currency: **Sauce**, earned by playing

Retire the fragmented Book/Key/GenerateBudget trio as the *only* resources and add a
persistent `int32 Sauce` wallet (single authority, mirror Raymond's controller-side
wallet pattern). Earn sources — all from *play*, all already-built systems:

- Chapter completion (big grant — `UCompileEndSubsystem` already fires once).
- Books beyond the Ch3 requirement (books become sauce ingredients — thematic!).
- Slapping a Refuser (small grant; makes the Slap loop rewarding, R5).
- **Curios in the Many Worlds** (see Step 5).

*Rule applied:* R2. Also fixes "resources with no sink / sinks with no resource."

### Step 3 — The Sauce Cauldron becomes the shop (deterministic spend)

Unstub `ASauceCauldron`: feeding it sauce is how the player **buys** with known
prices and known effects. First catalog, each item touching exactly one existing lever
(R3):

| Purchase | The one code point |
|---|---|
| A power unlock (alt-path to chapter completion, or post-game re-specs) | `UPowerProgressionSubsystem` |
| +Generate budget | `GenerateComponent.h::RemainingBudget` |
| Code Vision range/duration | `CodeVisionComponent` reveal params |
| Slap power (knockback force) | `USlapComponent` impulse |
| A Carousel entry token (see Step 4) | `FCarouselRun` entry |

The `L_AI_Temple` level and `ABookRain` feed ceremony already exist — the buy moment
gets a built-in ceremony for free (R1's "small ceremony at loop end").

### Step 4 — Wire in the Carousel of Fates as the gambling venue

This is the biggest fun-per-effort win in the repo. The Carousel is a complete
Balatro-style roguelike — shop, Charms, quotas, interest economy — with real decisions
(which Charm to buy, when to bank), which is exactly the *agency* a plain slot lacks.
It's already simulated and smoke-tested; it needs a door, not a redesign.

- Player wagers **Sauce** to enter a run; run winnings pay out in Sauce.
  Currency-for-currency, optional — Raymond's tea-room shape exactly (R4).
- Physically: put the carousel in the cathedral or a new side room off the office;
  the nine fate glyphs already foreshadow it in the opening apparition.
- **Celestial Fortune stays as the free-play coda gift** — it's a lovely grace note
  and shouldn't take stakes. The Carousel is where stakes live.

### Step 5 — Give the Many Worlds a gentle reason: curios grant Sauce

The curio/Cabinet loop was cut for wanting "wonder, not collection" — a fair instinct,
but the result is forests with no reason to enter twice. Compromise: reactivate curios
as **sparse, unlabeled sauce pickups** (no checklist, no completion meter, no cabinet
UI). Wandering stays wonder-first; it just also feeds the economy quietly.
Raymond's equivalent: fishing zones are atmospheric *and* they're where souls come from.

### Step 6 — Build the finale (the game needs a reachable win)

The seven-stage power-gate puzzle in the cathedral is the payoff for Steps 1–3: it
only lands if powers were genuinely earned. Sequence: last Mrs. Hall wall → each power
used once in order → Synthesis → **Celestial Fortune coda as the gift**. Until this
exists, the game is a vignette collection with no ending — the single biggest
"strange" factor for a new player.

### Step 7 — A player HUD and reward ceremonies (feedback, not diagnostics)

The dev overlay (2×-scaled canvas text) is the de-facto HUD. Replace the
player-facing layer with: sauce count, current power prompts, and a **reward
ceremony** when sauce is earned or a power unlocks (the AI-apparition ceremony is
already the game's best moment — reuse a small version of it, the way Raymond reuses
the Polaroid reveal for every single catch). Keep the dev overlay on a debug toggle.

---

## 5. Sequencing and effort

| Step | Effort | Unblocks |
|---|---|---|
| 1. Power gating | S | 3, 6 |
| 2. Sauce wallet + earn sources | S–M | 3, 4, 5 |
| 3. Cauldron shop | M | 4, 6 |
| 4. Carousel integration | M (sim is done; needs entry/exit + Sauce plumbing) | — |
| 5. Curio sauce pickups | S (system exists, deactivated) | — |
| 6. Finale | L | shipping 1.0 |
| 7. HUD + ceremonies | M | polish, any time after 2 |

Steps 1+2 together make the game *feel* different within a weekend of work: suddenly
things you do produce a number that goes up, and the number promises to become power.

---

## 6. Two habits to borrow from Raymond's process

1. **A conventions rule.** Raymond keeps `Docs/UX_CONVENTIONS.md`: every player-facing
   system defaults to the conventions of games the players already know, and every
   divergence is logged and signed off. Sibelius diverges constantly (dev-key
   bindings like 6/7/8 for branch ops, double-Q quit, no pause menu). Divergence is
   fine for an art game — but it should be *chosen*, one decision at a time, in a doc.
2. **Playtest the loop, not the feature.** Raymond's milestones gate on "does the
   whole loop feel good," not "does the feature pass." After Step 2 lands, the test
   is: *hand someone the game and watch whether they choose to earn more sauce.* If
   they do, the loop works.

---

## Appendix: the slot machine question, answered in one paragraph

The Celestial Fortune / AI-sauce idea is not silly — it's half of a good economy.
"Consume sauce to gain powers" is a shop, and shops are the proven backbone of games
like Raymond's (and Webfishing, and Stardew). The only correction needed is to the
*earn* side: sauce should come primarily from playing, with gambling as an optional
way to risk-and-multiply what you've earned — never the sole faucet, never a gate on
progression. Raymond's game follows exactly this shape, and his tea-reading gamble
explicitly credits Celestial Fortune's math discipline as its model. Your slot machine
didn't need to be cut from the design; it needed to be *demoted from spine to
seasoning* — and given the Carousel of Fates, this repo already contains a strictly
more interesting gambling game waiting for a door.
