# import_mixamo_dances.py — bring Walt's Mixamo dances into the game, retargeted to the agents.
#
# Walt, 2026-10-07: "would it be possible for F change her dance to pull up a big menu of dances,
# maybe all the Morro dances plus all the Mixamo dances as well" - then "download your favorite
# dances to C:\dances" - "go". 44 dances from Adobe Mixamo, FBX Binary, Without Skin, 30 fps, in
# SourceArt/Mixamo (gitignored, like Content/Mixamo: Mixamo's terms allow them in a game, not as
# loose files in a public repo), plus _YBot_TPose.fbx - Mixamo's Y Bot WITH skin, the one file
# Unreal can build Mixamo's skeleton from (a skin-less FBX imports as "no data").
#
# Steps, each skipped when its asset already exists, so a re-run only does what is missing:
#   1. Y Bot -> /Game/Mixamo/Character/SK_YBot (+ its skeleton). Never seen in the game.
#   2. every dance FBX -> /Game/Mixamo/Dances/Anim_<Name>, on Y Bot's skeleton.
#   3. IK_Mixamo (auto-characterised from Y Bot) and RTG_Mixamo_to_MetaHuman (-> IK_Metahuman,
#      the rig the Morro dances went through), chains auto-mapped, MetaHuman's retarget pose
#      auto-aligned to Mixamo's T-pose (Mixamo stands in a T, MetaHumans in an A).
#   4. every dance retargeted onto the MetaHuman skeleton: /Game/Mixamo/Retargeted/Anim_<Name>_MH -
#      what UDancerAgentComponent's DancePaths names and the F menu shows as "<Name>".
# Run INSIDE the editor (UnrealClaude), PIE stopped. Not headless: the batch retarget updates the
# Content Browser and asserts without Slate (CurrentApplication.IsValid(), 2026-10-07). Its
# output goes to RETARGET_DIR, a folder the game's DancePaths do not hold loaded until the next
# restart, so nothing it writes is in use. Saves what it makes.

import os
import unreal

TAG = "###MIXAMO###"
SRC_DIR = "C:/Users/wpark/projects/sibelius-game/SourceArt/Mixamo"
CHAR_DIR = "/Game/Mixamo/Character"
DANCE_DIR = "/Game/Mixamo/Dances"           # the imports, on Y Bot's skeleton
RETARGET_DIR = "/Game/Mixamo/Retargeted"     # the agents' copies, Anim_<Name>_MH (see step 4)
RIG_DIR = "/Game/Mixamo/Retargeting"
YBOT_FBX = "_YBot_TPose.fbx"
MH_RIG = "/Game/Characters/Retargeting/IK_Metahuman.IK_Metahuman"
MH_MESH = "/Game/MetaHumans/MHC_Aisling/Body/SKM_MHC_Aisling_BodyMesh.SKM_MHC_Aisling_BodyMesh"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def say(text):
    print("%s %s" % (TAG, text))


def asset_name(fbx):
    """'Hip Hop Shimmy.fbx' -> 'Anim_Hip_Hop_Shimmy'."""
    return "Anim_" + os.path.splitext(fbx)[0].replace(" ", "_").replace("-", "_")


def import_fbx(path, dest, name, skeleton=None):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    if skeleton:   # an animation onto an existing skeleton
        ui.set_editor_property("import_mesh", False)
        ui.set_editor_property("import_animations", True)
        ui.set_editor_property("skeleton", skeleton)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    else:          # the character that brings the skeleton
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def first_of_class(folder, cls):
    for p in eal.list_assets(folder, recursive=True):
        a = unreal.load_asset(p)
        if isinstance(a, cls):
            return a
    return None


def find_anim(name, folder=DANCE_DIR):
    path = "%s/%s.%s" % (folder, name, name)
    if not eal.does_asset_exist(path):
        return None
    a = unreal.load_asset(path)
    return a if isinstance(a, unreal.AnimSequence) else None


