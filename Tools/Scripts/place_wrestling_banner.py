# place_wrestling_banner.py - import the wrestling banner and hang it in the living room.
#
# Walt, 2026-11-13: "I want advertising for Wrestling in AI City on a banner in the living room."
# The art is SourceArt/Wrestling/T_Sign_Wrestling.png (make_wrestling_banner.py). It hangs on two
# cords from the living-room ceiling (about 308 cm, measured) over the aisle between the pool
# table and the pinball - not above either table, whose players' cameras look down from there -
# and shows the same picture both ways along the aisle (AHangingBanner).
#
# Idempotent: re-importing replaces the texture, and an existing WrestlingBanner is reused.
# Run with the editor CLOSED:
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
# (or with L_Office_v02 open, via the bridge - then save the level yourself).

import os
import unreal

TAG = "###WRESTLEBANNER###"
HERE = os.path.dirname(os.path.abspath(__file__))
PNG = os.path.normpath(os.path.join(HERE, "..", "..", "SourceArt", "Wrestling", "T_Sign_Wrestling.png"))
DEST = "/Game/Signs"
ASSET = DEST + "/T_Sign_Wrestling"
LEVEL = "/Game/L_Office_v02"
LABEL = "WrestlingBanner"

# Where the cords meet the ceiling: the aisle between the pool table (to y 9524) and the pinball
# (from y 9681), clear of both. Facing along the aisle (+X and, on its back, -X).
AT = unreal.Vector(-2150.0, 9602.0, 300.0)
YAW = 0.0
WIDTH = 150.0   # the aisle is about 157 cm wide
DROP = 15.0


def main():
    if not os.path.isfile(PNG):
        unreal.log_error("%s %s not found - run make_wrestling_banner.py first." % (TAG, PNG))
        return
    eal = unreal.EditorAssetLibrary
    # Import only when the texture is missing (or REIMPORT=1 with the editor open): headless, the
    # importer's last step - syncing the Content Browser - asserts for want of Slate, after the
    # texture is already imported and saved (2026-11-13).
    if eal.does_asset_exist(ASSET) and os.environ.get("REIMPORT") != "1":
        unreal.log("%s %s exists - not re-importing." % (TAG, ASSET))
    else:
        import_texture()
    tex = unreal.load_asset(ASSET)
    if not tex:
        unreal.log_error("%s texture import FAILED." % TAG)
        return
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    eal.save_asset(ASSET)
    hang(tex)


def import_texture():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", PNG)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", "T_Sign_Wrestling")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def hang(tex):
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    banner = None
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == LABEL:
            banner = a
            break
    if not banner:
        cls = unreal.load_class(None, "/Script/SibeliusGame.HangingBanner")
        if not cls:
            unreal.log_error("%s no HangingBanner class - build the editor first." % TAG)
            return
        banner = eas.spawn_actor_from_class(cls, AT, unreal.Rotator(roll=0.0, pitch=0.0, yaw=YAW))
        banner.set_actor_label(LABEL)
    banner.set_actor_location_and_rotation(AT, unreal.Rotator(roll=0.0, pitch=0.0, yaw=YAW), False, False)
    banner.set_editor_property("picture", tex)
    banner.set_editor_property("width", WIDTH)
    banner.set_editor_property("drop", DROP)
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.log("%s banner hung at %s, level saved: %s" % (TAG, AT, saved))


main()
