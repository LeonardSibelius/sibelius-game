# build_elsewhere_datatables.py — SIB-47 Sauce Door: import the content DataTables.
#
# Creates /Game/Data/DT_ElsewherePlaces (row struct FPlaceTypeDef) and
# /Game/Data/DT_ElsewhereCurios (row struct FCurioDef) from the committed CSVs in
# Data/. The runtime (UElsewhereSubsystem) loads these if present, else the identical
# code defaults. The Server Cathedral row's kit palette points at the real Crebotoly
# ModularSciFiEnv_K meshes (referenced by path; the kit bytes are NOT committed).
#
# ORDER: Build.bat the editor target FIRST (the row structs must exist), then run:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_elsewhere_datatables.py"
# (or via the live editor: ue_bridge exec-python exec(open(...).read())).

import unreal

ROOT = "C:/Users/wpark/projects/sibelius-game/Data"
DEST = "/Game/Data"
TAG = "###ELSEWHERE-DT###"

TABLES = [
    ("ElsewherePlaces.csv", "DT_ElsewherePlaces", "/Script/SibeliusGame.PlaceTypeDef"),
    ("ElsewhereCurios.csv", "DT_ElsewhereCurios", "/Script/SibeliusGame.CurioDef"),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()

for csv_name, asset_name, struct_path in TABLES:
    row_struct = unreal.load_object(None, struct_path)
    if row_struct is None:
        unreal.log_error("%s row struct %s not found — Build.bat the editor target first." % (TAG, struct_path))
        continue

    asset_full = "%s/%s" % (DEST, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_full):
        unreal.EditorAssetLibrary.delete_asset(asset_full)

    factory = unreal.DataTableFactory()
    factory.struct = row_struct
    dt = tools.create_asset(asset_name, DEST, unreal.DataTable, factory)
    if dt is None:
        unreal.log_error("%s create_asset failed for %s" % (TAG, asset_name))
        continue

    csv_path = "%s/%s" % (ROOT, csv_name)
    problems = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(dt, csv_path)
    unreal.EditorAssetLibrary.save_asset(asset_full)

    rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)
    unreal.log("%s %s: %d rows [%s]; fill_ok=%s" % (
        TAG, asset_name, len(rows), ", ".join([str(r) for r in rows]), problems))
