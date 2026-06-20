# forest_verify_phase45.py — read-only: confirm the Forest place-type + curios imported, the PCG
# graph has all layers, and the forest return door is placed + armed. (Data check, not look.)
import unreal, json
out = {}
dtp = unreal.load_asset("/Game/Data/DT_ElsewherePlaces")
out["places_rows"] = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(dtp)]
try:
    out["travel_levels"] = list(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dtp, "TravelLevelName"))
    out["curio_pools"] = list(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dtp, "CurioPool"))
except Exception as e:
    out["col_err"] = repr(e)
dtc = unreal.load_asset("/Game/Data/DT_ElsewhereCurios")
out["curios_rows"] = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(dtc)]

g = unreal.load_asset("/Game/PCG/PCG_ForestScatter")
out["pcg_node_count"] = len(g.nodes)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level("/Game/Maps/L_Elsewhere_Forest")
door = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "ReturnDoor_Forest":
        door = a
        break
out["return_door_placed"] = bool(door)
if door:
    out["self_trigger"] = bool(door.get_editor_property("bSelfReturnTrigger"))
    out["home_level"] = str(door.get_editor_property("HomeLevelName"))

open(r"C:/Users/wpark/projects/sibelius-game/forest-verify.json", "w").write(json.dumps(out, indent=1))
unreal.log("###VERIFY### done")
