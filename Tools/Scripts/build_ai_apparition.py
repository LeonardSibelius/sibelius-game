# build_ai_apparition.py — the opening AI apparition (see docs/ai-apparition-notes.md).
#
# ORDER (AP5): Build.bat FIRST (this script SPAWNS AAIApparition), then run
# this with the OFFICE level open (L_Office_v02). Does three things,
# idempotently:
#   1. Creates /Game/AIApparition/M_ai_core — unlit white-gold core material
#      with a "Glow" scalar the actor animates (flare -> pulse -> fade).
#   2. Imports Tools/Audio/ai_intro.wav (Cowork-converted from Walt's
#      ElevenLabs MP3) as /Game/AIApparition/S_ai_intro. Skips with a warning
#      if the WAV isn't there yet — the actor runs silent until it is (AP3).
#   3. Spawns an AIApparition 350 cm in front of the PlayerStart at core
#      height 150 cm (AP6: if the PlayerStart faces a wall, drag the actor).
#
# RUN NATIVELY (editor Cmd box, never the bridge):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_ai_apparition.py"

import os
import unreal

TAG = "###APPARITION###"
PKG_DIR = "/Game/AIApparition"
WAV = "C:/Users/wpark/projects/sibelius-game/Tools/Audio/ai_intro.wav"
FORWARD_CM = 350.0
CORE_Z = 150.0


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary

    # ---- 1. M_ai_core ------------------------------------------------------
    full = "%s/M_ai_core" % PKG_DIR
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mat = asset_tools.create_asset("M_ai_core", PKG_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    tint = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -700, 0)
    tint.set_editor_property("constant", unreal.LinearColor(1.0, 0.86, 0.55, 1.0))  # white-gold

    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 260)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 25.0)

    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -420, 60)
    mel.connect_material_expressions(tint, "", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_asset(full)
    unreal.log("%s M_ai_core created (white-gold, Glow=25)" % TAG)

    # ---- 2. S_ai_intro -----------------------------------------------------
    snd = "%s/S_ai_intro" % PKG_DIR
    if os.path.isfile(WAV):
        if eal.does_asset_exist(snd):
            eal.delete_asset(snd)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", WAV)
        task.set_editor_property("destination_path", PKG_DIR)
        task.set_editor_property("destination_name", "S_ai_intro")
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        asset_tools.import_asset_tasks([task])
        if eal.does_asset_exist(snd):
            unreal.log("%s S_ai_intro imported from %s" % (TAG, WAV))
        else:
            unreal.log_error("%s WAV import FAILED — check Output Log for the importer's complaint." % TAG)
    else:
        unreal.log_warning("%s %s not found — apparition will run SILENT until the WAV exists; re-run me after." % (TAG, WAV))

    # ---- 3. spawn the apparition (AP5: class must exist -> Build.bat first) -
    cls = unreal.load_class(None, "/Script/SibeliusGame.AIApparition")
    if not cls:
        unreal.log_error("%s AIApparition class not found — close editor, run Build.bat, reopen, re-run me." % TAG)
        return

    # idempotent: clear previous spawns
    for a in list(eas.get_all_level_actors()):
        try:
            if a.get_class().get_name() == "AIApparition":
                eas.destroy_actor(a)
        except Exception:
            pass

    start = None
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.PlayerStart):
            start = a
            break
    if not start:
        unreal.log_error("%s no PlayerStart in this level — is L_Office_v02 open?" % TAG)
        return

    fwd = start.get_actor_forward_vector()
    loc = start.get_actor_location() + fwd * FORWARD_CM
    loc.z = start.get_actor_location().z + CORE_Z

    app = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(roll=0, pitch=0, yaw=0))
    app.set_actor_label("AIApparition")
    # PK16: ASSIGN the voice so the map references it — the cooker only ships
    # referenced assets; a runtime LoadObject ships nothing and plays silence.
    if eal.does_asset_exist(snd):
        app.set_editor_property("voice_line", unreal.load_asset(snd))
        unreal.log("%s voice line assigned (cook-safe)." % TAG)
    unreal.log("%s spawned at (%.0f, %.0f, %.0f) — 350cm ahead of PlayerStart. Ctrl+S, then PIE." % (TAG, loc.x, loc.y, loc.z))


main()
