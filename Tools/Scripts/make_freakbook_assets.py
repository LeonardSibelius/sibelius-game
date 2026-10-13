# make_freakbook_assets.py — Freakbook's pictures, imported as /Game/Freakbook/T_FB_<name>.
#
# *** RUN FROM THE OPEN EDITOR, any level, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_freakbook_assets.py"
#
# Draw them first:  python Tools/Scripts/make_freakbook_pictures.py   (SourceArt/Freakbook/*.png)
# Safe to re-run: each picture is re-imported over its texture.
#
# UI textures: the UI group (no mips, never streamed out from under the laptop screen) and BC7,
# which keeps the cartoons' thin outlines clean at a quarter of the uncompressed size. The folder
# is cooked by name (DefaultGame.ini, DirectoriesToAlwaysCook): UFreakbookWidget loads these by
# path when the laptop opens, and nothing in a level references them.

import os
import unreal

TAG = "###FREAKBOOK###"
FOLDER = "/Game/Freakbook"
ART = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Freakbook")

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def say(text):
    print("%s %s" % (TAG, text))


def main():
    pngs = sorted(f for f in os.listdir(ART) if f.lower().endswith(".png")) if os.path.isdir(ART) else []
    if not pngs:
        say("no pictures in %s - run make_freakbook_pictures.py first. Nothing imported." % ART)
        return
    tasks = []
    for png in pngs:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(ART, png)
        task.destination_path = FOLDER
        task.destination_name = "T_FB_" + os.path.splitext(png)[0]
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    done = 0
    for task in tasks:
        path = "%s/%s" % (FOLDER, task.destination_name)
        tex = eal.load_asset(path)
        if not tex:
            say("MISSING after import: %s" % path)
            continue
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        eal.save_asset(tex.get_path_name())
        done += 1
    say("imported %d of %d pictures into %s." % (done, len(tasks), FOLDER))


main()
