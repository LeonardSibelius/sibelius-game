# The spaceport — Generate builds something that leaves (2026-09-01, rev 3)

> **rev 3 (2026-09-03)** — Phases A and B shipped in 1.1.0. Added **Phase E, the
> supply run**: Nyra appears after the spaceport is built and sends Leonard to
> uFoods for supplies, and the supplies are the permission to board. That
> promotes the "power you earn" seed into scheduled work, and it is the first
> thing in this game that needs a guide stage to change in a *running* world.

Leonard stands on the empty lawn across from Jacob's Downtown Deli. He presses
**G**, types `spaceport`, and the ground opens. A pad and a gantry assemble
themselves out of the grass, and a rocket he did not build leaves the Earth
while he watches.

**He does not fly it.** That is not a limitation, it is the point. Forty years
of assembling systems by hand, one piece at a time, and now he says a word and
something goes to space without him. A rocket he had to pilot would undercut
the whole theme; a rocket that launches because he asked for it *is* the theme.

Nyra already tells him to do this. Her second speech, recorded and shipped in
1.0, says: *"Look at the empty lawn across the street. In your old life you
would have filled it one brick at a time. You do not do that any more. Stand on
the grass, and Generate."* **No re-record is needed.** She was always pointing
at this.

---

## What the launch is FOR — the memoir goes up

Rev 1 of this plan ended with "he watches it leave." That is the physics
scope, and it stands. But a rocket that carries nothing is a firework.

The game already collects Walt's eight messages to former employers, one per
power shrine and placard (`docs/MEMOIR_VOICE.md`) — SAIC 1988, IBM 1995,
Seagate 1998, Motorola 2001, Northrop Grumman 2002, San Diego County 2005,
Bally 2007, US Army iKrome 2022. He gathers them as rewards. Nothing is ever
*done* with them.

**The spaceport is where they go.** Each rocket carries one message. Walt's
own line about them is the design brief: *"Of course they will not see the
messages, but it will be satisfying to say them."* There is no more satisfying
place to put a message nobody will read than orbit.

- A launch needs a **payload**: one collected message. You can only launch what
  you have earned, so the spaceport pays off the whole game behind it.
- **Eight launches, eight employers**, in whatever order he chooses; iKrome
  (2022, "especially iKrome", now being retired) is the natural last.
- On ignition, the message reads once — on the HUD, or spoken. After 1.0's
  face pipeline, a recorded read in Walt's own voice is a real option; that is
  a separate decision and not a dependency.
- **Every successful, Deployed launch adds one moving light to the city's
  sky.** Eight lights, eventually. They are the only stars that move.

This turns a tech showcase into the end of a memoir. It is also cheap: the
messages exist, the ceremony that shows them exists, and a moving light is a
point light on a timer.

---

## The six powers finally compose

Every power was taught alone. The spaceport is the first object in the game
that answers to all of them, and that — more than the rocket — is what makes it
the endgame.

| Key | Power | On the spaceport |
|---|---|---|
| **G** | Generate | Builds the spaceport (and, later, a fresh rocket on the pad) |
| **V** (hold) | Code Vision | **Live telemetry** — TWR, mass, velocity, altitude, dynamic pressure, stage, fuel. The simulation *shown*, not just running |
| **6 / 7 / 8** | Test-Drive | **Branch before launch.** A failed launch on a branch is *free* — discard and try again. Merge to keep it |
| **R** | Refactor | Cycles the rocket's configuration on the pad — stages, engine class, fuel load. In the office R made animals; in the city R reveals; here R redesigns |
| **C** | Compile | Pre-flight. A design with TWR < 1 or an empty tank is a *compile error*, read out by Mrs. Hall like any other refusal |
| **0** | Deploy | Makes the launch real. Undeployed, it was a test; Deployed, the satellite enters the save and the sky |

**Test-Drive is the one that matters most.** Kerbal's genius was that failure
is the fun, and the branch system already exists to make failure cost nothing.
*"In your old life a failed launch cost you. Now you branch."* `IBranchable` is
a `uint8` capture/restore, and a spaceport's state fits in a byte: empty,
rocket on pad, launched.

**Code Vision is the showcase.** The numbers are already in the physics body;
drawing them under **V** is the same HUD work the game does everywhere. It is
the difference between "look at the effects" and "look at the physics."