# --- 1. Y Bot, for the skeleton -------------------------------------------------------------
ybot = first_of_class(CHAR_DIR, unreal.SkeletalMesh)
if not ybot:
    paths = import_fbx(os.path.join(SRC_DIR, YBOT_FBX), CHAR_DIR, "SK_YBot")
    ybot = first_of_class(CHAR_DIR, unreal.SkeletalMesh)
    say("Y Bot imported: %s" % paths)
if not ybot:
    raise RuntimeError("no Y Bot skeletal mesh - nothing else can import")
skeleton = ybot.get_editor_property("skeleton")
say("Y Bot: %s, skeleton %s, %d bones" % (ybot.get_path_name(), skeleton.get_name(),
    len(ybot.get_editor_property("ref_skeleton").get_editor_property("raw_ref_bone_info")) if False else -1))

# --- 2. the dances --------------------------------------------------------------------------
fbx_files = sorted(f for f in os.listdir(SRC_DIR) if f.lower().endswith(".fbx") and not f.startswith("_"))
made, kept, failed = [], [], []
for fbx in fbx_files:
    name = asset_name(fbx)
    if find_anim(name):
        kept.append(name)
        continue
    import_fbx(os.path.join(SRC_DIR, fbx), DANCE_DIR, name, skeleton)
    (made if find_anim(name) else failed).append(name)
say("dances: %d imported, %d already there, %d FAILED %s" % (len(made), len(kept), len(failed), failed))

# --- 3. the converter: IK_Mixamo and RTG_Mixamo_to_MetaHuman -----------------------------------
mh_rig = unreal.load_asset(MH_RIG)
mh_mesh = unreal.load_asset(MH_MESH)
if not mh_rig or not mh_mesh:
    raise RuntimeError("missing %s or %s" % (MH_RIG, MH_MESH))

