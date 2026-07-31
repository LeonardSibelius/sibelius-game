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
pawns: the assembled Blueprints derive from **`AActor`**, so they get no capsule
of their own and none of the Refuser's movement tuning applies. They stand where
they are placed and play a looping idle. A collision capsule is added by hand —
see the table; without it the player walks straight through them.

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

### Known-good edits the look and the blocking depend on

| Where | Setting | Value | Why |
|---|---|---|---|
| MetaHuman Creator → **Body** | Archetype | **Female / Medium / NormalWeight** | Determines which `Common/` skeleton assembly emits. Most Fab clothing targets *Tall* — only **parametric** outfits fit Medium. |
| MetaHuman Creator → **Assembly** | Assembly | **UE Optimized** | `UE Cine (Complete)` builds strand grooms + full-res meshes; far heavier to assemble and to run for background NPCs. |
| MetaHuman Creator → **Assembly** | Root Directory / Name | `/Game/MetaHumans` · `MHC_Aisling` / `MHC_Elise` | Keeps both characters sharing one `Common/` folder (~354 MB) instead of duplicating it. |
| `Common/Female/Medium/NormalWeight/Body/metahuman_base_skel` → **Retarget Manager → Manage Compatible Skeletons** | Add Skeleton | `/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel` | **The non-obvious one.** Assembly copies the skeleton into `/Game`, so plugin-authored animations read as a *different* skeleton and never appear in `Anim to Play`. Not bi-directional — must be set on the project skeleton. Done once; both characters share it. |
| `BP_MHC_*` → `Body` component | Animation Mode | **Use Animation Asset** | Assembly ships only post-process AnimBPs (`ABP_Body/Face/Clothing_PostProcess`). Nothing drives the skeleton, so without this they stand in A-pose. |
| `BP_MHC_*` → `Body` component | Anim to Play | `AS_MH_Neutral_Stand_Idle_Loop` | In `/MetaHumanCharacter/Optional/Animation/UEFNAnimPreset/Locomotion/`. Requires **Show Plugin Content**. MetaHuman-native — the mannequin's `MM_Idle` would need an IK retarget. |
| `BP_MHC_*` → `Body` component | Looping | **✓** | |
| `BP_MHC_*` → **add a Capsule Collision component** under `Root`, sibling of `Body` | Capsule Half Height / Radius | **88** / **34** | Assembly ships the meshes with collision off — they are authored for cinematics, where nothing bumps into them. 88 matches the 176 cm MetaHuman body; 34 is Unreal's standard character radius. |
| `BP_MHC_*` → `Capsule` | Relative Location Z | **+88** | These actors' origin is at the **feet**, so the capsule has to be raised to straddle the body. Note this is the *opposite sign* to the Refuser's −88 — that one is `ACharacter`-based, where the capsule is the root and the mesh hangs below it. Do not copy the −88 here. |
| `BP_MHC_*` → `Capsule` | Collision Presets | **BlockAllDynamic** | |

Set the animation on **`Body` only**. The face mesh follows via Leader Pose and
keeps its own `ABP_Face_PostProcess`; driving it directly fights that.

Parent the capsule to `Root`, not to `Body` — if it lands inside `Body` it
inherits the mesh's transform and sits in the wrong place. A per-bone alternative
exists (set the `Body` component's own collision to use the generated
`PHYS_MHC_*` physics asset), but the capsule is cheaper and cannot snag on a
finger bone.

Both play the same idle from `Initial Position` 0, so they breathe in sync.
Considered and deliberately left alone — offsetting one is a one-field change if
it ever grates.

### Each dancer carries her own key light

MetaHuman skin needs a **directional key** or it renders flat and grey — subsurface
scattering has nothing to scatter through. In a torch-lit room full of small point
lights you get ambient fill and no shaping, and faces read as waxworks. This cost
an evening of chasing texture and assembly theories before the answer turned out
to be "stand them somewhere brighter."

The durable fix is a **RectLight component on each dancer Blueprint**, so lighting
travels with the character instead of being re-done per level. A rect light rather
than a spot: it is a panel, so shadow falloff is soft — the softbox principle.

Starting values (tune by eye; these are a baseline, not gospel):

| Setting | Value | Why |
|---|---|---|
| Location / Rotation | X `150`, Z `170` · Yaw `180`, Pitch `-25` | in front, head height, angled down. Straight-down gives raccoon eyes; straight-on goes flat. |
| Source Width / Height | `80` / `120` | the softbox — bigger is softer |
| Temperature | `4500` K, Use Temperature ✓ | skin dies under cold light |
| Attenuation Radius | `400` | keeps the light on her, not on the room |
| Volumetric Scattering | `0` | otherwise it makes fog beams |

**Lighting Channels are the trick that makes this usable in a dark scene.** Put the
RectLight on **Channel 1 only**, and the character's `Body` and `Face` on **both
Channel 0 and 1**. Her key light then affects only her, while the room's own lights
still reach her normally — so you can light a face like a film subject without
washing out a deliberately moody room.

