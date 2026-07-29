# MetaHumans — intentionally NOT in version control

The MetaHuman **source** assets under `Content/MetaHumans/` are git-ignored on
purpose (see `.gitignore`, "MetaHuman source" block). Epic's MetaHuman license
discourages redistributing MetaHuman source, and this repo has a hosted remote,
so the meshes/blueprints live only on local disk.

Everything *gameplay* about these characters — the C++ (`ARefuserController`,
`ARefuserSpawner`), the spawn wiring, navigation, and level placement — **is**
versioned. Only the cosmetic MetaHuman re-skin is local. This note exists so the
re-skin is reproducible after a fresh clone (where this folder will be empty).

Two *other* ignored zones feed these characters and are equally invisible to a
fresh clone: `Content/Fab/` holds the MetaHuman Character (`MHC_*`) source
assets, and `Content/Paid/` holds purchased outfit packs. Both are re-downloadable
from the Fab library on the Epic account that bought them.

---

## Refuser — `MH_Refuser/BP_MH_Refuser`

The Ch3 (SIB-27) antagonist. A MetaHuman re-skin driven as a pawn by
`ARefuserController` (versioned in `Source/SibeliusGame/`) and spawned by
`ARefuserSpawner`.

### How to rebuild it
1. Create a MetaHuman (or import the project's existing one) into
   `Content/MetaHumans/MH_Refuser/` so the asset path is
   `Content/MetaHumans/MH_Refuser/BP_MH_Refuser`.
2. Point the spawner's `RefuserClass` (or the level's Refuser actors) at
   `BP_MH_Refuser`.

### Known-good edits the gameplay depends on
These were tuned during Ch3 Phase 3 so the capsule sits on the floor and the
Refuser can climb the built attic stairs. Reapply them on the BP after a rebuild:

| Where | Setting | Value | Why |
|---|---|---|---|
| `BP_MH_Refuser` → root `CapsuleComponent` | Relative Location Z | **−88** | Seats the MetaHuman mesh on the capsule base so it spawns on the floor, not buried/floating. |
| `BP_MH_Refuser` → `CharacterMesh0` (inherited mesh) | Relative Location Z | **0** | Mesh offset moved up to the root; the mesh component itself stays at 0. |
| `BP_MH_Refuser` → `CharacterMovement` | Max Step Height | **100** | Lets the Refuser mount the built staircase steps without stalling. |

Spawn-time vertical nudge lives on the **spawner** (`SpawnZNudge = 100`,
versioned) — not here — and is what lifts the pawn clear of the spawn surface.

> If you change any of these in-editor, update this table in the same commit.
> The assets stay ignored; this note is the source of truth for their config.

---

## AI Temple agents — `MHC_Aisling/BP_MHC_Aisling`, `MHC_Elise/BP_MHC_Elise`

Two feminine MetaHumans flanking the throne in `L_AI_Temple`. Set dressing, not
pawns: the assembled Blueprints derive from **`AActor`**, so there is no capsule
and none of the Refuser's capsule/movement tuning applies. They stand where they
are placed and play a looping idle.

`Content/Maps/L_AI_Temple.umap` **is** tracked and references both Blueprints, so
a fresh clone opens that level with two missing actors until these are rebuilt.

### How to rebuild them

1. Re-download the two MetaHuman Character source assets from the Fab library into
   `Content/Fab/MetaHuman/` → `MHC_Aisling.uasset`, `MHC_Elise.uasset`.
2. Re-download the outfit (Jels Studio, *"Parametric and recolorable Dress with
   lace details — made for MetaHumans"*, Standard License, UE 5.7 build
   `oa_aput.mhpkg`). In the Content Browser create `Content/Paid/Outfits/Aput/`
   and drag the `.mhpkg` into it — this must go through the Content Browser, not
   a file copy. Yields `OA_Aput` + `WI_Aput`.
3. Open each `MHC_*` asset. Toolbar → **Download Texture Sources** and let it
   finish (needs an Epic sign-in; `Assemble` stays greyed out until it completes).
4. **Hair & Clothing** tab → drag `WI_Aput` into the *All Assets* panel →
   double-click to apply. Recolor via `MPC_Aput` in the package's Material folder.
5. **Assembly** tab → settings per the table below → **Assemble**.
6. Apply the Compatible Skeletons entry and the idle-animation settings below.

### Known-good edits the look depends on

| Where | Setting | Value | Why |
|---|---|---|---|
| MetaHuman Creator → **Body** | Archetype | **Female / Medium / NormalWeight** | Determines which `Common/` skeleton assembly emits. Most Fab clothing targets *Tall* — only **parametric** outfits fit Medium. |
| MetaHuman Creator → **Assembly** | Assembly | **UE Optimized** | `UE Cine (Complete)` builds strand grooms + full-res meshes; far heavier to assemble and to run for background NPCs. |
| MetaHuman Creator → **Assembly** | Root Directory / Name | `/Game/MetaHumans` · `MHC_Aisling` / `MHC_Elise` | Keeps both characters sharing one `Common/` folder (~354 MB) instead of duplicating it. |
| `Common/Female/Medium/NormalWeight/Body/metahuman_base_skel` → **Retarget Manager → Manage Compatible Skeletons** | Add Skeleton | `/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel` | **The non-obvious one.** Assembly copies the skeleton into `/Game`, so plugin-authored animations read as a *different* skeleton and never appear in `Anim to Play`. Not bi-directional — must be set on the project skeleton. Done once; both characters share it. |
| `BP_MHC_*` → `Body` component | Animation Mode | **Use Animation Asset** | Assembly ships only post-process AnimBPs (`ABP_Body/Face/Clothing_PostProcess`). Nothing drives the skeleton, so without this they stand in A-pose. |
| `BP_MHC_*` → `Body` component | Anim to Play | `AS_MH_Neutral_Stand_Idle_Loop` | In `/MetaHumanCharacter/Optional/Animation/UEFNAnimPreset/Locomotion/`. Requires **Show Plugin Content**. MetaHuman-native — the mannequin's `MM_Idle` would need an IK retarget. |
| `BP_MHC_*` → `Body` component | Looping | **✓** | |

Set the animation on **`Body` only**. The face mesh follows via Leader Pose and
keeps its own `ABP_Face_PostProcess`; driving it directly fights that.

Optional polish, not currently applied: give one of the two a non-zero
**Initial Position** (~`1.7`) on the same component so the pair don't breathe in
lockstep from level start.

### Gotchas that cost time the first go

- **Assembly needs ≥ 10 GiB free RAM** and refuses below that. The MetaHuman
  Creator editor alone holds ~11 GB (it loads the whole groom/template library).
  Load an **Empty Level**, close other apps, and assemble one character at a time.
- **Download Texture Sources deactivates the Assembly tool.** The panel goes blank
  and clicking the already-selected Assembly tab does nothing. Click any other tab,
  then Assembly, to force it to rebuild.
- **Re-assembly overwrites `BP_MHC_*`.** Do wardrobe and body changes *first*,
  assemble, and apply the animation settings **last** — otherwise they are silently
  discarded.

> Same rule as above: change any of this in-editor and update these tables in the
> same commit. The assets are ignored; this note is all a fresh clone gets.