rig = unreal.load_asset(RIG_DIR + "/IK_Mixamo.IK_Mixamo")
if not rig:
    rig = tools.create_asset("IK_Mixamo", RIG_DIR, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    rc = unreal.IKRigController.get_controller(rig)
    rc.set_skeletal_mesh(ybot)
    rc.apply_auto_generated_retarget_definition()
    eal.save_loaded_asset(rig)
rc = unreal.IKRigController.get_controller(rig)
chains = [str(c.get_editor_property("chain_name")) for c in rc.get_retarget_chains()]
say("IK_Mixamo: root %s, %d chains %s" % (rc.get_retarget_root(), len(chains), chains))

SRC, TGT = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
# THE HANDS (Walt, 2026-10-07: "the hands distort terribly on the 44 new dances"). MetaHuman
# fingers have a one-bone METACARPAL chain each (LeftIndexMetacarpal: index_metacarpal_l) besides
# the finger chain; Mixamo fingers have none. FUZZY auto-mapping gave Mixamo's LeftIndex to BOTH,
# so the palm bone took the whole finger's rotation - measured 146-179 degrees off a relaxed hand
# (measure_dance_hands.py; the Morro dances under 60). The metacarpals are left unmapped and in
# their reference pose; the fingers still follow the dance.
METACARPAL_CHAINS = ["%s%sMetacarpal" % (side, f) for side in ("Left", "Right") for f in ("Index", "Middle", "Ring", "Pinky")]
METACARPAL_BONES = ["%s_metacarpal_%s" % (f, side) for side in ("l", "r") for f in ("index", "middle", "ring", "pinky")]
POSE = "Mixamo_T"


def configure(c):
    """The whole setup, from scratch each time - so a re-run can never stack it twice (the first
    build did: the factory's default ops plus add_default_ops, eleven ops, five duplicated)."""
    c.remove_all_ops()
    c.set_ik_rig(SRC, rig)
    c.set_ik_rig(TGT, mh_rig)
    c.set_preview_mesh(SRC, ybot)
    c.set_preview_mesh(TGT, mh_mesh)
    c.add_default_ops()
    c.assign_ik_rig_to_all_ops(SRC, rig)
    c.assign_ik_rig_to_all_ops(TGT, mh_rig)
    c.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    for chain in METACARPAL_CHAINS:
        c.set_source_chain("None", chain)
    # Mixamo stands in a T, MetaHumans in an A: pose the MetaHuman like the source to retarget
    # from - all but the metacarpals, which keep the relaxed hand they are drawn with.
    if POSE not in [str(p) for p in c.get_retarget_poses(TGT)]:
        c.create_retarget_pose(POSE, TGT)
    c.set_current_retarget_pose(POSE, TGT)
    c.reset_retarget_pose(POSE, [], TGT)
    c.auto_align_all_bones(TGT)
    c.reset_retarget_pose(POSE, METACARPAL_BONES, TGT)


def configured(c):
    return c.get_num_retarget_ops() <= 6 and all(str(c.get_source_chain(ch)) == "None" for ch in METACARPAL_CHAINS)


rtg = unreal.load_asset(RIG_DIR + "/RTG_Mixamo_to_MetaHuman.RTG_Mixamo_to_MetaHuman")
if not rtg:
    rtg = tools.create_asset("RTG_Mixamo_to_MetaHuman", RIG_DIR, unreal.IKRetargeter, unreal.IKRetargetFactory())
c = unreal.IKRetargeterController.get_controller(rtg)
rebuilt = not configured(c)
if rebuilt:
    configure(c)
    eal.save_loaded_asset(rtg)
say("RTG: %d ops %s, metacarpals unmapped: %s, target pose %s%s" % (
    c.get_num_retarget_ops(), [str(c.get_op_name(i)) for i in range(c.get_num_retarget_ops())],
    configured(c), c.get_current_retarget_pose_name(TGT), " (REBUILT)" if rebuilt else ""))

# THE RETARGETS LIVE IN THEIR OWN FOLDER (RETARGET_DIR), not beside the sources. The first set,
# with the wrenched hands, was in DANCE_DIR - and could not be deleted to make room: the game's
# DancePaths hold every dance loaded, ForceDelete failed and left them "potentially corrupt" in
# the editor (2026-10-07). So a fixed set goes to a new folder and DancePaths moves with it.

# --- 4. every dance onto the MetaHuman skeleton ------------------------------------------------
def gather_strays():
    """The batch retarget writes its copies to /Game's ROOT, not beside the source (found
    2026-10-07) - and only Content/Mixamo is gitignored. Move every one home."""
    moved = 0
    for fbx in fbx_files:
        name = asset_name(fbx) + "_MH"
        stray = "/Game/%s" % name
        if eal.does_asset_exist(stray) and not find_anim(name, RETARGET_DIR):
            if eal.rename_asset(stray, "%s/%s" % (RETARGET_DIR, name)):
                moved += 1
    if moved:
        say("moved %d retargeted dance(s) from /Game's root into %s" % (moved, RETARGET_DIR))


gather_strays()
todo = []
for fbx in fbx_files:
    name = asset_name(fbx)
    if find_anim(name) and not find_anim(name + "_MH", RETARGET_DIR):
        todo.append(eal.find_asset_data("%s/%s.%s" % (DANCE_DIR, name, name)))
if todo:
    out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(todo, ybot, mh_mesh, rtg, "", "", "", "_MH", False)
    say("retargeted %d of %d" % (len(out), len(todo)))
    gather_strays()
eal.save_directory(RETARGET_DIR, only_if_is_dirty=True, recursive=True)

# The check: every dance has its _MH, on the skeleton the Morro dances use.
morro = unreal.load_asset("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_12_MH.Anim_Slow_Rhythm_Dance_12_MH")
want = morro.get_editor_property("skeleton") if morro else None
bad = []
for fbx in fbx_files:
    a = find_anim(asset_name(fbx) + "_MH", RETARGET_DIR)
    if not a or (want and a.get_editor_property("skeleton") != want):
        bad.append(asset_name(fbx))
say("ready: %d of %d on %s; missing or on the wrong skeleton: %s" % (
    len(fbx_files) - len(bad), len(fbx_files), want.get_name() if want else "?", bad))