Optional rim light: a second RectLight behind and above, ~half intensity, cooler
(`6500` K), **Cast Shadows OFF**. That is what separates a dancer from a dark
background. Shadow-casting lights are the expensive ones; the rim does not need it.

Cost note: this is one or two dynamic lights per dancer. Fine at two or three per
level; at ten in one room it becomes the performance problem the per-level split
was meant to avoid.

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

---

## Dance animations — the mannequin → MetaHuman retarget

MetaHuman body animation does not exist as a product category. Fab's MetaHuman
channel is characters, grooms and clothing only — filtering it by "dance" returns
costumes. Animation packs live in the Unreal Engine channel and are authored on a
**mannequin** skeleton, so a retarget is unavoidable. Buy on that basis.

**The pipeline is tracked; the motion is not.** `IK_Mannequin` and
`RTG_Mannequin_to_MetaHuman` under `Content/Characters/Retargeting/` are
configuration authored against our own `SKM_Manny_Simple` and survive a fresh
clone. The retargeted output is vendor mocap resampled onto our skeleton —
git-ignored, and re-exported after re-downloading the pack.

### Buying

Check the listing for the **skeleton**, which almost nobody states. Of four packs
surveyed, only **Morro Motion** said it outright ("Updated to UE5 Mannequin"), and
that one word is the difference between a 15-minute job and a 90-minute one:

| Source skeleton | Consequence |
|---|---|
| **UE5 Mannequin** (Manny/Quinn) | `IK_Mannequin` works as-is. This is what we own. |
| UE4 Mannequin (`UE4_Mannequin_Skeleton`) | Needs a *second* IK Rig authored from scratch. The free XenoMocap pack is this. |

Also confirm the listing covers **UE 5.7** — several dance packs stop at 5.6.

### How to rebuild it

1. Re-download the pack (Morro Motion, *Dance MoCap 05*) from the Fab library.
   It installs to `Content/MorroMotion/` and **also drops raw FBX at the project
   root** as `Anim_Source/`. Both are git-ignored.
2. Open `Content/Characters/Mannequins/Meshes/SK_Mannequin` → **Retarget Manager**
   → **Manage Compatible Skeletons** → **Add Skeleton** →
   `/Game/MorroMotion/Characters/Mannequins/Meshes/SK_Mannequin`. **Save.**
3. `Content/Characters/Retargeting/IK_Mannequin` and `RTG_Mannequin_to_MetaHuman`
   are already in the repo — no need to rebuild them. If you ever do, see the
   table below.
4. Open the retargeter, select animations in the **Asset Browser**, and
   **Export Selected Animations** with Suffix `_MH`.
5. Assign to `BP_MHC_*` → `Body` → **Anim to Play**, same field as the idle.

### Known-good settings and the UI names that hide them

| Where | What | Why |
|---|---|---|
| `SK_Mannequin` → Retarget Manager → **Manage Compatible Skeletons** | add Morro's `SK_Mannequin` | **The load-bearing one.** Their animations sit on *their* copy of the mannequin skeleton; our IK Rig is on *ours*. Without this entry the animations never appear in the retargeter's Asset Browser and the whole thing looks broken. Not bi-directional — set it on ours. |
| `IK_Mannequin` | built on our `SKM_Manny_Simple` | Three other `SKM_Manny_Simple` copies exist, all inside **git-ignored vendor folders**. Picking one of those puts the pipeline on a foundation absent from a fresh clone. Hover the picker to check the path. |
| IK Rig editor | **Import Hierarchy** (green button, Hierarchy panel) | This is how an IK Rig takes its mesh. It is *not* in Preview Scene Settings, and the asset is created empty without asking. |
| IK Rig editor | right-click pelvis → **Set Pelvis** | This is what "set the retarget root" is called in 5.7. |
| IK Rig editor | toolbar → **Auto Create Retarget Chains** | Builds spine/arms/legs/head automatically. Don't author chains by hand. |
| Retargeter | "Assign IK Rig to All Ops?" → **Assign** | 5.7's retargeter is an Ops stack; each op needs the rig. Answering No leaves them unassigned. |
| Retargeter → **Target Preview Mesh** | `SKM_MHC_Aisling_BodyMesh` | Exported animations inherit this mesh's skeleton, so they land on *our* MetaHuman skeleton rather than the plugin's. |
| Export dialog | **Use Source Path — UNCHECKED** | Ticked, it writes output into `/Game/MorroMotion/`, i.e. inside the ignored vendor folder. The one setting that silently undoes the whole arrangement. |

### Non-issues that look like problems

- **Two figures, four arms** in the retargeter viewport — both preview meshes draw
  at the origin. Cosmetic; separate them with **Preview Offset → Source Mesh Offset**.
- **Aisling has no head** — `SKM_MHC_*_BodyMesh` is the body only; the head is a
  separate Face mesh that follows via Leader Pose at runtime.
- **`Root ... chain too short` and `Root Motion Remap Op, missing root bone`** in
  the retarget log — expected. The Root chain is a single bone, and root motion is
  unused: these play through `Anim to Play`, which does not consume it.
- **Exported assets don't appear on disk** — the batch export creates them dirty
  in memory. **Save All** before looking for the files.
