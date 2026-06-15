# build_carousel_datatables.py — SIB-46 content-as-data (June 2026).
#
# Authors /Game/Data/DT_CarouselSymbols (FSymbolDef rows) and DT_CarouselCharms (FCharmDef rows),
# seeded with the EXACT tuned baseline so the sim numbers do not change. The C++ sim loads these at
# init (falling back to baked literals if absent), so symbols/charms can be tuned live via the
# SIB-45 bridge with no recompile. /Game/Data is already in DirectoriesToAlwaysCook (PK21).
#
# RUN in the editor (Cmd box):  py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_carousel_datatables.py"

import unreal
import json

DIR = "/Game/Data"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
dtfl = unreal.DataTableFunctionLibrary


def make_table(name, struct, rows):
    full = "%s/%s" % (DIR, name)
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", struct)
    dt = tools.create_asset(name, DIR, unreal.DataTable, factory)
    dtfl.fill_data_table_from_json_string(dt, json.dumps(rows))
    eal.save_asset(full)
    return dt


# --- Symbols (the tuned 12-symbol baseline; values must match CarouselSim.cpp fallback exactly) ---
#       Id           Type          P3   P4    P5    Wt  mult  wild   scatter
SYMBOLS = [
    ("Cog",       "Normal",     27,   72,  180, 16, 1, False, False),
    ("Key",       "Normal",     27,   72,  180, 14, 1, False, False),
    ("Coin",      "Normal",     27,   72,  180, 14, 1, False, False),
    ("Gear",      "Normal",     45,  135,  360, 10, 1, False, False),
    ("Book",      "Normal",     45,  135,  360, 10, 1, False, False),
    ("Eye",       "Normal",     54,  162,  405,  6, 1, False, False),
    ("Flame",     "Normal",     54,  162,  405,  6, 1, False, False),
    ("Star",      "Multiplier", 45,  108,  270,  3, 2, False, False),
    ("Dragon",    "Normal",    180,  540, 1800,  2, 1, False, False),
    ("SauceDrop", "Normal",    225,  720, 2700,  1, 1, False, False),
    ("Fate",      "Wild",      135,  450, 1350, 12, 1, True,  False),
    ("Carousel",  "Scatter",    45,  135,  360,  2, 1, False, True),
]
sym_rows = []
for (sid, stype, p3, p4, p5, w, mult, wild, scat) in SYMBOLS:
    sym_rows.append({
        "Name": sid, "Id": sid, "Type": stype,
        "LinePayouts": {"3": p3, "4": p4, "5": p5},
        "MultiplierValue": mult, "bSubstitutesAsWild": wild, "bPaysAnywhere": scat,
        "BaseWeight": w,
    })
make_table("DT_CarouselSymbols", unreal.SymbolDef.static_struct(), sym_rows)

# --- Charms (10; flat ShopCost 5 = current CarouselRun.CharmCost so RunDemo is unchanged) ---
CHARMS = ["Wildfire", "Cascade", "Compounder", "MundaneRiches", "HighRoller",
          "ScatterShrine", "NearMissMercy", "Hoarder", "TwinReels", "StickyFate"]
charm_rows = [{"Name": c, "Id": c, "ShopCost": 5, "Rarity": 0, "Triggers": 0} for c in CHARMS]
make_table("DT_CarouselCharms", unreal.CharmDef.static_struct(), charm_rows)

# --- Verify ---
sdt = unreal.load_asset("%s/DT_CarouselSymbols" % DIR)
cdt = unreal.load_asset("%s/DT_CarouselCharms" % DIR)
print("###DT### symbols:", len(dtfl.get_data_table_row_names(sdt)),
      "charms:", len(dtfl.get_data_table_row_names(cdt)))
