# MetaHumans — intentionally NOT in version control

The MetaHuman **source** assets under `Content/MetaHumans/` are git-ignored on
purpose (see `.gitignore`, "MetaHuman source" block). Epic's MetaHuman license
discourages redistributing MetaHuman source, and this repo has a hosted remote,
so the meshes/blueprints live only on local disk.

Everything *gameplay* about these characters — the C++ (`ARefuserController`,
`ARefuserSpawner`), the spawn wiring, navigation, and level placement — **is**
versioned. Only the cosmetic MetaHuman re-skin is local. This note exists so the
re-skin is reproducible after a fresh clone (where this folder will be empty).

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
