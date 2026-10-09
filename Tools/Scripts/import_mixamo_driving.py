# import_mixamo_driving.py — the agents' driving pose for the Transhuman Grand Prix.
#
# Walt, 2026-10-09: "is it possible to put the metahumans in the karts" - then picked Mixamo's
# "Driving" (Male Driving A Car, 151 frames, default sliders), downloaded FBX Binary, Without Skin,
# 30 fps to SourceArt/Mixamo/Driving (gitignored like the dances; its own folder so
# import_mixamo_dances.py, which reads SourceArt/Mixamo's top level, never takes it for a dance).
#
# Steps, each skipped when its asset already exists:
#   1. Driving.fbx -> /Game/Mixamo/Driving/Anim_Driving, on Y Bot's skeleton (the dance import made it).
#   2. retargeted with the dances' RTG_Mixamo_to_MetaHuman -> /Game/Mixamo/Driving/Anim_Driving_MH.
#   3. and with its own RTG_Mixamo_to_Manny -> Anim_Driving_Manny, for Leonard's driver.
# Then bake_kart_driving.py seats both in the kart.
# Run INSIDE the editor (UnrealClaude), PIE stopped, after import_mixamo_dances.py has made Y Bot
# and the retargeter. Saves what it makes.

import os
import unreal

TAG = "###DRIVING###"
SRC = "C:/Users/wpark/projects/sibelius-game/SourceArt/Mixamo/Driving/Driving.fbx"
DIR = "/Game/Mixamo/Driving"
NAME = "Anim_Driving"
YBOT = "/Game/Mixamo/Character/SK_YBot.SK_YBot"
RTG = "/Game/Mixamo/Retargeting/RTG_Mixamo_to_MetaHuman.RTG_Mixamo_to_MetaHuman"
MH_MESH = "/Game/MetaHumans/MHC_Aisling/Body/SKM_MHC_Aisling_BodyMesh.SKM_MHC_Aisling_BodyMesh"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def say(text):
    print("%s %s" % (TAG, text))


def anim(name):
    path = "%s/%s.%s" % (DIR, name, name)
    a = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    return a if isinstance(a, unreal.AnimSequence) else None


ybot = unreal.load_asset(YBOT)
rtg = unreal.load_asset(RTG)
mh_mesh = unreal.load_asset(MH_MESH)
if not ybot or not rtg or not mh_mesh:
    raise RuntimeError("run import_mixamo_dances.py first: missing %s" % [p for p, a in
                       ((YBOT, ybot), (RTG, rtg), (MH_MESH, mh_mesh)) if not a])

# --- 1. the import ----------------------------------------------------------------------------
if not anim(NAME):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("skeleton", ybot.get_editor_property("skeleton"))
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SRC)
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
source = anim(NAME)
if not source:
    raise RuntimeError("the import made no %s/%s" % (DIR, NAME))
say("source %s: %.2f s" % (source.get_path_name(), source.get_editor_property("sequence_length")
                           if hasattr(source, "sequence_length") else unreal.AnimationLibrary.get_sequence_length(source)))

# --- 2. onto the MetaHuman skeleton -------------------------------------------------------------
MH = NAME + "_MH"
if not anim(MH):
    stray = "/Game/%s" % MH   # the batch retarget writes to /Game's root (import_mixamo_dances.py)
    if not eal.does_asset_exist(stray):
        unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            [eal.find_asset_data(source.get_path_name())], ybot, mh_mesh, rtg, "", "", "", "_MH", False)
    if eal.does_asset_exist(stray):
        eal.rename_asset(stray, "%s/%s" % (DIR, MH))
eal.save_directory(DIR, only_if_is_dirty=True, recursive=True)
target = anim(MH)
say("retargeted: %s on %s" % (target.get_path_name() if target else "MISSING",
                             target.get_editor_property("skeleton").get_name() if target else "-"))

