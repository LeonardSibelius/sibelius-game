# import_symbol_sprites.py — import the 9 Celestial Fortune vector sprites
# (June 11, 2026 set — Cowork-drawn SVG -> 512px PNG, fully ours, committable)
# from the celestial-fortune web repo into /Game/SlotFactory/SymbolSprites as
# T_sym_<id>, UI texture group. Idempotent (replace_existing).
#
# RUN NATIVELY (editor Cmd box, never the bridge):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_symbol_sprites.py"
#
# The slot screen (USlotScreenWidget) loads these by exact path; the SlotSmokeTest
# asserts all nine resolve. ESlotSymbol::Earth uses the scatter art (docs note).

import os
import unreal

SRC_DIR = "C:/Users/wpark/projects/celestial-fortune/src/factory"
DEST = "/Game/SlotFactory/SymbolSprites"
IDS = ["star", "moon", "galaxy", "saturn", "mars", "crown", "seven", "wild", "scatter"]
TAG = "###SPRITES###"


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    ok = 0
    for sid in IDS:
        src = "%s/%s.png" % (SRC_DIR, sid)
        if not os.path.exists(src):
            unreal.log_error("%s MISSING source file: %s" % (TAG, src))
            continue
        task = unreal.AssetImportTask()
        task.filename = src
        task.destination_path = DEST
        task.destination_name = "T_sym_%s" % sid
        task.automated = True
        task.save = True
        task.replace_existing = True
        tools.import_asset_tasks([task])
        tex = unreal.load_asset("%s/T_sym_%s" % (DEST, sid))
        if tex:
            try:
                # UI group: no streaming surprises at widget scale.
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
                eal.save_asset("%s/T_sym_%s" % (DEST, sid))
            except Exception as ex:
                unreal.log("%s %s: lod_group not set (%s) — fine, default group works" % (TAG, sid, ex))
            ok += 1
            unreal.log("%s imported T_sym_%s" % (TAG, sid))
        else:
            unreal.log_error("%s import FAILED for %s" % (TAG, sid))
    unreal.log("%s DONE — %d/9 sprites in %s" % (TAG, ok, DEST))


main()
