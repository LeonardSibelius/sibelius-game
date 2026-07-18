# import_cards.py — SIDE_GAMES G5: the genuine deck.
#
# Imports Tools/Art/Cards/T_card_*.png (52 cards + back, generated in the
# house style — see the deck generator notes in the poker commit) into
# /Game/Cards as UI-group textures. The widget runtime-loads them by path;
# cooking is guaranteed by DirectoriesToAlwaysCook=/Game/Cards in
# DefaultGame.ini (the menagerie pattern — NOT the PK16 map-reference route),
# and PokerSmokeTest hard-fails unless all 53 resolve.
#
# Run via the bridge (editor OPEN) or the editor Cmd box (dropdown = Cmd):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_cards.py"

import os
import unreal

TAG = "###CARDS###"
SRC = "C:/Users/wpark/projects/sibelius-game/Tools/Art/Cards"
DEST = "/Game/Cards"


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary

    names = sorted(n[:-4] for n in os.listdir(SRC)
                   if n.startswith("T_card_") and n.endswith(".png"))
    if len(names) != 53:
        unreal.log_error("%s expected 53 pngs, found %d — rerun the generator." % (TAG, len(names)))
        return

    tasks = []
    for name in names:
        if eal.does_asset_exist("%s/%s" % (DEST, name)):
            eal.delete_asset("%s/%s" % (DEST, name))
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", "%s/%s.png" % (SRC, name))
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    asset_tools.import_asset_tasks(tasks)

    ok = 0
    for name in names:
        asset = "%s/%s" % (DEST, name)
        tex = unreal.load_asset(asset)
        if tex:
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            eal.save_asset(asset)
            ok += 1
        else:
            unreal.log_error("%s import FAILED: %s" % (TAG, asset))
    unreal.log("%s %d/53 card textures imported to %s (UI group)." % (TAG, ok, DEST))


main()
