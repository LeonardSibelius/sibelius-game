# dump_poker_machine.py
a = None
for act in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Actor):
    try:
        if "PokerMachine" in act.get_actor_label() or act.get_class().get_name() == "PokerMachine":
            a = act
            break
    except Exception:
        pass
if a is None:
    payload = {"error": "no PokerMachine"}
else:
    l = a.get_actor_location()
    r = a.get_actor_rotation()
    s = a.get_actor_scale3d()
    meshes = []
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        sm = c.get_editor_property("static_mesh")
        meshes.append({
            "name": c.get_name(),
            "mesh": sm.get_path_name() if sm else None,
            "rel_scale": _ser(c.get_editor_property("relative_scale3d")),
            "rel_loc": _ser(c.get_editor_property("relative_location")),
            "rel_rot": _ser(c.get_editor_property("relative_rotation")),
        })
    payload = {
        "label": a.get_actor_label(),
        "class": a.get_class().get_name(),
        "location": {"x": round(l.x, 1), "y": round(l.y, 1), "z": round(l.z, 1)},
        "rotation": {"pitch": round(r.pitch, 1), "yaw": round(r.yaw, 1), "roll": round(r.roll, 1)},
        "scale": {"x": round(s.x, 3), "y": round(s.y, 3), "z": round(s.z, 3)},
        "meshes": meshes,
    }
