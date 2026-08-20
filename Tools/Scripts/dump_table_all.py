# dump_table_all.py — every actor near SM_Table_A2, including unnamed books.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
table = None
for a in eas.get_all_level_actors():
    try:
        if a.get_actor_label() == "SM_Table_A2":
            table = a
            break
    except Exception:
        pass
if table is None:
    payload = {"ok": False, "error": "no table"}
else:
    t = table.get_actor_location()
    near = []
    for a in eas.get_all_level_actors():
        try:
            lbl = a.get_actor_label()
            cls = a.get_class().get_name()
        except Exception:
            continue
        loc = a.get_actor_location()
        dxy = ((loc.x - t.x) ** 2 + (loc.y - t.y) ** 2) ** 0.5
        if dxy > 80.0:
            continue
        mesh = None
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.get_editor_property("static_mesh")
            if m:
                mesh = m.get_name()
                break
        near.append({
            "label": lbl,
            "class": cls,
            "mesh": mesh,
            "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1),
            "dxy": round(dxy, 1),
            "dz": round(loc.z - t.z, 1),
        })
    near.sort(key=lambda d: d["dxy"])
    payload = {"ok": True, "count": len(near), "near": near}
