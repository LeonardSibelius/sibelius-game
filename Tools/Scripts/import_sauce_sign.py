# import_sauce_sign.py — SIB-44 step 1: the inscription on the hidden door.
#
# Imports Tools/Art/T_sign_sauce.png as /Game/Signs/T_sign_sauce and ASSIGNS
# it to every AHiddenDoor in the open level (assignment on the placed actor =
# the map references the texture = it gets cooked; the PK16 lesson).
#
# ORDER: Build.bat FIRST (the sign properties are new C++), then open
# L_Office_v02, then run me, then nudge the sign with the Details knobs
# (Sign Relative Location / Rotation / Width) until it sits on the door face.
#
# RUN NATIVELY (editor Cmd box, dropdown = Cmd):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_sauce_sign.py"

import os
import unreal

TAG = "###SAUCESIGN###"
PNG = "C:/Users/wpark/projects/sibelius-game/Tools/Art/T_sign_sauce.png"
DEST = "/Game/Signs"
ASSET = "%s/T_sign_sauce" % DEST


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not os.path.isfile(PNG):
        unreal.log_error("%s %s not found." % (TAG, PNG))
        return

    if eal.does_asset_exist(ASSET):
        eal.delete_asset(ASSET)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", PNG)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", "T_sign_sauce")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])

    tex = unreal.load_asset(ASSET)
    if not tex:
        unreal.log_error("%s texture import FAILED." % TAG)
        return
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    eal.save_asset(ASSET)
    unreal.log("%s T_sign_sauce imported (UI group)." % TAG)

    doors = 0
    for a in eas.get_all_level_actors():
        if a.get_class().get_name() == "HiddenDoor":
            a.set_editor_property("sign_texture", tex)
            doors += 1
            unreal.log("%s sign assigned to %s — nudge with the Details 'Sign' knobs." % (TAG, a.get_actor_label()))
    if doors == 0:
        unreal.log_error("%s no HiddenDoor in this level — open L_Office_v02 (and Build.bat first for the new properties)." % TAG)
    else:
        unreal.log("%s done: %d door(s) signed. Ctrl+S." % (TAG, doors))


main()
