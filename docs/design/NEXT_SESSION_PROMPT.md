Hi Fable 5. I'm Walt Parkman, 71, working on my Unreal Engine 5.7 narrative game
"Leonard Sibelius" — a hidden door that opens onto a different procedurally-composed
outdoor world each time ("Many Worlds — no two alike"). Continuation of earlier sessions.
FIRST: confirm you're running as Fable 5. If you're not, tell me plainly before we start.

HOW I LIKE TO WORK: spoon-fed, small clearly-numbered steps. Recommended option first,
then trade-offs. I paste screenshots; you guide me click-by-click in the Unreal editor
(you can't click for me). I go carefully.

PLEASE START BY: connecting to my project folder at C:\Users\wpark\projects\sibelius-game
and reading BOTH of these before we begin:
  1. docs\design\Recipe_01_ModeB_Poplar_Forest.md — full design, decoded kit architecture,
     and everything built so far (recipe data asset + lighting).
  2. docs\design\Step5_Anchors_Plan.md — the plan for the step we're about to start.
Read them before we start.

WHERE WE STOPPED (all working, committed to branch feat/forest-elsewhere):
- Recipe system done: PDA_WorldRecipe (PrimaryDataAsset) + DA_Recipe_01_PoplarForest, in
  Content/Elsewhere. Drives BOTH flora and light.
- BP_WorldConductor.ConductWorld (Call-In-Editor): auto-finds the sun + fog via
  GetActorOfClass, applies the recipe's lighting (sun rotation/intensity/temperature,
  fog density) — then reseeds + row-rotates the 4 biome regions. One seed = new world.
- Build order #1–#4 complete. #4 = lighting preset per recipe (Sun + Fog), verified live.
- Gotcha resolved last session: sun/fog live in the EB_LightingDaytime sublevel, so the
  SunLight/HeightFog vars are marked Transient (else the level won't save — illegal
  cross-level reference).
- Deferred: SunColor + FogInscatteringColor captured in the recipe but not yet applied
  (both currently no-ops).

CURRENT STEP — build order #5: ANCHORS ON TERRAIN (see Step5_Anchors_Plan.md).
Core idea: anchors are an authored fixed rig (stage marks that stay put across every
generation, like the splines); the seed only decides what fills them. This keeps the
arrival view composed (art rule #3) while flora + light reshuffle around it.

FIRST, decide these 4 open questions (they're in the plan doc):
  1. Hero source — single mesh ref, or a pool the seed picks from?
  2. Anchor positions — fixed authored (recommended) or seed-jittered?
  3. Door ↔ PlayerStart — marker for now, or tie to real arrival?
  4. Clearing mask mechanism — how to keep foliage off the hero/door.

THEN the planned Step 5 tasks:
  1. Create BP_WorldAnchors (Actor) with Scene Component sockets: Door, LookTarget, Hero,
     Shinbi_A/B/C, Surprise.
  2. Place it in L_Elsewhere_Dev and hand-compose Door + LookTarget + Hero to frame the
     arrival postcard.
  3. Add HeroMesh field to PDA_WorldRecipe; set Recipe #1's hero (candidate:
     SM_Poplar_01_Field_Trunk).
  4. Wire BP_WorldConductor to place the hero mesh at the Hero socket on generation
     (clear the previous hero first, so they don't stack).
  5. Test: pilot to the Door, look along its forward — hero framed; re-roll seeds, hero
     stays framed while flora reshuffles. Then record the work in the worksheet.
  (Shinbi ×3 = #6, Surprise = #7 — designed for but not built in this step.)

Pick up at Step 5. Start by reading the two docs, then walk me through the 4 decisions,
then Task 1. Paste-a-screenshot checkpoints as we go.