---

## Failure is the show

Rev 1 said TWR < 1 sits on the pad. That is the honesty test, and it stays.
But failure should be **spectacular**, because the branch makes it free.

- **Chaos Destruction.** Fracture mode ships with the engine
  (`Engine/Plugins/Experimental/ChaosEditor`). A rocket that tips, or whose
  stage separation goes wrong, becomes a Geometry Collection and comes apart —
  the one marquee UE simulation feature this game has never used, and it lands
  on a plaza full of cafe tables.
- **Wind.** A small random lateral force per launch. Some launches drift.
  Branches differ. Kerbal again: the second attempt is never the first.
- **The spent first stage comes back.** Stage separation gives it drag and a
  tumble, and it falls where physics says — sometimes on the lawn, sometimes
  on the plaza. Deploy keeps it there as a monument.

None of this needs a solver. It is Chaos rigid bodies, a fracture asset, and
one random vector.

---

## The city reacts

The AI ghosts have ignored him since he arrived. The launch is the first thing
that makes them stop.

- **Ignition: every dancing ghost freezes and faces the pad.** That is
  `TalkDanceSpeed`-style freezing plus a yaw — machinery `UDancerAgentComponent`
  already has. Cheapest emotional beat in the plan.
- **A few ghosts become the ground crew.** Teleport three to marked spots
  around the pad, idle (`GS_Idle_MH`, already cooked). The transhumans run the
  spaceport. "No animals in heaven" — but there are technicians.
- **Nyra's stage 2.** The guide-stage system built for 1.0 exists for this:
  after the first Deployed launch, `dancer_guide3_nyra` and one more entry in
  `GuideVoiceNames[]`. Her line is Walt's to write; the mechanism is done.
- **Sound.** The city goes quiet at T-minus ten. The launch is heard late,
  by `distance / 343`. The two together are what make people look up.

---

## What already exists

The typed-request half of this feature shipped in Ch6 and works:

| Piece | What it does |
|---|---|
| `UGenerateRequestWidget` | The panel he types into (**G**) |
| `UGenerateMatcher` | Keyword match against the catalog |
| `FGenerateCatalogEntry` | One row: keywords, a mesh, a Sauce cost |
| `UGenerateComponent` | Budget, resolve, spawn |
| `ABuildSite` | A thing that reveals in phases — `EBuildSiteRevealPhase` |
| `IBranchable` | `uint8` capture / restore — what Test-Drive snapshots |
| `UCodeVisionComponent` | The **V** overlay, with an `OnCodeVisionChanged` hook |
| `UDancerAgentComponent` | Guide stages, freeze-and-face, idle |

So the player *already* types a word and gets a thing, on a budget, with Mrs.
Hall reading out the refusal when it does not match. None of that needs
rewriting. The catalog simply cannot describe anything bigger than one static
mesh.

---

## Phase A — teach the catalog to spawn an actor — **DONE 2026-09-01**

`FGenerateCatalogEntry` holds `TSoftObjectPtr<UStaticMesh> Mesh`. It now has a
sibling:

```cpp
UPROPERTY(EditAnywhere, Category = "Generate")
TSoftClassPtr<ABuildSite> ActorClass;
```

**`ABuildSite`, not `AActor` — rev 2 said `AActor` and rev 2 was wrong.**
Reading the spawn path is what corrected it. *Everything* Generate creates is an
`ABuildSite`, and that is not incidental: the site is what carries `IBranchable`
(so Test-Drive can branch a launch and discard a failure for free),
`MarkGenerated` provenance (so Deploy knows which row made it), and a stable
GUID across save and re-spawn. A plain `AActor` would have spawned, looked
right, and quietly sat outside branch, Deploy and save — three systems this game
is built on. Constrained to `ABuildSite`, `ASpaceport` **inherits** all of it.

Two spawn paths existed and each named `ABuildSite::StaticClass()` itself:
`SpawnEntry` (live) and `RespawnGeneratedSite` (reload). That is two copies of
one fact, and harmless only while there is exactly one class. The day a row
spawns a subclass, they can disagree — generate a spaceport, save, reload, and
get a bare BuildSite wearing the spaceport's `EntryId`. **`ResolveSiteClass()`
is now the only place the question is answered**, and a class that fails to load
falls back to a plain site with a warning rather than refusing a request the
player already paid for.

