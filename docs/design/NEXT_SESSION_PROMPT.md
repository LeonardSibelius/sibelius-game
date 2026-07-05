Hi. I'm Walt Parkman, 71, working on my Unreal Engine 5.7 narrative game
"Leonard Sibelius." The Many Worlds door now works (v0.5.4 shipped): a deck
of eight baked EasyBiomes poplar forests, shuffled per entry. This session
brings the forests to LIFE: the three Shinbi watchers become followers who
defend me, and the Refusers get a new demonic body with real attack
animations.

FIRST: confirm which model you are, plainly, before we start.

HOW I LIKE TO WORK: spoon-fed, small clearly-numbered "bites." Recommended
option first, then trade-offs. I paste screenshots; you guide me
click-by-click in the Unreal editor (you can't click for me). For
PowerShell you give me one command at a time and I paste the output back.
I go carefully. Use the AskUserQuestion tool for the open decisions before
building.

PLEASE START BY connecting to C:\Users\wpark\projects\sibelius-game and reading:


docs\design\NEXT_SESSION_PROMPT.md  (this file)
docs\design\Recipe_01_ModeB_Poplar_Forest.md  (whole recipe/anchors/deck
system, INCLUDING the bottom sections: Plan B deck, PlayerStart lesson,
Shinbi-on-the-road coordinates)
docs\sib-42-packaging-notes.md  (packaging + butler runbook)


WHERE WE ARE (v0.5.4 shipped to itch; feat/forest-elsewhere MERGED to main)


Build order #2 DONE — BP_Gideon_Refuser in Content/Characters, spawners repointed, Shinbi BlockAll, gates 12/12. Resume at build order #3 (forest nav).

The Sauce Door shuffles a deck of 8 baked forests (L_Forest_01..08,
Content/Maps). C++: ASauceDoor.TravelTargetLevels, random no-repeat pick.
All 8 in MapsToCook. Smoke gates 12/12 green.
Every deck level has a PlayerStart at the Door anchor (~20554, -4125,
-1350, yaw -60.4). LESSON LEARNED THE HARD WAY: PIE spawns at the editor
camera when a level has no PlayerStart — only a packaged build proves
arrival. Never trust PIE for spawn position.
BP_WorldAnchors rig (Content/Elsewhere) carries the composed layer in
every deck level: Door/LookTarget/Hero/Shinbi_A-C/Surprise. The three
Shinbi_X_Mesh components idle-animate (Animation Mode = Use Animation
Asset, Paragon idle) and have Collision Presets = BlockAll (set INSIDE
the blueprint — editing the level instance silently doesn't stick).
Shinbi stand at the ROAD EDGE (the authored splines keep the road clear,
so no seed can impale them — coordinates in the worksheet).
EasyBiomes kit is STOCK (Is Partitioned ON, Runtime Generation off).
Runtime PCG in cooked builds is a dead end — proven, documented in the
worksheet. Do not re-fight that battle.
NEW ASSET: Paragon: Gideon in Content/ParagonGideon/ (gitignored —
VERIFY with git status before any git add). Chosen look: the
Gideon_Mephisto skeletal mesh (Skins/Mephisto/Meshes/Gideon_Mephisto)
— red demonic skin — PLUS SM_DemonHornAttachment (static mesh, same
folder): the horns are a separate attachment that must be socketed to
the head. Pack includes animations, AnimBPs, skins, FX.
License notes: may not use the trademark PARAGON in marketing; Fab
listing says "Allows usage with AI: No" (read as: no feeding the asset
into generative-AI tools; record the reading in the asset-license notes).


THE MILESTONE

Two headline features, one supporting lift:

A. Demonic Refusers. Replace the MetaHuman Refuser body with
Gideon_Mephisto + horns + the pack's attack/ability animations. The chase
logic (ARefuserController, ARefuserSpawner, slap-ragdoll via
USlapComponent) is body-agnostic; this is a mesh/AnimBP swap on the
Refuser character BP plus montage wiring. Gideon ships a physics asset,
so the slap-ragdoll should survive the swap — verify.

B. Shinbi bodyguards. In the Many Worlds, the three watchers follow me
and attack Refusers that appear. They are currently static mesh components
on the rig — they must become Characters (pawn + movement + small AI).
The follow pattern already exists in the codebase: ARefuserController
chases with MoveToActor re-issued when path-following idles; a follower is
the same with a friendly leash (follow radius, stop distance).

C. Supporting lift: forest navigation. Nothing has ever walked in the
forests — the deck levels have NO nav mesh. AI movement needs a
NavMeshBoundsVolume (+ RecastNavMesh settings) per level. Per-level actors
mean either an 8-level lap (like the PlayerStart fix) or a full deck
re-bake from L_Elsewhere_Dev (seeds are recorded in the worksheet).

BUILD ORDER (agreed plan — de-risk order, cheapest reversible first)


Safety pass. git status (ParagonGideon must NOT appear); record
license notes; confirm gates still 12/12 before touching anything.
Refuser reskin (office, self-contained). Swap the Refuser BP's
mesh to Gideon_Mephisto, socket SM_DemonHornAttachment to the head,
assign the pack AnimBP (or Animation-Asset mode + montages). Verify in
the office: chase still works, slap still ragdolls, RefuserSmokeTest
green. This ships value even if the rest slips.
Nav in the workbench. NavMeshBoundsVolume in L_Elsewhere_Dev sized
to the playable bowl; verify with the nav-debug view; watch nav-data
build time and size (big landscape — may need tuning/bounds trimming).
BP_ShinbiCompanion. A Character with the Shinbi mesh + follow AI.
PROVE IN PIE FIRST with one companion in L_Elsewhere_Dev; remember the
packaged-build-is-truth rule before declaring victory.
Refusers in the forest. Place/enable an ARefuserSpawner in the dev
level (waves modest — Nanite forest + several characters = watch perf).
Combat. Shinbi attack montage on proximity to a Refuser; Refuser
attack montage (Gideon ability anims) on proximity to Shinbi/player;
damage model per the open questions below. Reuse ragdoll as the death.
Propagate to the deck. Decide: 8-level lap (paste nav volume +
spawner, like the PlayerStart fix) vs re-bake from dev (seeds in the
worksheet; re-vet each world — Shinbi/boat/hero/arrival ritual).
Gates, package, cooked-build door dance, ship (0.5.5 or 0.6 per
open question). Butler runbook as usual. Update worksheet + README.


OPEN QUESTIONS TO DECIDE AT THE START (AskUserQuestion)


Do all THREE Shinbi follow, or one (the other two hold the postcard)?
Composition vs. spectacle.
Do the statues "wake" when approached (nice narrative beat: the
watchers come alive) or follow from arrival?
Can Shinbi be hurt/die? (Recommend: no health, they can be knocked
back but always recover — keeps the tone wonder, not war.)
Do forest Refusers threaten ME (damage/fail state) or only tangle with
the Shinbi (theatrical combat)? (Recommend theatrical first — no fail
state in a wander world.)
Version: 0.5.5 (feature drop) or 0.6 (the forests-come-alive release)?
Branch name (suggest feat/watchers-and-demons, off main).


GOTCHAS CARRIED


PIE LIES about spawn position (no-PlayerStart → editor camera) and about
cooked-build behavior generally. The packaged build is the only truth.
Component edits must happen INSIDE the blueprint (BP_WorldAnchors etc.),
not on a level instance — instance edits can silently not stick.
EasyBiomes stays stock. No Runtime Generation, no Is Partitioned
changes. The deck is the architecture.
Third-party folders gitignored: EasyBiomes, ParagonShinbi, ParagonGideon,
Vehicles, Helicopter, MetaHumans, the swept-in packs. Run the paid-asset
git status check before every git add -A.
The kit's Spawn/Clear buttons (not raw PCG Generate) for any re-bake.
Cross-level actor refs to sublevel actors must be Transient (EB lighting
sublevel) or the level won't save.
Deck re-bakes must end with the vetting ritual: Shinbi ×3 unpierced,
boat, hero, arrival postcard, PlayerStart present.
MetaHuman Refuser body is rebuilt locally (not in git) — if the Gideon
swap replaces it entirely, note what happens on a fresh clone.


FIRST ACTIONS NEXT SESSION


Confirm model; connect to the project; read the three docs above.
Run the safety pass (build order #1) and paste git status.
Decide the six open questions (AskUserQuestion).
Start build order #2 — the Refuser reskin — in bites.


AFTER THAT (still queued from last time)

Make the boat and the helicopter (Content/Vehicles, Content/Helicopter)
DRIVABLE — a vehicle pawn + input + physics. Its own milestone; a good
reward run. (A drivable boat in a forest whose surprise object is a
grounded boat is almost too good.)