# forest_finish.py — confirm the 508m landscape, assign M_ForestGround, seat the PlayerStart, save.
import unreal, json
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

out = {}
land = None
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.Landscape):
        land = a
        break

if not land:
    out["error"] = "NO LANDSCAPE"
    unreal.log("###FINISH### NO LANDSCAPE")
else:
    origin, extent = land.get_actor_bounds(False)
    out["size_m"] = [round(extent.x * 2 / 100.0, 1), round(extent.y * 2 / 100.0, 1)]
    mat = unreal.EditorAssetLibrary.load_asset("/Game/Forest/M_ForestGround")
    land.set_editor_property("landscape_material", mat)
    out["material_assigned"] = bool(mat)
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "PlayerStart":
            a.set_actor_location(unreal.Vector(0.0, 0.0, 200.0), False, False)
            out["playerstart_z"] = 200
    les.save_current_level()
    unreal.log("###FINISH### size=%s m, material=%s, saved" % (out["size_m"], out.get("material_assigned")))

open(r"C:/Users/wpark/projects/sibelius-game/forest-finish.json", "w").write(json.dumps(out, indent=1))