# --- 3. onto Manny, Leonard's driver (Walt: "can the player's kart driver be Manny") -------------
# Its own retargeter, set up the way import_mixamo_dances.py sets up the MetaHuman one: Y Bot's
# IK_Mixamo to the template's IK_Mannequin, chains auto-mapped, the metacarpals left out (Mixamo
# has none, and fuzzy mapping wrenched the agents' palms), Manny posed in Mixamo's T to start from.
MANNY_RIG = "/Game/Characters/Retargeting/IK_Mannequin.IK_Mannequin"
MANNY_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
MIXAMO_RIG = "/Game/Mixamo/Retargeting/IK_Mixamo.IK_Mixamo"
RTG_MANNY = "/Game/Mixamo/Retargeting/RTG_Mixamo_to_Manny"
SRC_SIDE, TGT_SIDE = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
manny_rig, manny_mesh, mixamo_rig = (unreal.load_asset(p) for p in (MANNY_RIG, MANNY_MESH, MIXAMO_RIG))
if not manny_rig or not manny_mesh or not mixamo_rig:
    raise RuntimeError("missing %s" % [p for p, a in ((MANNY_RIG, manny_rig), (MANNY_MESH, manny_mesh),
                                                    (MIXAMO_RIG, mixamo_rig)) if not a])
rtg_manny = unreal.load_asset(RTG_MANNY + "." + RTG_MANNY.split("/")[-1])
if not rtg_manny:
    rtg_manny = tools.create_asset(RTG_MANNY.split("/")[-1], RTG_MANNY.rsplit("/", 1)[0],
                                   unreal.IKRetargeter, unreal.IKRetargetFactory())
    c = unreal.IKRetargeterController.get_controller(rtg_manny)
    c.remove_all_ops()
    c.set_ik_rig(SRC_SIDE, mixamo_rig)
    c.set_ik_rig(TGT_SIDE, manny_rig)
    c.set_preview_mesh(SRC_SIDE, ybot)
    c.set_preview_mesh(TGT_SIDE, manny_mesh)
    c.add_default_ops()
    c.assign_ik_rig_to_all_ops(SRC_SIDE, mixamo_rig)
    c.assign_ik_rig_to_all_ops(TGT_SIDE, manny_rig)
    c.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    chains = [str(ch.get_editor_property("chain_name")) for ch in unreal.IKRigController.get_controller(manny_rig).get_retarget_chains()]
    metacarpals = [ch for ch in chains if "Metacarpal" in ch]
    for ch in metacarpals:
        c.set_source_chain("None", ch)
    c.create_retarget_pose("Mixamo_T", TGT_SIDE)
    c.set_current_retarget_pose("Mixamo_T", TGT_SIDE)
    c.auto_align_all_bones(TGT_SIDE)
    c.reset_retarget_pose("Mixamo_T", ["%s_metacarpal_%s" % (f, s) for s in ("l", "r") for f in ("index", "middle", "ring", "pinky")], TGT_SIDE)
    eal.save_loaded_asset(rtg_manny)
    say("RTG_Mixamo_to_Manny made: %d ops, metacarpal chains left out %s" % (c.get_num_retarget_ops(), metacarpals))

MANNY = NAME + "_Manny"
if not anim(MANNY):
    stray = "/Game/%s" % MANNY
    if not eal.does_asset_exist(stray):
        unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            [eal.find_asset_data(source.get_path_name())], ybot, manny_mesh, rtg_manny, "", "", "", "_Manny", False)
    if eal.does_asset_exist(stray):
        eal.rename_asset(stray, "%s/%s" % (DIR, MANNY))
eal.save_directory(DIR, only_if_is_dirty=True, recursive=True)
eal.save_directory(RTG_MANNY.rsplit("/", 1)[0], only_if_is_dirty=True, recursive=True)
manny_anim = anim(MANNY)
say("retargeted: %s on %s" % (manny_anim.get_path_name() if manny_anim else "MISSING",
                             manny_anim.get_editor_property("skeleton").get_name() if manny_anim else "-"))
