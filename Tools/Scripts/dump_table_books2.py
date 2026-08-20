# Wider hunt: any SM_Book / BookPickup near the living-room table.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
origin = unreal.Vector(-2137.6, 9445.2, 60.7)
hits = []
for a in eas.get_all_level_actors():
    try:
        lbl = a.get_actor_label()
        cls = a.get_class().get_name()
    except Exception:
        continue
    loc = a.get_actor_location()
    dxy = ((loc.x - origin.x) ** 2 + (loc.y - origin.y) ** 2) ** 0.5
    if dxy > 150.0:
        continue
    mesh = None
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m = c.get_editor_property("static_mesh")
        if m:
            mesh = m.get_name()
            break
    low = (lbl + " " + (mesh or "")).lower()
    if "book" not in low and cls != "BookPickup":
        continue
    hits.append({
        "label": lbl, "class": cls, "mesh": mesh,
        "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1),
        "dxy": round(dxy, 1), "dz": round(loc.z - origin.z, 1),
    })
hits.sort(key=lambda d: d["dxy"])
payload = {"world": unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name(), "hits": hits}
