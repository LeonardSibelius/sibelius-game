# snap_book_glow.py — Glow was dragged off the mesh (lightbulb vs book gizmo).
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
pickup = None
papers = []
for a in eas.get_all_level_actors():
    try:
        lbl = a.get_actor_label()
    except Exception:
        continue
    if lbl == "BookPickup_LivingRoom":
        pickup = a
    if "newspaper" in lbl.lower() or "news" in lbl.lower():
        loc = a.get_actor_location()
        papers.append({"label": lbl, "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1)})

if pickup is None:
    payload = {"ok": False, "error": "no BookPickup_LivingRoom"}
else:
    glow = None
    for c in pickup.get_components_by_class(unreal.PointLightComponent):
        glow = c
        break
    notes = []
    if glow is None:
        notes.append("no Glow component in editor")
    else:
        try:
            glow.set_editor_property("absolute_location", False)
            glow.set_editor_property("absolute_rotation", False)
        except Exception:
            pass
        mloc = pickup.get_actor_location()
        glow.set_world_location(unreal.Vector(mloc.x, mloc.y, mloc.z + 8.0), False, False)
        glow.set_relative_location(unreal.Vector(0.0, 0.0, 8.0), False, False)
        glow.set_relative_rotation(unreal.Rotator(0.0, 0.0, 0.0), False, False)
        notes.append("snapped Glow onto mesh at +8 Z")
    loc = pickup.get_actor_location()
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saved = bool(les.save_current_level()) if les else False
    payload = {
        "ok": True,
        "saved": saved,
        "pickup": {"x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1)},
        "notes": notes,
        "newspapers": papers,
    }