`AuthorGeneratedSite` skips mesh authoring for a class-backed row: `ASpaceport`
builds itself out of dozens of meshes, and stamping one catalog mesh onto its
`FinalMesh` would overwrite the scale and rotation its own construction needs.

**Every existing catalog row is unaffected** — an empty `ActorClass` is exactly
the old behaviour. Keywords, cost, budget, refusal and provenance all unchanged.

It was an afternoon, it unlocks everything below it, and it was worth doing
whether or not a single rocket is ever built — "Generate can only make one
static mesh" was the real ceiling on that power.

---

## Phase B — `ASpaceport`, the thing that assembles

`ASpaceport : public ABuildSite` — a C++ actor that raises its parts out of the
ground over roughly eight seconds and owns the pad the rocket spawns onto.

Extending `ABuildSite` rather than `AActor` is what Phase A bought: branch state
(empty / rocket on pad / launched) is a `uint8` the site already
captures and restores, so **Test-Drive works on a launch the day the class
exists**, with no branching code written for it.

**Drive it from a looping FTimerManager timer, NOT `TickComponent`.** This is
not a preference. `UDancerAgentComponent` spent three rounds of debugging on a
component tick that was registered, enabled, active, and never once invoked;
the shot runs on a 60 Hz timer to this day. `FTimerManager` has never failed in
this project. Start there and skip the afternoon.

Parts are already owned — no purchase:

| Pack | Meshes |
|---|---|
| `ModularSciFiEnv_F` | 65 |
| `ModularSciFiEnv_J` | 80 |
| `SciFi_Box_B` | 10 |
| Columns, pipes | in `ModularSciFiEnv_*/Meshes/Columns`, `/Pipes` |

Pad, gantry, fuel tanks, cable runs. Each part starts below the surface and
eases upward, so the structure grows out of the lawn rather than popping into
existence. **The grass retreats as the concrete spreads** — a material
parameter or a PCG spline, and the lawn visibly becomes a spaceport rather
than having one dropped on it.

`ABuildSite` already models phased reveal. Borrow its shape rather than
inventing a second one.

---

## Phase C — the rocket, on real physics

This is where "advanced simulation" stops being a word in a plan.

A rigid body with real mass: `SetSimulatePhysics(true)`, then `AddForce` along
its up-vector every tick. Gravity is already there. Stage separation is a
second rigid body detaching with its own mass and its own tumble.

**Tune thrust-to-weight below 1 and it sits on the pad.** Keep that test — a
rocket that cannot fail to launch is an animation wearing a physics costume.

Chaos does all of this natively. No custom integrator, no floating origin, no
solver of our own.

### Real rocket science that is free — the safe side of the cliff

Five things, each a line or two, each *visible in the telemetry under V*:

1. **Fuel burns off.** Mass decreases every tick; acceleration climbs as it
   does. That is the rocket equation, happening on screen.
2. **The air thins.** Drag is `v²` times a density that falls off exponentially
   with altitude. Two terms.
3. **Max-Q.** Follows from 1 and 2 for free: dynamic pressure peaks partway up
   and then falls. Show it. It is the number real launch commentary calls out.
4. **A gravity turn.** A slow programmed pitch after clearing the tower. Not
   player control — a schedule. It is the difference between a launch and a
   firework going straight up.
5. **Wind.** One random lateral vector per launch, for variance between
   branches.

None of these touch the physics engine's internals. They are forces and a mass
property.

### The rocket's name

The `Text3D` plugin is enabled and used nowhere near this. **Leonard names the
rocket when he generates it** — he is already typing into that panel — and the
name is printed down the hull in Text3D. The employer's name is the obvious
default. *SEAGATE 1998*, on the side of the thing that leaves.

---

## Phase D — the plume

`Tools/Scripts/build_sauce_fluids.py` already retints the Niagara Fluids
templates for the sauce cauldron, and `NiagaraFluids` is enabled in the
`.uproject`. **The same machinery, pointed downward and orange.**

Then the cheap things that do most of the work:

- ground scorch decal
- camera shake on the player, falling off with distance
- **the sound delayed by `distance / 343`** — it lifts in silence, and then the
  noise arrives

