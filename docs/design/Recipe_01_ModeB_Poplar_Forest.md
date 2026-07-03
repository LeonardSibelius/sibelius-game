# World Recipe Worksheet — Recipe #1
## "Mode B Poplar Forest"

Project: Leonard Sibelius (UE 5.7) · Level: L_Poplar_Forest · Kit: EasyBiomes Poplar
Status: Step 2 complete (palette identified). Open items marked ⬜.

---

## Block A — Identity

| # | Field | Value |
|---|-------|-------|
| 1 | Recipe Name | Mode B Poplar Forest |
| 2 | Compatible Terrain Stages | Stage 1 (L_Poplar_Forest terrain) |
| 3 | Selection Weight | (deferred — default 1) |

## Block B — Vegetation Palette
*Design decision: palette references EasyBiomes **collections** (DT_..._Collection data tables), not individual meshes. Variants inside a collection count as one pick.*

| # | Field | Value |
|---|-------|-------|
| 4 | Hero Candidates | ✅ Resolved (Step 5) = **SM_Poplar_01_Field** (whole assembled field poplar: trunk+branches+leaves). NOT SM_Poplar_01_Field_Trunk — that earlier note was the bare leafless trunk (one component only). |
| 5 | Canopy Set | DT_Poplar_Collection, DT_BoxElder_Collection |
| 6 | Midstory Set | DT_Cliff_Collection, DT_Branches_01_Collection *(no bush collection in this row — bushes live in the Fiedls_With_Bushes row)* |
| 7 | Ground Scatter Set | DT_MeadowPlants_Collection, DT_GrassCurly_Collection, DT_GrassPatches_Collection, DT_Fern_Collection, DT_Nettle_Collection |
| 8 | Accent Set | (empty on purpose — Recipe #1 is the pure baseline) |

## Block C — Mood

| # | Field | Value |
|---|-------|-------|
| 9 | Lighting Preset | "Mood_ForestMorning" = current level lighting: DirectionalLight + ExponentialHeightFog (hazy coastal morning, warm-green). ⬜ Capture exact values in Step 4. |
| 10 | Audio Ambience | (deferred) |

## Block D — Composition Parameters

| # | Field | Value |
|---|-------|-------|
| 11 | Densities (canopy / midstory / scatter) | 0.6 / 0.5 / 0.8 (starting values, tune by eye in Step 3) |
| 12 | Clearing Size Range | 8–20 m radius |
| 13 | Path Width | 3 m |
| 14 | Edge Falloff | (deferred — graph default) |

## Block E — Surprise Object

| # | Field | Value |
|---|-------|-------|
| 15 | Surprise Pool Tag | "any" |
| 16 | Integration Scatter Set | (deferred — defaults to item 7) |

## Block F — Budgets & Shinbi

| # | Field | Value |
|---|-------|-------|
| 17 | Instance Budget (canopy / midstory / scatter) | 400 / 1,500 / 25,000 — ceilings, profile in Step 8 |
| 18 | Shinbi Pose/Anim Tag | (deferred — default idle) |

---

## Discovered kit architecture (reference notes)

- Each **BP_Biome_Poplar** actor = one spline region. Its Details → Biome Preset picks a **Data Table + Row Name**.
- **DT_Biome_Poplar** (Content/EasyBiomes/PCG/BiomePresets) has 4 rows = 4 pre-made looks:
  Forest_With_Meadows · Fiedls_With_Bushes (kit's typo) · Forest_Dead · Forest_Dense
- Each row = MeshCollections array + BiomeGraph + CoverageGraph + TerrainGraph.
  Forest_With_Meadows uses the 9 collections listed in Blocks B above.
- Rows share collections and vary the mix → a row is already a "mini-recipe."
  Our recipe system can work by pointing biome actors at **our own data table rows** in the same format.
- Mesh collection tables live in Content/EasyBiomes/Foliage/Trees/Poplar/ (and BoxElder/).
- ✅ Row assignments confirmed — each spline region uses a different row:
  | Actor | Row |
  |---|---|
  | BP_Biome_Poplar | Forest_With_Meadows |
  | BP_Biome_Poplar2 | Fiedls_With_Bushes |
  | BP_Biome_Poplar3 | Forest_Dense |
  | BP_Biome_Poplar4 | Forest_Dead |
  → The level = 4 hand-drawn spline regions (Walt's composition) × 4 kit looks.
  → Variation levers for Step 3: (a) permute row assignments per region, (b) reseed the PCG graphs. Splines stay fixed per terrain stage — they ARE the authored composition.

## Step 3 findings (proven in L_Elsewhere_Dev)

- Row swap works: BP_Biome_Poplar4 changed Forest_Dead → Forest_With_Meadows, regenerated correctly.
- Reseed works: PCG_Biome component → Settings → Seed (was 42, tested 19). Full re-roll = set seed on PCG_Biome, PCG_Cover, PCG_Terrain.
- Regeneration cost: **< 1 second per region** via the kit's Spawn button (Details → Controls, actor Self selected).
- Correct workflow: use the kit's **Spawn / Clear** buttons, NOT the raw PCG Generate button. Bare Generate creates orphan PCGStampChild actors (duplicate forests); we deleted 4 of these.
- With Is Partitioned = on, Spawn output lives in **PCGWorldActor0** (engine-managed) — no loose actors, actor count stable at 543.
- Generation Trigger = "Generate on Demand" — biomes only rebuild when told. Perfect for door-open regeneration.
- Door-open sequence (proven by hand): Clear → set Row Name → set Seed → Spawn, per region.
- Known cosmetic issue: Clear logs warnings about stale WaterPlane/PostProcess refs (side effect of Save-As duplication). Non-fatal.
- Hero candidate spotted: SM_Poplar_01_Field_Trunk (open-grown "field" poplar, seen in stamp ISM list).
- Dev sandbox level: **L_Elsewhere_Dev** (copy of L_Poplar_Forest). Original level untouched.

## World Conductor v1 (WORKING — built in Content/Elsewhere/BP_WorldConductor)

- Actor blueprint placed in L_Elsewhere_Dev. Variables: BiomeRegions (BP_Biome_Poplar object ref array, instance-editable, holds regions 1–4), WorldSeed (int, instance-editable).
- Custom event **ConductWorld** (Call In Editor → button on the actor's Details panel):
  ForEachLoop over BiomeRegions → Set Seed (WorldSeed + Array Index) → call **Spawn** (the kit's Controls function).
- KEY LESSON: call the kit's plain **Spawn** function (under "Controls" category). It runs the full ceremony: Cleanup (self-cleans old output — no separate Clear needed) → flags → Spawn Runtime → Generate. Calling Spawn Terrain/Biome/Cover individually does nothing; Spawn Runtime alone is gated by the "Runtime Generation" checkbox (unticked; revisit when the door opens mid-gameplay).
- Full 4-region rebuild ≈ 71k PCG tasks, progress toast bottom-right, ~1 min. One button + one number = new world. ✔
- Logic lives in parent class **BP_Biome** (/EasyBiomes/PCG/Blueprints); BP_Biome_Poplar is a data-only child.
- Seed/density quirk RESOLVED: tested seeds 10 / 1,000 / 10,000 / 50,000 — no monotonic density trend, just normal per-seed variance. Any seed is fair game.
- Bonus discovery: the kit seeds living ambience too — BP_MeadowInsects and BP_WindDeciduous actors spawn per generation under PCGPartitionGridActor (actor count varies ~544–564; engine-managed, ignore).

## Recipe Data Asset v1 (WORKING — built this session)

Scope decision: **"Core + stubs"** — core fields drive generation now; stub fields are captured for later systems (lighting, budgets, surprise/Shinbi) but nothing reads them yet.

- **Class:** `PDA_WorldRecipe` (Content/Elsewhere), parent **PrimaryDataAsset** — Asset-Manager friendly for future random-recipe selection. 15 variables, all **Instance Editable** (required so they show on the instance).
- **Instance:** `DA_Recipe_01_PoplarForest` (Content/Elsewhere) — Recipe #1 values below.

Schema + Recipe #1 values:
- **Core (read by Conductor):**
  - `BiomeDataTable` (Data Table ref) = **DT_Biome_Poplar**
  - `RowNames` (Name[]) = **[Forest_With_Meadows, Fiedls_With_Bushes, Forest_Dense, Forest_Dead]** (kit typo on #1 preserved; order = the v2 rotation order)
  - `CanopyDensity / MidstoryDensity / ScatterDensity` (float) = **0.6 / 0.5 / 0.8**
  - `RecipeName` (Text) = **"Mode B Poplar Forest"**
- **Stubs (captured, not yet read):**
  - `LightingMood` (Name) = Mood_ForestMorning
  - `CanopyBudget / MidstoryBudget / ScatterBudget` (int) = 400 / 1,500 / 25,000
  - `ClearingRadiusMin / ClearingRadiusMax` (float) = 8 / 20 (m)
  - `PathWidth` (float) = 3 (m)
  - `SurprisePoolTag` (Name) = any
  - `ShinbiPoseTag` (Name) = idle

Conductor wiring (BP_WorldConductor):
- Added `Recipe` variable (PDA_WorldRecipe ref, Instance Editable, **Default = DA_Recipe_01_PoplarForest** so the placed actor auto-loads it).
- ConductWorld now reads **Recipe.BiomeDataTable** (Get Biome Data Table → Set Biome Preset's Data Table pin) and **Recipe.RowNames** (Get Row Names → the modulo GET node), replacing the hardcoded DT_Biome_Poplar dropdown and the local RowNames variable. The old local RowNames getter node was deleted; the rotation math is unchanged: `region row = RowNames[(ArrayIndex + WorldSeed) % 4]`.

Verified: recipe-driven ConductWorld regenerates cleanly with no errors. **Seed 1 and Seed 2 produce clearly distinct worlds** (full re-deal of the 4 looks + PCG re-scatter), matching prior v2 behavior. Refactor is behavior-preserving — one editable data asset now drives the world without touching the graph. Actor count healthy (~472–587, engine-managed variance as before).

## Lighting Preset per Recipe v1 (WORKING — built this session, build order #4)

Scope: **data-driven, Sun + Fog** (chosen over reference-preset / sky / post-process). The recipe owns the mood; the Conductor applies it on generation, before the region loop.

Lighting fields on PDA_WorldRecipe (6 total, all Instance Editable):
- `SunIntensity` (Float), `SunColor` (Linear Color), `SunRotation` (Rotator), `SunTemperature` (Float) — added because the sun's warmth comes from Temperature (Use Temperature = on), not the (white) Light Color.
- `FogDensity` (Float), `FogInscatteringColor` (Linear Color).

Recipe #1 captured values (Mood_ForestMorning, read off the level actors):
- Sun: Intensity **10** lux, Color **white (1,1,1,1)**, Rotation **(55.411931, -40.400902, 70.614597)**, Temperature **5000 K**. Mobility = **Movable** (required to re-angle/tint at runtime with no lighting rebuild).
- Fog: Density **0.01**, Inscattering Color **(0,0,0,1)** — black; the visible haze actually comes from the fog's **Sky Atmosphere Ambient Contribution**, so inscattering stays black.

Conductor wiring (BP_WorldConductor):
- Two new instance-editable actor refs, `SunLight` (DirectionalLight) + `HeightFog` (ExponentialHeightFog). **Not** hand-assigned — the graph resolves them automatically with **Get Actor Of Class** at the start of ConductWorld (confirmed to work in-editor via the Call-In-Editor button). Robust to renames and reusable in any level with one sun + one fog.
- ConductWorld now begins with a lighting-apply chain, then runs the existing region loop:
  `GetActorOfClass(DirectionalLight) → Set SunLight → GetActorOfClass(ExponentialHeightFog) → Set HeightFog → Set Actor Rotation (sun = Recipe.SunRotation) → [Light Component] Set Intensity (Recipe.SunIntensity) → Set Temperature (Recipe.SunTemperature) → [Fog Component] Set Fog Density (Recipe.FogDensity) → For Each Loop (regions, unchanged)`.
- The sun's Intensity/Temperature are set via **Get Light Component** off SunLight; fog density via the ExponentialHeightFogComponent.

Deferred (captured but NOT yet applied in the graph): `SunColor` and `FogInscatteringColor` — both are currently white/black no-ops, so wiring them was skipped. Add the two Set nodes (Set Light Color, Set Fog Inscattering Color) when a future recipe needs colored light or tinted fog.

Verified live: a dramatic test (Intensity 40, Temperature 12000 K, Fog Density 0.15) produced a bright, ice-blue, heavy-fog world — confirming rotation, intensity, temperature, and fog density all fire from the recipe. Restored to captured Mood_ForestMorning values afterward.

**GOTCHA (resolved):** the EasyBiomes DirectionalLight + ExponentialHeightFog live in a **separate sublevel** (`/Game/EasyBiomes/Maps/Lighting/EB_LightingDaytime`), not in L_Elsewhere_Dev. Storing the Get-Actor-Of-Class results in the Conductor's `SunLight`/`HeightFog` variables created an illegal cross-level reference, and the persistent level refused to save ("Illegal reference to private object"). Fix: mark both variables **Transient** (Details → Advanced → Transient) — they're rebuilt every ConductWorld run anyway, so nothing is lost, and the level saves cleanly. Any future actor-ref that points at a sublevel actor must be Transient.

## Anchors on Terrain v1 (WORKING — built this session, build order #5)

Core idea proven: **anchors are an authored fixed rig** — the door/hero framing is hand-composed and stays put across every generation (like the splines); the seed only decides what fills the marks. Serves art rules #3 (composed arrival) and #4 (one hero).

Four open questions — all decided as recommended:
- **Hero source:** single mesh ref on the recipe (not a seed-picked pool). Pool is an easy superset to add later.
- **Anchor positions:** fixed authored transforms (not seed-jittered).
- **Door ↔ PlayerStart:** marker socket only for now (Door transform + forward vector). Real portal/arrival deferred.
- **Clearing mask:** decided approach = **PCG exclusion volume** sized by recipe `ClearingRadiusMin/Max`. BUILD deferred to #7.

**BP_WorldAnchors** (Actor, Content/Elsewhere) — placed in L_Elsewhere_Dev, persists across generations. Scene-Component sockets, all children of DefaultSceneRoot (flat siblings): **Door, LookTarget, Hero, Shinbi_A, Shinbi_B, Shinbi_C, Surprise**. Nudge the whole rig as a unit; Conductor reads one clean thing.

Authored transforms (this world):
- **Door** (= arrival view / postcard): rig Location **(20554, -4125, -1424)**, Rotation **(0, -6, -60.4)**. Its forward = the sightline down the path into the clearing.
- **Hero** socket: set via **Absolute (World) Location** = **(21690, -6125, -1589.72)** — ~23 m straight down the Door sightline, base grounded (End key snaps to floor). Baked as an instance override on the placed actor.

**Hero display mechanism (better than the plan's spawn/destroy):** added a **StaticMeshComponent `HeroMeshComp`** as a child of the Hero socket, mesh empty by default. The Conductor *fills that one slot* each generation. One slot = duplicates are physically impossible, nothing to clean up. (Plan originally said spawn hero + clear previous; this is simpler and stack-proof.)

Recipe additions (PDA_WorldRecipe): added **`HeroMesh`** (Static Mesh object ref, Instance Editable). DA_Recipe_01 `HeroMesh` = **SM_Poplar_01_Field**.

Conductor wiring (BP_WorldConductor.ConductWorld): off the **For Each Loop → Completed** exec (runs once after all 4 regions spawn):
`Get Actor Of Class (BP_WorldAnchors) → [Cast To BP_WorldAnchors — redundant, compiler NOTE: ReturnValue already that type; harmless, left in] → Get HeroMeshComp → Set Static Mesh (New Mesh = Recipe.HeroMesh)`. No clear step needed.

Verified live: deleted the temp reference tree, ran ConductWorld → hero appears at the socket (placed purely from the recipe). **Re-rolled World Seed → the 4 regions re-deal / flora reshuffles, hero stays fixed and framed.** The rig is seed-independent, exactly like the splines.

Lessons / gotchas from this session:
- **Piloting drags the actor:** while piloting an actor (Ctrl+Shift+P), flying the camera (WASD/RMB-drag) MOVES the actor. Composed the Door by piloting, but re-checking by re-piloting kept nudging it. Reliable method: **author transforms by typed numbers**, pilot only to *look* (no movement keys), or use the free camera.
- **Component world-space entry:** the socket transform dropdown offers **"Absolute Location"** = world space — type world coords directly, no relative-to-rotated-parent math.
- **End key** snaps a selected actor down onto the terrain (grounds the hero).
- Cross-level rule still holds: BP_WorldAnchors + HeroMeshComp live in L_Elsewhere_Dev (not a sublevel), so no Transient needed here.

To revisit (polish, non-blocking):
- Hero mesh renders **green** in-level though its thumbnail/earlier drop looked gold — confirm which look we want; a color-contrasting hero is legitimate (rule #4).
- Door "eye height" is ~1 m above the road (composed by pilot) — fine as a marker; refine if we want a precise player-eye arrival.
- DA_Recipe_01 currently shows Sun Intensity **20** / Sun Rotation Y **-40.0** — worksheet Step 4 captured **10** / **-40.400902**. Reconcile (likely a leftover from the dramatic lighting test).

## Shinbi Anchors v1 (WORKING — built this session, build order #6)

Shinbi = **Paragon: Shinbi** (Epic free character, added from Fab to `Content/ParagonShinbi/`). Skeletal mesh: `/Game/ParagonShinbi/Characters/Heroes/Shinbi/Meshes/Shinbi` (skeleton `Shinbi_Skeleton`; also ships `ShinbiPlayerCharacter` BP and a `Shinbi_Wolf` — candidate Surprise later). **Gitignored** (`Content/ParagonShinbi/`) per the third-party policy — bytes stay local, referenced by /Game path.

Decisions: **same figure ×3** (not a pool); **skeletal**; **fixed authored** positions; **static placement first** (posing via `ShinbiPoseTag` deferred).

BP_WorldAnchors: added a **SkeletalMeshComponent** under each Shinbi socket — `Shinbi_A_Mesh` / `Shinbi_B_Mesh` / `Shinbi_C_Mesh` — with **Skeletal Mesh Asset = Shinbi** assigned directly on the component (baked into the rig; skeletal meshes are Movable by default, no mobility toggle shown).

Authored positions — instance **Absolute (World) Location** on each socket, mid-distance staggered along the door sightline between the door and the hero:
- `Shinbi_A` = **(21532, -4835, -1558)**
- `Shinbi_B` = **(20854, -5565, -1565)**
- `Shinbi_C` = **(21610, -5481, -1573)**

Feet-seating height nudge (hand-tuned so each figure stands on the sloped terrain) — set on the **Shinbi sockets' Location Z: A = 110, B = 70, C = 90**. Facing: Absolute Rotation yaw 144 on all three sockets.

Verified: reseeded World Seed → flora re-dealt, all three Shinbi held their marks. They're seed-independent by construction (part of the persistent authored rig; the Conductor never touches them).

Deferred (noted for compatibility):
- **Recipe-driven Shinbi:** mesh is currently baked on the rig components (every world shows the three). To make it data-driven like the hero (different/absent figures per recipe), add a `ShinbiMesh` (Skeletal Mesh ref) field to PDA_WorldRecipe and have the Conductor `Set Skeletal Mesh Asset` on the three components each gen. Not needed for Recipe #1.
- **Posing/animation (`ShinbiPoseTag`):** figures stand in default reference pose; add idle/pose anim later.
- **Facing:** ✅ turned to face the arrival — all three set to **Absolute Rotation yaw = 144** (uniform; Shinbi mesh forward cooperated). Reads as three "watchers" flanking the approach.
- **Clearing mask (#7):** foliage can grow through the figures; the PCG exclusion volume (sized by `ClearingRadiusMin/Max`) will carve clearings around all anchors.

## Surprise object v1 (WORKING — boat placed this session, build order #7 part 1)

Surprise = **SM_SailBoat_01a** (from `Content/Vehicles/VOL16_Boats/Meshes/`; 5 variants 01a–05a; **gitignored** as `Content/Vehicles/`). A sailboat run aground *across the forest road* — "the wrong thing in the right place" (art rule #7), and on the road so it stays visible when flora re-deals (Walt's call).

Mechanism: **StaticMeshComponent `Surprise_Mesh`** under the Surprise socket, mesh = SM_SailBoat_01a, Movable — baked on the rig (like Shinbi). Positioned by dragging on the path beside Shinbi_C; final transform saved in the level (socket ≈ (22441, -6302, -1560), rot (18, -6, 20); `Surprise_Mesh` given a relative offset ≈ (82.6, -225.2, -146.8) to seat her on the path). Sits marooned on the road; could sink a touch deeper for a fully "buried" read if desired.

Deferred: recipe-driven Surprise via `SurprisePoolTag` (a pool the seed picks from) — baked one boat for now, same pattern as Shinbi.

## Runtime Generation de-risk (build order #8 — PRE-FLIGHT PROVEN, 2026-07-03)

THE question before committing to the runtime path: does the EasyBiomes PCG forest
actually generate at RUNTIME in a cooked build? (Editor success proves nothing —
see packaging PK-ledger PK21: whole engine features vanish in Shipping.) **Answer:
YES.** Proven with a throwaway Development package.

Method (one cook, isolated, shipping config untouched):
- Ticked **Runtime Generation** = on (Details → Advanced) on all 4 biome regions in
  L_Elsewhere_Dev (the checkbox deferred back in Step 3 — un-gates the kit's "Spawn
  Runtime" so it runs in a package).
- Level Blueprint: **Event BeginPlay → Print String("DERISK BeginPlay fired") → Get
  Actor Of Class(BP_WorldConductor) → ConductWorld**. NOTE: Get Actor Of Class is an
  *impure* node (has exec pins) — it MUST sit in the exec chain or its Return Value is
  null and ConductWorld gets a null Target. Chain order matters.
- Cook: `Tools/Scripts/package_pcg_runtime_test.ps1` — cooks ONLY /Game/Maps/
  L_Elsewhere_Dev via `-Map=` (no edit to DefaultGame.ini / MapsToCook), Development,
  archived to C:\Users\wpark\builds\sibelius-pcg-runtime-test. EXITCODE=0, ~3m11s
  (shaders warm from 0.5.1).
- Ran: `SibeliusGame.exe /Game/Maps/L_Elsewhere_Dev -windowed -log`. A full composed
  forest generated live (poplars + box elder + midstory + ground scatter). Runtime log
  (copied to runtime-derisk.log) confirms `DERISK BeginPlay fired` AND
  `BP_WorldConductor_C:ConductWorld` in the runtime call stack — our Conductor, not
  incidental geometry.

Non-fatal warnings observed in the cooked runtime (carry, don't block):
- `Accessed None ... WaterPlane / PostProcess in BP_Biome_C` — the stale Save-As refs
  (Recipe gotcha) now firing as runtime script warnings during generation. Clean up so
  the shipping log is quiet.
- `SkipPackage: /Game/EasyBiomes/Foliage/Trees/Poplar/SM_PoplarField_0{1,2,3}/
  SM_Poplar_0X_Field_Leaves_04 ... does not exist on disk` — a few poplar-leaf variants
  missing from the minimal cook. Forest still looked full. Before shipping 0.5.2:
  confirm whether /Game/EasyBiomes foliage needs adding to DirectoriesToAlwaysCook, or
  these are dangling kit entries to ignore.
- `Invalid material [MI_RiverMuddy] on Nanite [SM_WaterPlane] (SingleLayerWater not
  supported on Nanite)` — cosmetic. Profiler-DLL misses (aqProf/Vtune/PIX) — benign.

Consequence: the 5 open questions are settled toward TRUE RUNTIME — (1) overwrite
L_Poplar_Forest, (2) true runtime PCG per visit, (3) random seed each entry, (4) reuse
TravelTransitionSubsystem cover, (5) clearing mask after de-risk / before 0.5.2.

## Layer 1 + Layer 2a + composed arrival (build order #8, 2026-07-03) — DOOR OPENS THE WORKED FOREST

The shipped-0.5.1 problem (door opened the plain kit forest; all work lived in the
uncooked L_Elsewhere_Dev) is CLOSED.

- **Layer 1 — promotion (DONE).** Overwrote `/Game/Maps/L_Poplar_Forest` with the worked
  Elsewhere content. Method: the door/travel/smoke-test all key off the level-name
  STRING (`SauceDoor.TravelTargetLevel` = **FName** "L_Poplar_Forest"; `CathedralDoor`
  uses `FName TargetLevelName`; `TravelTransitionSubsystem.cpp:209` fires its cover on
  any name containing "Poplar_Forest" OR "Elsewhere"). So overwriting in place = ZERO
  code changes. Levels are monolithic (non-WP; 350-377 MB umaps, no external actors), so
  a Save-As is a clean one-file copy. Save-As overwrite was blocked ("Unable to overwrite
  existing package" — target loaded in memory); fix was to **Force Delete** the old
  L_Poplar_Forest (Reference Viewer confirmed NO on-disk referencers — the door is an
  FName, not an asset ref — so nothing real broke), then Save Current Level As →
  L_Poplar_Forest. Old plain forest preserved in git; L_Elsewhere_Dev kept on disk as a
  second backup. Commit 3662cd2.
- **Layer 2a — random seed each entry (DONE).** Level Blueprint: `BeginPlay → Print
  String → Get Actor Of Class(BP_WorldConductor) → Set WorldSeed = Random Integer in
  Range(0, 2e9) → ConductWorld`. Randomizing at the CALL SITE (not inside ConductWorld)
  keeps the editor button reproducible while every door-entry rolls a fresh world. Gotcha
  fixed: Get Actor Of Class is impure — must sit IN the exec chain or its Return Value is
  null; and the Set node's Target must be wired to the Conductor (defaulted to `self`).
- **Composed arrival (DONE — art rule #3).** The deferred "Door ↔ PlayerStart" gap: the
  player spawned at the level's generic PlayerStart, not the Door viewpoint — so the
  first cook-walk wandered the forest and never found the tableau. Fix: moved
  **PlayerStart onto the Door anchor** — Location **(20554, -4125, -1424)**, Rotation yaw
  **-60.4** (slightly above the road; player settles on spawn; faces down the sightline).
  Verified in PIE: spawn looks straight at the boat + three Shinbi down the road. The
  full dev game (package_v052dev_test.ps1 → sibelius-v0.5.2-dev, EXITCODE=0) confirmed the
  kitchen door opens the promoted forest and generates a random world live; the composed
  arrival needs one more cook to bake the PlayerStart move.
- **Art decision (2026-07-03):** arrival look = **EMBRACE THE FULL RANGE** — no
  arrival-region lushness pin/bias. Dead/sparse/dense/meadow all valid; a barren arrival
  is a legitimate "world." Seed stays fully random.

Still open toward 0.5.2: (Layer 2b) travel-cover timing — the dev walk didn't report a
jarring pop-in, so the existing TravelTransitionSubsystem cover may already suffice
(confirm); (#7 part 2) clearing mask so foliage stops spearing the anchors in LUSH seeds
(invisible in the dead seed); final cook with the arrival fix + butler push.

## Open items

- ⬜ Exact lighting values for Mood_ForestMorning — mostly captured in Step 4; reconcile Sun Intensity (20 vs 10) on DA_Recipe_01.
- ⬜ Identify the green triangle billboard actors (possible future anchors)
- ⬜ Framing polish: optional precise player-eye-height Door; hero green-vs-gold decision

## Build order (agreed plan)

1. ✅ Recipe schema (fields defined)
2. ✅ Recipe #1 on paper (this worksheet)
3. ✅ World Conductor v1 working: one world-seed → reseed + respawn all 4 regions.
3b. ✅ Conductor v2 — ROW ROTATION working. Each region's row = RowNames[(ArrayIndex + WorldSeed) % 4]. RowNames array var holds the 4 rows (Forest_With_Meadows / Fiedls_With_Bushes / Forest_Dense / Forest_Dead). Graph: loop → Set Seed → Set Biome Preset (Data Table=DT_Biome_Poplar, Row Name from GET(a copy) of RowNames at modulo index; struct pin split) → Spawn. Verified: region 0 reads Forest_Dense at seed 2, Fiedls_With_Bushes at seed 1. Every world contains all 4 looks, dealt to different regions.
3c. ✅ Recipe data asset (PDA_WorldRecipe + DA_Recipe_01_PoplarForest) built and wired into the Conductor — "Core + stubs" scope. Conductor reads Recipe.BiomeDataTable + Recipe.RowNames instead of hardcoded values. Verified behavior-preserving (seeds 1 vs 2 = distinct worlds). See "Recipe Data Asset v1" section below. Next: lighting preset per recipe (build order #4).
4. ✅ Lighting preset applied on generation — Sun + Fog, data-driven from the recipe (see "Lighting Preset per Recipe v1"). Post-process + colored light/fog deferred.
5. ✅ Anchors on terrain — BP_WorldAnchors rig (Door/LookTarget/Hero/Shinbi_A-C/Surprise), Door + Hero hand-composed, HeroMesh added to recipe, Conductor fills HeroMeshComp slot each gen. Verified: hero stays framed across seed re-rolls. See "Anchors on Terrain v1". (Shinbi ×3 = #6, Surprise = #7, clearing mask = #7.)
6. ✅ Shinbi placement + sanity check — Paragon: Shinbi ×3 (skeletal) baked on Shinbi_A/B/C sockets, hand-placed mid-distance on the sightline. Verified: hold marks across seed re-rolls. See "Shinbi Anchors v1". (Recipe-driven fill + posing + facing deferred.)
7. Surprise pool + integration pass
8. Async loading + budget caps, profile door-open time.
   8-preflight. ✅ RUNTIME GENERATION DE-RISK PROVEN (2026-07-03) — EasyBiomes PCG +
   ConductWorld generate a full forest at runtime in a cooked Development build. See
   "Runtime Generation de-risk" section. Path confirmed: Layer 1 (promote into
   L_Poplar_Forest) + Layer 2 (runtime Conductor w/ random seed + travel cover).
9. Recipes #2–3, terrain stage 2

## Art-direction rules (quick reference)

1. One biome rules each world (~80/15/5)
2. Lighting is the glue — one locked mood per recipe
3. The door frames a postcard — arrival view always composed
4. Exactly one hero per world
5. Density in gradients, never uniform
6. Shinbi is the figure in the landscape — on the sightline, mid-distance
7. Surprise object = the wrong thing in the right place, integrated into the ground
8. Small palette, big repetition
9. Randomness only inside fences
