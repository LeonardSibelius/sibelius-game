# SWARM_PLAN — armies of AI agents and demons

Started 2026-08-26. Walt: *"More powers are needed... actually, what I want is Swarm. I
want to find an Unreal Engine ability to render armies of AI agents and demons."*

Then: *"My five MetaHumans and I will fight the army of Gideons."*

---

## 1. The measurement, first

Everything below follows from one number, and the number is measured rather than
assumed. `USwarmBenchSubsystem` (`swarm.Bench`) ramps real Refusers in rungs and reports
the frame cost of each. Run in **L_Meadow**, 2026-08-26, RTX 5070 Ti:

| Gideons | frame | fps | cost of the demons |
|---|---|---|---|
| 0 | 8.33 ms | 120 | — |
| 10 | 8.45 ms | 118 | +0.1 ms |
| 25 | 8.70 ms | 115 | +0.4 ms |
| 50 | 9.93 ms | 101 | +1.6 ms |
| 100 | 12.60 ms | 79 | +4.3 ms |
| 150 | 15.25 ms | 66 | +6.9 ms |
| 200 | 18.23 ms | 55 | +9.9 ms |
| 300 | 24.57 ms | 41 | +16.2 ms |

**It never reached the 33 ms budget.** The ladder ran out at 300 while still at 41 fps —
the bench stopped at its own maximum, not the machine's. 60 fps lands around **170**;
30 fps extrapolates to roughly **450**.

Raw output: `Saved/swarm_bench.json`.

### What that overturned

The plan before the measurement was a **hybrid**: a cheap instanced horde in the
distance, real Actors only near the player, promoting one into the other at a boundary.
That rested on an assumption — that real Actors would run out somewhere near 50 — and
the assumption was wrong by a factor of three.

**150 real Gideons cost 7 ms.** Every one is a full Actor: slappable, refactorable into
a goat, visible to Code Vision. An army where every member answers every power in the
game. 150 demons on a ridge line already *is* an army.

**So build that, and only reach for Niagara if a thousand is genuinely wanted.** The
hybrid is not cancelled, it is deferred until something demands it — which is a better
place for it than the critical path.

### Two caveats that belong next to the number

- **This machine is not the player's.** A 5070 Ti, and the minimum spec is untested
  because there is one machine in this project. Budget **60–80** for shipping and treat
  150 as a ceiling rather than a target.
- **Nothing was fighting.** Those Gideons were idling. Montages, hit reactions and
  effects all cost more than standing about.
- Rung 0 reads 8.334 ms with almost no variance, which is *exactly* 120 fps — that looks
  like a 120 Hz ceiling the bench could not remove, so the empty-scene cost is probably
  lower than stated. It does not affect the useful range; 100–300 is clean.

---

## 2. Why Mass Entity is not the answer here

Mass (MassEntity / MassAI / MassCrowd, all installed) is the AAA answer and it would
render the army beautifully. It is also the wrong tool for *this* game, for a reason
that has nothing to do with performance:

**Mass entities are not Actors, and every power in this game traces to an AActor.** The
slap sweeps for `ACharacter` on the pawn channel. Refactor hunts an actor with a static
mesh and hides it. Code Vision reads components off actors. A demon that cannot be
slapped is not a demon in this game — it is wallpaper.

Given that 300 Actors run at 41 fps, paying a multi-week paradigm shift to lose every
verb the player has learned would be a bad trade at any frame rate.

---

## 3. The battlefield

`L_Meadow`. A bowl: flat floor, hills at the rim.

**Why a meadow and not a forest.** An army you cannot see is not an army. Trees occlude
the exact thing the feature exists to show, and dense foliage is overdraw — the most
expensive thing to put between a camera and a crowd. Open ground puts the whole frame
budget into pawns. It replaces nothing: the forests were already gone (v0.7.2 size
diet), and `Content/Forest` is 1 MB of orphaned materials.

The empty meadow costs 8.3 ms, which is most of a 120 fps frame for a field with
nothing in it. Worth a look before the army arrives — that is budget the demons could
be spending.

| piece | how |
|---|---|
| terrain | `Tools/Scripts/make_meadow_heightmap.py` → `HM_Meadow.png`, imported as a Landscape |
| ground | `Tools/Scripts/make_meadow_material.py` → `M_MeadowGround` |
| dressing | `Tools/Scripts/dress_meadow.py` → PlayerStart, RefuserSpawner, low sun |
| bench | `swarm.Bench` in PIE console |

**Read `make_meadow_material.py`'s header before touching any landscape material in this
project.** Getting ground onto that landscape took five passes and four unrelated
causes, all with the identical symptom. They are all written down there.

---

## 4. The fighters

**The army is free.** `BP_Gideon_Refuser` — Paragon Gideon, 224 animations including
`Primary_Attack_A/B/C`, `Cast`, `Death_Back/Fwd`, `HitReact` in four directions, run,
idle. He already attacks, flinches and dies. Nothing to author.

**The heroes need one retarget.** The five MetaHumans have 12 retargeted animations
between them — dances, a bow, a celebration — and no combat. But `RTG_UE4_to_MetaHuman`
already exists in this project and has already been used, and Paragon rides the UE4
skeleton. So:

> Retarget **Greystone's** melee set through `RTG_UE4_to_MetaHuman`.
> 174 animations: `Attack_A/B` at Fast/Med/Slow, `Ability_Q/E/R/Ultimate`.

Greystone is a swordsman where Gideon is a caster. Five swordsmen against an army of
casters is a better-looking fight than five casters against five hundred.

Side benefit: Greystone costs 321 MB of cooked data. If only his *animations* are
needed, his mesh and textures can go once the retarget is done.

---

## 5. The player, in third person

`UBattleFormComponent` — built 2026-08-26, console `battle.Toggle`.

The whole game so far is a pair of eyes. Switching to a camera behind him at the battle
means **the first time the player ever sees Leonard Sibelius is the moment he has become
something that can fight back.** That beat is free; it comes out of the switch, paid for
by every hour of first person before it. He also finally sees the five agents beside
him.

**It is an avatar, not a transformation**, and the difference is the whole game. A
71-year-old programmer who turns into a Paragon sword hero has stopped being a memoir.
An AI that *gives* him a body which can fight is the same move Kaia made in the opening
when she gave him his name — his employer calls him "Programmer" and refuses the name;
the agents hand him both. That distinction lives entirely in framing and costs nothing
to get right. Frame the grant on screen. Skip it and it is a costume change.

**A mode, not a migration.** Every power traces from the camera, tuned for a camera
inside his head; three metres back and those rays start behind his back. The office
stays first person.

### Known gap

`UBattleFormComponent` suspends the tick on the camera-trace components, which kills the
targeting half. It **cannot** stop the input half — the R binding lives on the character
and calls straight through. **Gating the power inputs on `IsInBattleForm()` is a separate
change to `ASibeliusGameCharacter` and has not been made.** Until it is, R in battle form
still rolls the menagerie on whatever the camera sees.

---

## 6. What is next, in order

1. **Retarget Greystone's melee set** onto the MetaHumans. Independent of everything
   else and it is what turns five bystanders into five fighters.
2. **Gate the power inputs** on `IsInBattleForm()` — the known gap above.
3. **Put 150 Gideons on the ridge and look at it.** Not a bench run; a composition. Does
   an army read from the meadow floor? That answers whether anything else is needed.
4. **Fight logic.** Nothing above makes them fight — they idle. This is the real work
   and none of the measurement above touches it.
5. *Deferred:* the Niagara/VAT horde and Actor promotion, if and only if 150 turns out
   not to be enough.
