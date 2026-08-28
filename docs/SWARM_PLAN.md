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
- **A CORRECTION, 2026-08-27 (later).** The bullet below blamed the navigation
  invokers for the 3.7 fps run. **That was wrong.** L_Meadow had no
  NavMeshBoundsVolume at all, so the invokers were asking for nothing and cost
  nothing — 30 chasing Refusers with invokers live measured **118 fps** once the
  log was silenced. The whole slowdown was the `[RefuserChase]` line at `Display`
  verbosity. Two things were changed in one build and the win was credited to the
  wrong one, which is the mistake this file keeps writing comments about.
  **The AI cost of a real charge is still unmeasured.**
- **The cost is thinking, not drawing — measured 2026-08-27.** 150 demons *held*
  (no chase, no navigation invoker) run at **88 fps**: `frame 11.3 ms, game 11.3,
  render 4.9, gpu 8.2`. The same 150 chasing ran at 3.7. Every Refuser injects a
  `UNavigationInvokerComponent` with 40/60 m radii and this project builds navmesh
  invoker-only, so 150 of them on a hillside asked for navmesh across most of a
  square kilometre. **Rendering an army is solved. Making one behave is the open
  problem** — which is step 4's budget, and it is CPU.
- It also surfaced a real shipping bug: `[RefuserChase]` logged at `Display`, so any
  Refuser that could not path wrote to the player's disk twice a second forever.
  43,800 lines in 145 seconds. Now `Verbose`.
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

> **Done, 2026-08-27.** `Tools/Scripts/retarget_greystone_combat.py`.
> **23 animations, not 174**, in `/Game/Characters/Retargeting/Combat/` — 15 MB
> rather than roughly 110.

The 151 left behind are MOBA traversal: `TravelMode_*`, `Spin_Jog_*`, `Turn_Left_90`,
for a hero walking a lane for twenty minutes. This game has a fight in a meadow. What
was taken is what a fight needs to *read*: idle, four jog directions, the three-swing
combo, four hit reacts, death, four abilities, Cast, four jump states — and `LevelStart`,
which is not combat at all. It is the hero-arrival animation, for the beat where the
agents **grant** him the body; §5 says that beat has to be framed or the whole thing is
a costume change, and that is the animation for it.

Two things the script's header records because each cost a run:

- `duplicate_and_retarget` wants **`AssetData`**, not loaded objects, and the flag is
  `include_referenced_assets` — the `remap_` name is from an older 5.x.
- It writes **next to the source**, inside the git-ignored 2.2 GB `ParagonGreystone`.
  Left there they would work on this machine and vanish on a clone, silently, the way
  missing vendor content always fails here. Every result is moved somewhere tracked.

**They do not cook yet**, because nothing hard-references them. PIE will play them and a
packaged build will not have them — the soft-reference trap this project has already been
bitten by once. They become real at step 4.

Montages were deliberately not taken: a montage belongs to its source skeleton.
`Attack_PrimaryA_Montage` has to be rebuilt on the MetaHuman side.

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

### The gap, closed 2026-08-27

`UBattleFormComponent` suspends the tick on the camera-trace components, which kills the
targeting half but cannot touch the input half — those bindings live on the character.
For a day, R in battle form still rolled the menagerie on whatever was over his shoulder.

`AreCameraPowersSuspended()` now lives on the character, and `CheckPowerUnlocked()` asks
it first — which covers Code Vision, Refactor **and** Compile in one edit, because all
three already funnelled through there. `DoInteract()` makes the same call by hand; it
never had a progression gate to hang it on.

The refusal does not reuse the progression wording. *"REFACTOR IS NOT YET YOURS"* is
about what he has earned; this is about where the camera is standing, and telling a
player he has not earned a power he used ten seconds ago in the office would read as
exactly the bug that banner exists to prevent. It says **NOT IN THIS BODY**, for 1.5s
rather than 3.5 — a mis-press mid-fight should not put a banner across the battle.
Interact stays silent, because E is the most-pressed key in the game.

It shipped in two halves, and the gap was written into `BattleFormComponent.h` for a day
rather than left to be found in a playtest. That is the only reason it got closed.

---

## 5b. The battle, as it stands 2026-08-27 night

Walt: *"i pressed 5 and 4 and swung - he really swings now."*

Working end to end, on keys rather than the console (**5** battle form, **4** thirty
Architects charging, **3** dismiss, **F** swing):

- 400 Refusers render at **88 fps**; 150 held cost 11.3 ms
- they **charge** — `L_Meadow` needed a NavMeshBoundsVolume *and* a RecastNavMesh the
  navigation system makes itself (Build → Build Paths)
- their **legs move** — `bUseAccelerationForPaths` defaults false, so AI path following
  never populates Acceleration, and Paragon's player-authored AnimBP blends on exactly that
- the crowd **surrounds, slows, pins and overrules** you, and swinging holds it off
- **Greystone is visible in third person**, and his sword reaches and connects

### Five things that reported success and had done nothing

Every one cost a round trip, and the pattern is one pattern:

| what lied | how it lied |
|---|---|
| `unreal_set_property` | scale 20 "succeeded", read back `1,1,1` — a "44 m Greystone that will not render" was an empty actor |
| the console | `swarm..Ridge` from an autocompleted double dot; `battle.UseAvatar 0` with the argument eaten (`LastSetBy: Constructor`) |
| the character's mesh component | mesh assigned, 2 m bounds, mainPass on, ownerNoSee off, visible, camera pointed at it — and painted nothing |
| `SetAnimInstanceClass` | sets the class without leaving `AnimationSingleNode`, so the graph never runs |
| a diagnostic probe | placed below an early-out, sampling only the instant velocity is guaranteed zero; then placed in `OnPossess` entirely by a one-tab substring anchor |

