# The spaceport — Generate builds something that leaves (2026-09-01)

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

## Decisions

- **Generate spawns an ACTOR, not just a mesh.** This is the foundational
  change and it is worth making on its own merits — "Generate can only produce
  one static mesh" is the real ceiling on that power, spaceport or no spaceport.
- **The rocket runs on real physics, not an animation.** Mass, thrust, gravity,
  drag, stage separation — Chaos rigid bodies. Get the thrust-to-weight wrong
  and it sits on the pad. That failure mode is the proof it is real.
- **The physics stops below orbit.** No orbital mechanics, no flight controls,
  no time warp, no staging UI. See "Where the physics stops" — this is the most
  important boundary in the document.
- **Nothing new gets bought.** 145 sci-fi meshes are already in the project.
  The rocket itself can be a cylinder, a cone and four fins; at launch distance
  under a plume, nobody can tell.
- **Every phase is playable alone.** Phase A improves Generate. Phase B is a
  building that grows. Only C and D need each other.
- **The sound arrives late.** Delay it by `distance / 343`. It costs ten
  minutes and nobody forgets it.

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

So the player *already* types a word and gets a thing, on a budget, with Mrs.
Hall reading out the refusal when it does not match. None of that needs
rewriting. The catalog simply cannot describe anything bigger than one static
mesh.

---

## Phase A — teach the catalog to spawn an actor

`FGenerateCatalogEntry` holds `TSoftObjectPtr<UStaticMesh> Mesh`. Give it a
sibling:

```cpp
/** Spawn this class instead of Mesh. Either one, never both — the actor wins. */
UPROPERTY(EditAnywhere, Category = "Generate")
TSoftClassPtr<AActor> ActorClass;
```

`UGenerateComponent` spawns the class when it is set and falls back to the mesh
when it is not. Keywords, cost, budget, refusal and provenance
(`FBranchTypes` records runtime-generated `ABuildSite`s already) all keep
working untouched, and **every existing catalog row is unaffected.**

An afternoon. It unlocks everything below it.

---

## Phase B — `ASpaceport`, the thing that assembles

A C++ actor that raises its parts out of the ground over roughly eight seconds.

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
existence.

`ABuildSite` already models phased reveal. Borrow its shape rather than
inventing a second one.

---

## Phase C — the rocket, on real physics

This is where "advanced simulation" stops being a word in a plan.

A rigid body with real mass: `SetSimulatePhysics(true)`, then `AddForce` along
its up-vector every tick. Gravity is already there. Drag is a velocity-squared
term. Stage separation is a second rigid body detaching with its own mass and
its own tumble.

**Tune thrust-to-weight below 1 and it sits on the pad.** Keep that test — a
rocket that cannot fail to launch is an animation wearing a physics costume.

Chaos does all of this natively. No custom integrator, no floating origin, no
solver of our own.

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

## Where the physics stops

**No orbit. No flight controls. No time warp. No staging UI.** It launches, it
climbs, it goes out of sight.

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
watches the rocket leave. That is the scope.

---

## Cost

Roughly a week and a half, phased.

| Phase | What lands | Rough |
|---|---|---|
| A | Generate can spawn actors | an afternoon |
| B | A spaceport grows out of the lawn | 3 days |
| C | It launches on real physics | 3 days |
| D | Plume, scorch, shake, late sound | 2 days |

**To buy: probably nothing.** A proper rocket body is one cheap Fab pack if the
primitive version disappoints, but that is an upgrade, not a dependency.

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