---

## Phase E — the supply run, and the permission to board

*Locked 2026-09-03 (Walt). This is the seed below, promoted: the power you earn
is an errand, and the errand is what makes the rocket boardable.*

The chain, end to end:

```
spaceport materialises  ->  Nyra appears, stage 2  ->  uFoods interior
                                                          |
                            boarding unlocked  <-  supplies bought
```

Every link but one already exists. **The deli is the whole pattern already
working**: a door with an `ArrivalTag`, `UTravelTransitionSubsystem::Travel`,
`ChoosePlayerStart` matching the tag, and arrival claiming a grant that a guide
then reads. uFoods is the deli again with a purchase in the middle.

### The one architectural change

`GuideStage()` is binary and **read once at BeginPlay**. The header says so on
purpose — "that is not a simplification, it is the reason there is no walking to
write." Stage 1 works because the change happens behind a level load: he goes
into the deli, he comes out, she is somewhere new.

Phase E breaks that. The spaceport appears *while he is standing on the lawn
watching it*, and she has to arrive after it, in a world that is already
running. So:

- Stage becomes **re-evaluatable**, not read-once. Three stages, not two.
- `ASpaceport::OnGeneratedFresh()` is the trigger. It already exists and already
  fires — Phase B put it there so the spaceport could play its own arrival.
- "Has a spaceport been built?" is answered by **asking the world**
  (`TActorIterator`), never by a saved bool. `AHintVolume` already does exactly
  this, and it is right after a load, after a Test-Drive discard, and on New
  Game — all three of which a bool gets wrong.

**And it reopens a question the read-once design got to dodge: does she walk, or
does she pop?** Popping was invisible behind a level load. In view, on an open
lawn, it is a teleport. This is the first time the game has to answer it.

### A bug this will trip over on day one

`AHintVolume` has a generic `FName RequiresGrant` property that is **ignored**.
`HintVolume.cpp:70` hardcodes `HasVisitedDeli` no matter what grant is named:

```cpp
if (!RequiresGrant.IsNone() && !ASibeliusGameCharacter::HasVisitedDeli(this))
```

Any hint gated on "has supplies" would silently gate on "has visited the deli".
`FProgressionState::HasClaimed(FName)` is right there and generic. One line.
Fix it before building on it, not after.

### What already exists, and what does not

| Needed | State |
|---|---|
| Grant registry | `HasClaimed(FName)` / `Claim(FName)` — generic, done |
| Currency | **Sauce**, unified, `TrySpendSauce` — done |
| Purchase ledger | `RecordPurchase(FName)` / `GetPurchaseCount` — done |
| Travel to an interior | The deli's door + `ArrivalTag` + `ChoosePlayerStart` — proven |
| "Spaceport exists?" | `TActorIterator`, the `AHintVolume` pattern — proven |
| Boarding gate | Branch state 2 (`RocketOnPad`) reserved in Phase C |
| **`L_uFoods` interior** | **does not exist** — the real cost of this phase |
| **Stage 2 voice + face** | `dancer_guide3_nyra` + its bake — the proven recipe |
| **A shop interaction** | new; closest existing pattern is `PowerGrant`'s `BookCost` |

**Walt chose the full interior** over a storefront counter (2026-09-03). It costs
the most and it is the most convincing: the deli set the expectation that a shop
in this city is a room you walk into, and a purchase panel bolted to a facade
would be the first place the city admits it is a set.

### Stage 3 — "See you there", and the destination

Walt's recorded stage 2 line (2026-09-03) added two things the plan did not have:

> "Nice job, Leonard! You built a Spaceport using your Generate power! Next, you
> will be travelling on your new rocket to **the planet Grok**. Before you go, you
> need to go down the block to the You Foods supermarket and buy supplies.
> **See you there.**"

**The planet Grok** is the first destination the rocket has ever had. Phase C was
"it launches"; now it launches *somewhere*, which gives the telemetry something to
count down to and the memoir somewhere to arrive.

**"See you there" means she is waiting when he comes OUT** — Walt's own reading,
correcting a first guess that she would be inside the shop. That makes stage 3 the
deli beat exactly: he goes in, he comes out, she is standing there with the next
thing. And it is nearly free, for the reason the stage-1 header already gives —
**leaving uFoods RELOADS L_City**, so there is no moving-while-watched problem and
no restage needed. She is just placed at BeginPlay, like stage 1.

