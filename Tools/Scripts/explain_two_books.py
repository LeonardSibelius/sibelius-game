# explain_two_books.py — how many BookPickup_LivingRoom, mesh bounds, glow world vs mesh world.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
found = []
for a in eas.get_all_level_actors():
    try:
        lbl = a.get_actor_label()
    except Exception:
        continue
    if lbl == "BookPickup_LivingRoom":
        found.append(a)

items = []
for a in found:
    loc = a.get_actor_location()
    rec = {
        "path": a.get_path_name(),
        "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1),
        "comps": [],
    }
    for c in a.get_components_by_class(unreal.ActorComponent):
        cr = {"name": c.get_name(), "class": c.get_class().get_name()}
        try:
            w = c.get_world_location()
            cr["wx"], cr["wy"], cr["wz"] = round(w.x, 1), round(w.y, 1), round(w.z, 1)
        except Exception:
            pass
        if isinstance(c, unreal.StaticMeshComponent):
            m = c.get_editor_property("static_mesh")
            cr["mesh"] = m.get_name() if m else None
            try:
                box = c.get_local_bounds()
                if isinstance(box, (tuple, list)):
                    mn, mx = box[0], box[1]
                    cr["bounds_min"] = [round(mn.x, 1), round(mn.y, 1), round(mn.z, 1)]
                    cr["bounds_max"] = [round(mx.x, 1), round(mx.y, 1), round(mx.z, 1)]
                else:
                    cr["bounds"] = str(box)
            except Exception as e:
                cr["bounds_err"] = str(e)
        rec["comps"].append(cr)
    items.append(rec)

payload = {"count": len(found), "items": items}
