# make_basketball_assets.py - the three-point contest's assets: the living room's BASKETBALL door
# sign, /Game/Basketball/T_Sign_Basketball, and the balls' leather, /Game/Basketball/T_Basketball.
#
# *** RUN FROM THE OPEN EDITOR, any level, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_basketball_assets.py"
#
# Draw the sign first:  python Tools/Scripts/make_basketball_sign.py
#                       (SourceArt/Basketball/T_Sign_Basketball.png)
# Safe to re-run: re-imports the picture over the texture. Drawn by our own code, so it is
# tracked in git and safe to render, like the Go sign (make_go_assets.py).

import os
import unreal

TAG = "###HOOPASSETS###"
FOLDER = "/Game/Basketball"
SIGN_NAME = "T_Sign_Basketball"
ART = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Basketball")

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def say(text):
    print("%s %s" % (TAG, text))


def import_texture(png, name, ui=False):
    if not os.path.exists(png):
        raise RuntimeError("no %s - run its make_basketball_*.py first" % png)
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = FOLDER
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    tex = eal.load_asset("%s/%s" % (FOLDER, name))
    if not tex:
        raise RuntimeError("%s did not import" % name)
    tex.set_editor_property("srgb", True)
    if ui:
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)   # as T_Sign_Go
    eal.save_asset(tex.get_path_name())
    say("texture %s %dx%d" % (tex.get_path_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
    return tex


try:
    import_texture(os.path.join(ART, "T_Sign_Basketball.png"), SIGN_NAME, ui=True)
    # The balls' leather (make_basketball_art.py), worn through M_GoSurface's Pattern by
    # ABasketballCourt, which hard-references it so it cooks.
    import_texture(os.path.join(ART, "T_Basketball.png"), "T_Basketball")
    say("DONE")
except Exception as error:
    say("FAILED: %s" % error)