The marker already exists: `place_ufoods_doors.py` made a PlayerStart tagged
`uFoodsStreet` for where he lands coming out. Stage 3 stands her in front of it.

So stage 3 costs: one anchor case, one `GuideLine4`, one `dancer_guide4_nyra` bake,
and the supplies grant to gate on. No new mechanism.

### What Nyra's stage 3 line committed the game to (Walt, 2026-09-03)

Recorded as `dancer_guide4_nyra`, outside uFoods with the supplies bought:

> "We are ready to go to Grok! **I will upload myself into the spaceship computer
> and I will be going with you!** You have plenty of supplies now. **Because you
> are part AI now**, you will be able to compress your sense of time and
> **40 light years** will go by quickly! Go back to the spaceport and we will do
> the **boarding procedures**."

Four things that were not in this plan an hour ago, and all four are load-bearing:

- **Nyra comes along.** She uploads herself into the ship's computer. The guide
  does not wave him off — she is the AI aboard. That answers "who talks during a
  40-light-year flight" before the question was asked, and it means the voice
  work already done is the voice of the voyage.
- **Leonard is part AI.** Stated plainly for the first time. It is also the
  in-fiction reason the flight can be survived at all.
- **40 light years**, and time compression. The trip has a distance now, which
  Phase C's telemetry can count, and a reason the player does not sit through it.
- **"Boarding procedures"** — boarding is a *procedure at the spaceport*, not a
  door you walk through. That is a better Phase C opening than "climb in": a
  sequence with steps, which is what the six powers are for.

The rocket cannot be boarded yet, so stage 3 currently ends by pointing at
something the game does not do. That is the next piece of work, and it now has a
shape: go back to the pad, run the procedures, and leave with her aboard.

### Open, and worth deciding before code

- **What are "supplies"?** A grant alone (`City.Supplies`), or an inventory item
  he carries to the rocket? The grant is enough to gate boarding; the item is
  better if it should be visible, loseable, or shown on the pad.
- **What do they cost?** Sauce starts at 50. Price it so the errand is felt but
  never blocks — a player who cannot afford the only path forward is stuck, and
  this game has no way to grind.
- **Is there a shopkeeper?** The deli has none. A uFoods clerk is another
  MetaHuman, another voice bake, and the first NPC in the game who is not a
  dancer.

### Cost

| What lands | Rough |
|---|---|
| `HintVolume` grant fix | minutes |
| Guide stage becomes re-evaluatable + stage 2 trigger | 1 day |
| Walk-or-pop: whichever is chosen | half a day to 2 days |
| `L_uFoods` interior, door, arrival tag, grant on arrival | 2–3 days |
| The purchase interaction | 1 day |
| `dancer_guide3_nyra` voice + face bake | half a day |
| Boarding unlock wired to the supplies grant | half a day |

---

## Where the physics stops

**No orbit. No flight controls. No time warp. No staging UI.** It launches, it
climbs, it goes out of sight — and if it was Deployed, a light appears in the
sky.

The cliff is real and it is worth naming precisely. UE5 has double-precision
world coordinates, so world size is not the blocker. The blocker is that Chaos
is a rigid-body engine, not an orbital integrator. Real orbits mean bypassing
the physics engine entirely and writing a patched-conic or n-body solver, plus
a floating-origin system, plus time warp, plus the UI to fly it.

That is not a feature. That is Kerbal Space Program — and as of 2026 the
official sequel has been abandoned in Early Access since June 2024, while the
spiritual successor (Kitten Space Agency), built by the man who wrote the
original alongside an ex-SpaceX engineer, is still in pre-alpha.

**Two people who know exactly how to do this are years into doing it.** Leonard
watches the rocket leave. That is the scope. Everything in this document lives
on this side of that line.

---

## Cost, in tiers

Rev 2 adds scope, so it is split honestly. **1.1.0 is the first tier only.**
The second tier is what makes it a memoir; the third is seeds.

### 1.1.0 — the launch (about two weeks)

