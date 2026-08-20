# import_mrshall_story.py — make Mrs. Hall's story lines survive packaging.
#
# THE BUG THIS FIXES IS SILENT AND TOTAL.
#
# MrsHallLines.cpp loads its tables asset-first, CSV-second -- and the CSV branch is
# #if WITH_EDITOR. A packaged build has no CSV fallback at all. On top of that, no .csv
# reaches the package: there is not one in the whole v0.9.1 build.
#
# The other three tables survive because each has a companion DataTable ASSET:
#
#     GenerateCatalog.csv   + GenerateCatalog.uasset
#     MrsHallLines.csv      + MrsHallLines.uasset
#     MrsHallBlocklist.csv  + MrsHallBlocklist.uasset
#     MrsHallStory.csv      + NOTHING
#
# So in a shipped v0.9.4, LoadMrsHallStoryLines would fail and Mrs. Hall would say
# NOTHING -- no opening ticket, no reaction to Vision, no last word at the finale. She
# works perfectly in PIE, which is exactly how this game has hidden its worst bugs
# before: the interaction prompts, the living-room book's glow, the panel font.
#
# This imports Content/Data/MrsHallStory.csv as /Game/Data/MrsHallStory, rows typed as
# FMrsHallLineRow -- the same shape the other three have.
#
# RE-RUN THIS WHENEVER THE CSV CHANGES. Editing the CSV alone updates the editor (via the
# fallback) and NOT the packaged game. GenerateSmokeTest now asserts the asset exists and
# holds every Reason the code asks for, so a forgotten re-import fails the gate instead of
# shipping a mute antagonist.
#
# RUN (editor Python console):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_mrshall_story.py"

import json
import os
import unreal

CSV = unreal.Paths.project_content_dir() + "Data/MrsHallStory.csv"
DEST_PATH = "/Game/Data"
DEST_NAME = "MrsHallStory"
ROW_STRUCT = "/Script/SibeliusGame.MrsHallLineRow"

notes = []
ok = False
rows = None

csv_full = os.path.abspath(CSV)
if not os.path.exists(csv_full):
    notes.append("CSV not found at %s" % csv_full)
else:
    struct = None
    try:
        struct = unreal.load_object(None, ROW_STRUCT)
    except Exception as e:
        notes.append("could not load row struct %s: %s" % (ROW_STRUCT, e))

    if struct is None:
        notes.append("row struct is None -- is the editor running a build that has "
                     "FMrsHallLineRow? rebuild if this is an old DLL")
    else:
        try:
            factory = unreal.CSVImportFactory()
            factory.automated_import_settings.import_row_struct = struct

            task = unreal.AssetImportTask()
            task.filename = csv_full
            task.destination_path = DEST_PATH
            task.destination_name = DEST_NAME
            task.automated = True
            task.replace_existing = True
            task.save = True
            task.factory = factory

            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

            asset = unreal.load_object(None, "%s/%s.%s" % (DEST_PATH, DEST_NAME, DEST_NAME))
            if asset:
                try:
                    rows = len(asset.get_row_names())
                except Exception:
                    rows = "?"
                ok = True
                notes.append("imported %s rows" % rows)
                # Save it explicitly: task.save is not always enough for a fresh asset.
                try:
                    unreal.EditorAssetLibrary.save_asset("%s/%s" % (DEST_PATH, DEST_NAME))
                    notes.append("asset saved")
                except Exception as e:
                    notes.append("WARNING: import succeeded but save_asset failed: %s -- "
                                 "save it by hand in the Content Browser" % e)
            else:
                notes.append("import ran but the asset did not resolve afterwards")
        except Exception as e:
            notes.append("import failed: %s" % e)

payload = {
    "ok": ok,
    "csv": csv_full,
    "asset": "%s/%s" % (DEST_PATH, DEST_NAME),
    "rows": rows,
    "notes": notes,
    "why": "the CSV fallback in MrsHallLines.cpp is #if WITH_EDITOR, and no .csv reaches "
           "a packaged build -- without this asset Mrs. Hall is silent for every player",
    "REMEMBER": "re-run this whenever MrsHallStory.csv changes",
}

text = json.dumps(payload, indent=2, default=str)
out = unreal.Paths.project_saved_dir() + "import_mrshall_story.json"
try:
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[import_mrshall_story] wrote " + out)
except Exception as e:
    unreal.log_error("[import_mrshall_story] could not write %s: %s" % (out, e))

print(text)
