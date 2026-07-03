# NEXT SESSION — "The Living Forest" (make the kitchen door open the real, regenerating Elsewhere)

Paste this at the start of the next session.

---

Hi. I'm Walt Parkman, 71, working on my Unreal Engine 5.7 narrative game
"Leonard Sibelius" — a hidden kitchen door marked *"Many Worlds — no two alike"*
that should open onto a procedurally-composed forest that's different every visit.
Continuation of earlier sessions.

FIRST: confirm which model you are, plainly, before we start.

HOW I LIKE TO WORK: spoon-fed, small clearly-numbered steps. Recommended option
first, then trade-offs. I paste screenshots; you guide me click-by-click in the
Unreal editor (you can't click for me). For PowerShell you give me one command at
a time and I paste the output back. I go carefully. Use the AskUserQuestion tool
for the open decisions before building.

PLEASE START BY connecting to C:\Users\wpark\projects\sibelius-game and reading:
- docs\design\Recipe_01_ModeB_Poplar_Forest.md  (the whole recipe/anchors system,
  everything built through build order #7 part 1)
- docs\sib-42-packaging-notes.md  (packaging PK-ledger + the butler release runbook)

## WHERE WE ARE (all committed to branch feat/forest-elsewhere; 0.5.1 shipped to itch)

Build orders #1–#6 done and #7 part 1 (Surprise) done, all inside the dev sandbox
level **L_Elsewhere_Dev**:
- Recipe system (PDA_WorldRecipe + DA_Recipe_01_PoplarForest, Content/Elsewhere)
  drives flora + lighting.
- BP_WorldConductor.ConductWorld (Call-In-Editor button): one seed -> reseeds +
  row-rotates the 4 biome regions, applies Sun+Fog. ~1 min for a full 4-region
  rebuild (~71k PCG tasks).
- BP_WorldAnchors rig (authored, fixed): Door, LookTarget, Hero (SM_Poplar_01_Field),
  Shinbi_A/B/C (Paragon: Shinbi x3, facing the arrival), Surprise (SM_SailBoat_01a,
  run aground on the road). Baked on the rig; holds across seed re-rolls.
- 0.5.1 packaged and pushed to itch (leonardsibelius/leonard-sibelius:windows). The
  pipeline works. Release runbook + butler path recorded in the packaging notes.

## THE PROBLEM WE'RE SOLVING (found by playing the 0.5.1 download)

The kitchen "Many Worlds" door opens **/Game/Maps/L_Poplar_Forest** (hardcoded in
the travel path -- see Source/SibeliusGameEditor/ElsewhereSmokeTestCommandlet.cpp
`ForestMapPackage` and the character's `IsAwayFromOfficeLevelName("L_Poplar_Forest")`).
But ALL of our work lives in **L_Elsewhere_Dev**, a separate dev-sandbox *copy* of
that level. So the shipped door opens the plain, original EasyBiomes forest with
none of our Conductor/anchors/hero/Shinbi/boat. Two layers must close:

**Layer 1 -- the level that ships isn't the one with our work.**
Options: (A) promote L_Elsewhere_Dev's content into L_Poplar_Forest (the door's
existing target -- cleanest for the door), or (B) repoint the door + travel code +
MapsToCook at a renamed Elsewhere level. Decide next session.

**Layer 2 -- "no two alike" doesn't run at runtime yet.**
ConductWorld is an EDITOR button. In a packaged game nobody clicks it, so we'd ship
one *static* forest (the last saved generation). To regenerate per visit we must:
run the Conductor from GAME logic on level load (GameMode/level BeginPlay), turn on
the kit's **Runtime Generation** (the checkbox we deferred in Step 3), pick a fresh
random seed per entry, and hide the ~1-min generation behind the travel cover / a
loading screen. This is roughly build orders #8-#9 (runtime generation + async load).

## THE BIG RISK -- DE-RISK THIS FIRST, before committing to the whole path

Can the EasyBiomes PCG forest actually **generate at runtime in a *packaged/Shipping*
build**? (Editor success proves nothing -- see the packaging PK-ledger, e.g. PK21:
whole engine features like CSV import vanish in cooked builds.) If PCG runtime
generation doesn't survive cooking, we pivot to Plan B: pre-bake a handful of forest
variants in-editor and have the door pick one at random (still "many worlds," just a
finite deck). Prove the runtime-generation question with a small test package BEFORE
doing the level promotion and gameplay wiring.

## OPEN QUESTIONS TO DECIDE AT THE START (AskUserQuestion)

1. Promotion method: overwrite L_Poplar_Forest with the Elsewhere content, vs
   repoint the door to a renamed Elsewhere level (+ MapsToCook + travel code).
2. Regeneration model: true runtime PCG per visit, vs a pre-baked deck of N worlds
   the door shuffles (the fallback if runtime PCG won't cook).
3. Seeding: random each entry, vs seed tied to something (date, a counter, story).
4. Loading cover: reuse the existing TravelTransitionSubsystem cover, vs a dedicated
   loading screen, to hide generation time.
5. Do the deferred **clearing mask** (#7 part 2 -- PCG exclusion volume around the
   anchors so foliage stops growing through the hero/Shinbi/boat) as part of this,
   or after.

## GOTCHAS CARRIED

- L_Elsewhere_Dev has stale WaterPlane/PostProcess ref warnings from the Save-As
  duplication (non-fatal in editor; watch them during a cook).
- Sun/Fog live in the EB_LightingDaytime SUBLEVEL; any Conductor var pointing at a
  sublevel actor must be Transient or the level won't save.
- Use the kit's Spawn/Clear (not raw PCG Generate) to avoid orphan PCGStampChild
  actors. Is Partitioned=on -> output lives in PCGWorldActor0.
- Third-party asset folders are gitignored (EasyBiomes, ParagonShinbi, Vehicles,
  Helicopter, the swept-in packs). Before any `git add -A`, `.gitignore` already
  covers them; still run the paid-asset `git status` safety check.
- Relevant C++: CathedralDoor (level door; TargetLevelName; routes OpenLevel through
  TravelTransitionSubsystem), ElsewhereGameMode, ElsewhereSubsystem,
  BranchPIEComponent (skips deploy-restore in an Elsewhere).

## FIRST ACTIONS NEXT SESSION

1. Confirm model; read the two docs above.
2. Decide the 5 open questions (AskUserQuestion).
3. **De-risk:** small test package that proves whether the PCG forest regenerates at
   runtime in a cooked build. Everything else depends on this answer.
4. Layer 1: promote the level so the door opens our worked forest.
5. Layer 2: wire the Conductor to run on level load with a fresh seed + a generation
   cover; turn on Runtime Generation.
6. Do (or schedule) the clearing mask.
7. Cook a Development package, walk through the kitchen door, confirm a fresh,
   composed forest with the hero/Shinbi/boat.
8. Ship 0.5.2 (butler runbook in the packaging notes).

## AFTER THAT (the fun Walt actually wants)

Make the boat and the helicopter (Content/Vehicles, Content/Helicopter) DRIVABLE --
a vehicle pawn + input + physics. Its own milestone; a good reward run.
