# forest_finish_a.py — find the landscape Walt created, report its size, assign M_ForestGround.
import unreal, json
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

land = None
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.Landscape):
        land = a
        break

out = {}
if not land:
    out["error"] = "NO LANDSCAPE FOUND"
    unreal.log("###FINISH-A### NO LANDSCAPE FOUND")
else:
    origin, extent = land.get_actor_bounds(False)
    out["label"] = land.get_actor_label()
    out["origin"] = [origin.x, origin.y, origin.z]
    out["extent_uu"] = [extent.x, extent.y, extent.z]
    out["size_m"] = [round(extent.x * 2 / 100.0, 1), round(extent.y * 2 / 100.0, 1)]
    loc = land.get_actor_location()
    out["actor_location"] = [loc.x, loc.y, loc.z]
    mat = unreal.EditorAssetLibrary.load_asset("/Game/Forest/M_ForestGround")
    land.set_editor_property("landscape_material", mat)
    out["material_assigned"] = bool(mat)
    les.save_current_level()
    unreal.log("###FINISH-A### landscape=%s size=%sx%s m, material=%s" % (
        out["label"], out["size_m"][0], out["size_m"][1], bool(mat)))

open(r"C:/Users/wpark/projects/sibelius-game/forest-landscape.json", "w").write(json.dumps(out, indent=1))
