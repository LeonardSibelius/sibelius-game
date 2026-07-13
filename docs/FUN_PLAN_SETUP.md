# FUN_PLAN — Walt's In-Editor Setup Steps

The C++ for all seven FUN_PLAN steps is in. What's left is **placement work only**,
and it has to be done by hand in the editor (meshes and positions are composition —
your job, not the code's). Do these in order; each one is small.

> **Important first fact:** the powers are now *actually gated*. A fresh game has
> only Code Vision. Until you place the shrines (§1), unlock things in PIE with the
> console: press backtick, type `UnlockAllPowers`. Other cheats: `GrantSauce 500`,
> `GrantPower refactor`, `ResetProgression` (wipes powers + sauce back to a fresh
> game — use this to test the real first-time experience).

---

## 1. Place the five power shrines (L_Office_v02)  ~15 min

The shrine actor is **`APowerGrant`** (search "PowerGrant" in Place Actors). Place
**five** of them, one per chapter, each at the natural spot where that chapter
*begins* (the player earns the verb, then the chapter teaches it):

| # | Power setting | Suggested spot |
|---|---|---|
| 1 | `Refactor` | wherever Ch2 starts (near the first refactorables) |
| 2 | `Compile` | the library room entrance (before the books) |
| 3 | `TestDrive` | at the Ch4 branch tutorial area |
| 4 | `Deploy` | near the Ch5 start |
| 5 | `Generate` | near the Ch6 computers |

For each one:
1. Drag it in, set **Power** (Details → Power Grant) to the row's verb.
2. Assign any **mesh** to its Mesh component — a book, an orb, a floppy disk from
   the props packs; it spins and bobs on its own so anything reads as a pickup.
3. Leave **Sauce Reward** at 25 (tune later), **Grants Power** = true, **Grant Key** = None.
4. Optional: set **Grant Sound** to any sting you like.

Walk into one in PIE: banner "REFACTOR IS YOURS", +25 sauce top-right, shrine gone.
It stays gone across sessions (the claim is in the save). No shrine for Code
Vision — you start with it.

## 2. Place the Sauce Cauldron (L_Office_v02, the kitchen)  ~5 min

Drag in **`ASauceCauldron`** — the kitchen near the Many Worlds door is the
thematically right home. Assign a pot-ish mesh to **CauldronMesh** (and optionally
a glowing blob to **ContentsMesh**). Press E on it in PIE: the shop opens — locked
powers at 150 sauce, Generate-budget and Mighty-Slap upgrades. Esc closes.

## 3. Place the Carousel door (L_Office_v02)  ~5 min

Drag in an **`ACathedralDoor`** (yes, the same class — it's a general travel door):
- **Target Level Name** = `L_Carousel_Test`
- **Prompt Text** = `Ride the Carousel of Fates [E]`
- Assign a door mesh. A basement or side corridor fits the "casino in the walls" vibe.

In the carousel room: **N** stakes 50 sauce and starts a run, **E** pulls the lever,
**1/2/3** buy, **R** rerolls, **Enter** continues, **O** returns to the office.
Leaving mid-run keeps the run alive; re-enter to finish it.

## 4. Sprinkle curios in the forests (L_Forest_01..08)  ~10 min

Open a few forest levels and drag in **`ACurio`** — one per forest at most, and
leave a couple of forests empty (the point is quiet discovery, not a checklist).
Tuck them off the road: behind the hero poplar, by the sailboat. Each pays +15
sauce on E. The default glowing orb works as-is; no configuration needed.

## 5. Build the finale (L_Cathedral)  ~20 min

1. Drag in **`AFinaleAltar`** at the apse, in front of the slot cabinet. Assign an
   altar-ish mesh.
2. Build a **wall** of meshes between the nave and the apse so the cabinet is
   unreachable — use the cathedral kit's blocks, or red cubes for now (they're
   Mrs. Hall's error-blocks; red translucent material if you have one).
3. Select every wall piece → Details → Actor → **Tags** → add the tag `FinaleWall`.
4. PIE test: walk to the altar — banner asks for CODE VISION (1/6). Use each verb
   in chapter order **standing near the altar** (V hold, R, B, 6 then 7, 0, G).
   On the sixth: "THE THREE-PART SYNTHESIS IS COMPLETE", the wall falls, +200
   sauce, and the Celestial Fortune coda is open. Once done it stays done — the
   wall never comes back (delete the progression save or `ResetProgression` to
   re-test).

Note: stage 4 (TEST-DRIVE) counts entering, merging, OR discarding a branch — so
6-then-7 (enter, merge) leaves you clean for stage 5's Deploy (0), which refuses
while branched.

## 6. Playtest the loop (the real test)  ~30 min

`ResetProgression`, then play from scratch and watch for exactly one thing: **do
you ever choose to go earn more sauce?** Collect books → notice the counter →
find the cauldron → want something you can't afford → go slap a Refuser or ride
a forest → come back and buy. If that pull happens even once, the loop works.
Tuning knobs, all in one place each: prices in `SauceShop.h`, carousel stakes in
`CarouselRunSubsystem.h`, earn amounts on the actors' Details panels
(SauceOnCollect / SauceOnSlap / SauceReward).

---

## What the code did (for reference)

- **Powers are earned.** Six verbs gated through `CheckPowerUnlocked` on the
  character; fresh game = Code Vision only. `UProgressionSubsystem` persists
  powers + sauce + claims to its own `Progression` save slot.
- **Sauce earns:** books +5, connected slaps +2, curios +15, Ch3 completion +50
  (once), shrine pickups +25 (once each), Synthesis +200 (once).
- **Sauce spends:** cauldron shop (deterministic — powers 150, budget +5 for 40
  ×10 max, slap +50% for 60 ×3 max) and Carousel stakes (50 in; win 150 + 5 per
  banked coin; lose 10 per cleared round — never zero after progress).
- **HUD:** sauce count top-right with +N/-N flashes; power unlocks get a centered
  ceremony banner; the dev overlay (H) shows the full powers/sauce state.
- **Smoke gates:** `ProgressionSmokeTest` (21 checks) and `FinaleSmokeTest` join
  the ship-gate list; all pre-existing gates still exit 0.