**A tool that reports success without a readback is a guess in a lab coat.** Everything
written tonight reads its values back afterwards.

## 6. What is next, in order

1. **Retarget Greystone's melee set.** Done — §4.
2. **Gate the power inputs.** Done — §5.
3. **Put 150 Gideons on the ridge and look at it.** ***Done 2026-08-27, and it
   answered three questions, two of which nobody had asked.***

   **An army reads at 120 m and not at 300.** Walt: *"that is a pretty good sized
   herd."* `swarm.Ridge 150 120 45 6` — 150 demons, a 45-degree front, six ranks,
   3.8 m between shoulders. The hills are plainly visible behind them, so the fog
   this was blamed on was never involved.

   **The battlefield was built five times too big, and that is arithmetic rather
   than taste.** Gideon is about 2.2 m. On a 1080p screen at 90° FOV:

   | distance | pixels tall | reads as |
   |---|---|---|
   | 60 m | 45 | a person |
   | 120 m | 22 | an army |
   | 150 m | 18 | a smudge |
   | 300 m | 9 | nothing |
   | 450 m — the actual rim | 6 | nothing |

   A figure needs roughly 30 px to read as a person. That is about **80 m**. The
   meadow is 1008 m across, which puts its ridge line at 400–490 m. **To stand an
   army on the rim, the landscape scale wants to be about 0.3** — no re-import
   needed, and yesterday's world-position UVs mean the grass will not stretch when
   it shrinks. Or leave it: 120 m on the floor with the hills as backdrop is the
   shot in the screenshot, and it works.

   **Density is a second variable and it was wrong too.** The failed 300 m run put
   40 demons across a full circle: one every 47 m. Even at 150 over 90° it is one
   every 19 m. That is the picket fence this file warned about and was then
   configured as anyway. Distance AND spacing, every time.

   **What this said before the looking**, kept because the guess it encoded turned out
   to be exactly right — "150 humanoids at 300 m may read as gravel." They did.

   `swarm.Ridge [count=150] [radiusMetres=300] [arcDegrees=90] [ranks=6]`
   in PIE, in `L_Meadow`. All four are arguments because **the answer is unknown** — 150
   humanoids at 300 m may read as gravel, and finding out means retyping numbers, not
   rebuilding.

   The defaults encode guesses worth naming, so a bad result can be blamed on the right
   one. An **arc, not a ring**: ringing the player is a fence, and a fence reads as a
   spawn debug rather than a threat. **Six ranks**, because 150 in a single line is a
   picket fence with sky between the dots, and depth is what makes a crowd overlap into
   a mass — this is the knob most likely to decide the answer. **Facing you**, since an
   army with its back turned is scenery.

   This is a composition question and Walt is the only one who can answer it.
4. **Fight logic.** ***They charge, as of 2026-08-27.*** The meadow needed a
   `NavMeshBoundsVolume` **and** a `RecastNavMesh` that the navigation system
   creates itself — Build → Build Paths. Adding the volume from Python does not
   trigger that, and a factory-spawned `RecastNavMesh` is worse than none: no agent
   config, and its presence stops the real one being made. Symptom to recognise:
   `LogCrowdFollowing: Unable to find RecastNavMesh instance`.

   **Engulf works.** `[Engulf] overruled at pressure 30`. Walt: *"they all came
   gliding at me like they were on skates and I slapped them all and they all went
   down in a pile."*

   Two things that leaves:

   **The skating — fixed, and it was not what the paragraph here first claimed.**
   That claim (a failed cast leaving `Speed` at zero) was wrong on both halves, and
   reading the actual node graph killed it: `Set Speed` takes `Get Velocity` off the
   **Actor** with no cast to fail, and `Set IsAccelerating` casts only to plain
   `Character`, which succeeds. The `GideonPlayerCharacter` casts that do fail are
   all in the attack-combo path and touch nothing about walking. The 196
   divide-by-zero warnings are `DeltaTime == 0` on an actor's first frame — one per
   spawn, harmless, unrelated.

   The real cause was one node in `Event Blueprint Begin Play`: **`Montage Play`,
   running Paragon's `LevelStart_Montage`** — a hero-arrival animation for a MOBA
   respawn pad, carrying a `FullBody` curve that drives the AnimBP's `FullBody` bool
   and blends the whole body off locomotion. Spawn, lock into an arrival pose, slide.
   The 150 particle warnings naming `LevelStart_Montage` were the evidence, sitting
   in the log, read as noise.

   **Fix:** `ChasePlayer` stops any non-attack montage on the first move request —
   not in `OnPossess`, because the montage has not started by then (the AnimBP's
   BeginPlay runs after possession).

   **And one slap took the lot.** 30 Refusers went down in a pile because the slap
   is a `SweepMultiByChannel`. That is the next real design question, not a bug.

   *(the original text follows)* Nothing above makes them fight — they idle. This is the real work,
   and it is also what finally hard-references the 23 animations so they cook.
5. *Deferred:* the Niagara/VAT horde and Actor promotion, if and only if 150 turns out
   not to be enough.
