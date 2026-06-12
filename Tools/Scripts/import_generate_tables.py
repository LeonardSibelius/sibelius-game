# import_generate_tables.py — SIB-43/PK21: create the THREE DataTable assets
# the Generate system requires in PACKAGED builds ("CSV fallback is
# editor-only" — the runtime cannot parse CSVs at all; only the editor can).
#
# Imports each Content/Data CSV with its EXACT row struct (no dialog
# roulette), deletes any prior/broken asset first, verifies row counts.
# Expected: GenerateCatalog 6 rows, MrsHallLines 9, MrsHallBlocklist 8.
#
# RUN NATIVELY (editor Cmd box, dropdown = Cmd), ANY map open:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_generate_tables.py"
# Then Ctrl+S is NOT needed (assets save themselves) — just repackage.

import unreal

TAG = "###GENTABLES###"
CSV_DIR = "C:/Users/wpark/projects/sibelius-game/Content/Data"
DEST = "/Game/Data"

TABLES = [
    ("GenerateCatalog.csv",  "GenerateCatalog",  "/Script/SibeliusGame.GenerateCatalogEntry",  6),
    ("MrsHallLines.csv",     "MrsHallLines",     "/Script/SibeliusGame.MrsHallLineRow",        9),
    ("MrsHallBlocklist.csv", "MrsHallBlocklist", "/Script/SibeliusGame.GenerateBlocklistRow",  8),
]


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary

    ok = 0
    for csv_name, asset_name, struct_path, expected in TABLES:
        struct = unreal.load_object(None, struct_path)
        if not struct:
            unreal.log_error("%s row struct not found: %s — Build.bat first?" % (TAG, struct_path))
            continue

        full = "%s/%s" % (DEST, asset_name)
        if eal.does_asset_exist(full):
            eal.delete_asset(full)

        factory = unreal.CSVImportFactory()
        settings = factory.get_editor_property("automated_import_settings")
        settings.set_editor_property("import_row_struct", struct)
        factory.set_editor_property("automated_import_settings", settings)

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", "%s/%s" % (CSV_DIR, csv_name))
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("factory", factory)
        asset_tools.import_asset_tasks([task])

        table = unreal.load_asset(full)
        if not table:
            unreal.log_error("%s IMPORT FAILED: %s (check Output Log)" % (TAG, asset_name))
            continue
        rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
        status = "OK" if len(rows) == expected else "ROW COUNT UNEXPECTED"
        unreal.log("%s %s: %d rows (expected %d) — %s" % (TAG, asset_name, len(rows), expected, status))
        if len(rows) == expected:
            ok += 1

    unreal.log("%s done: %d/3 tables. NEXT: repackage (no rebuild needed)." % (TAG, ok))


main()