| Phase | What lands | Rough |
|---|---|---|
| A | Generate can spawn actors | an afternoon |
| B | A spaceport grows out of the lawn, branch-safe | 3 days |
| C | It launches on real physics, five free physics touches | 3 days |
| D | Plume, scorch, shake, late sound | 2 days |
| V | Telemetry under Code Vision | 1 day |
| — | Ghosts freeze and face the pad on ignition | half a day |

### 1.1.x — the memoir (about a week, after 1.1.0 ships)

| What lands | Rough |
|---|---|
| Payloads: one collected message per launch, read on ignition | 1 day |
| Deploy adds a moving light to the sky per launch | half a day |
| Text3D name on the hull | half a day |
| Ground-crew ghosts around the pad | half a day |
| Nyra's stage 2 (`dancer_guide3_nyra`) — Walt's words, the same bake | half a day |
| Chaos Destruction on a failed launch | 2 days |
| R redesigns the rocket on the pad; C is the pre-flight compile | 1–2 days |

### Seeds — not scheduled

- **GETTING INSIDE THE ROCKET IS A POWER YOU EARN** (Walt, 2026-09-03, after
  trying to climb it in the shipped build and finding he could not).

  > **PROMOTED THE SAME DAY — this is now Phase E above, and no longer a seed.**
  > The open question at the bottom of this entry ("whether the power is Compile,
  > a seventh verb, or something Nyra grants") was answered by Walt hours later:
  > it is an errand. Nyra appears once the spaceport is standing and sends him to
  > uFoods for supplies; the supplies are the permission. Kept here in full
  > because the reasoning below is what made Phase E obvious.

  *"tried climbing onto the spaceport from all angles but it won't let me which
  is good because getting inside the spaceship is another power you earn in
  town."*

  This turns a limitation into the design. The crew compartment is already
  built and already shipping — seven meshes a hundred metres up in the nose,
  put there because the pack included them and it cost nothing. There is
  nothing to model. What is missing is only the *permission*, which is exactly
  the shape of every other power in this game: Leonard cannot do a thing, then
  he is granted it, then he can.

  It also answers a question the seed below leaves open. If boarding is earned
  in the city, then the eighth launch has somewhere for him to be.

  Whether the power is Compile, a seventh verb, or something Nyra grants is
  Walt's call; the machinery — PowerGrant, the shrine ceremony, the memoir line
  that comes with it — all exists.

- **The eighth launch as the third door.** `[O]` is the office, `[>]` is the
  city. If iKrome is the last thing to leave, the rocket could be how Leonard
  leaves too. That is an ending, and endings are not features — it is written
  down here so nobody has to rediscover the idea.
- **A night launch.** The city is deliberately in full daylight (1.0). A plume
  lighting the plaza at dusk is a better shot; whether the launch is allowed to
  change the time of day is a design call, not a technical one.

**To buy: nothing.** A proper rocket body is one cheap Fab pack if the
cylinder-cone-fins version disappoints, but that is an upgrade, not a
dependency.

---

## Lessons carried in from 1.0

These cost real time this year. They apply directly to the work above.

- **Never load an asset in a constructor.** An `FObjectFinder` for `GS_Idle_MH`
  in `UDancerAgentComponent`'s constructor crashed the editor on *startup*,
  twice, before a window appeared. A constructor runs before the editor exists,
  so a bad asset there is not a broken animation — it is a project that will
  not open. Load on demand; cook with `DirectoriesToAlwaysCook`.
- **A `PlayerStart`'s location is its capsule CENTRE, not the floor.** Nyra
  stood a metre above the pavement until the half-height was subtracted. Any
  actor placed relative to a marker needs the same care — and read the
  half-height off the actor rather than assuming 88.
- **`SetRelativeLocation` on a ROOT component means WORLD.** It sent three
  coffee cups to the origin and made the food vanish.
- **Gitignored content reaches the pak only via `DirectoriesToAlwaysCook`.**
  A scan is not a reference. A soft path in C++ is not a reference. Verify in
  `Saved/Cooked` before pushing — the packaging script's check list exists for
  exactly this.
- **A tool that reverts your work while reporting success is worse than no
  tool.** `place_cafe_doors.py` used to recompute a door position that had been
  fixed by hand. It now inherits the transform it replaces.
- **Do not drive the rocket from `TickComponent`.** See Phase B. The tick that
  never fires is still unexplained; the timer has never failed.
