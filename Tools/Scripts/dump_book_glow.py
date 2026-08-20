# dump_book_glow.py — compare living-room vs library BookPickup lights.
import unreal

def info(label):
    a = _efind(label) if "_efind" in dir() else None
    if a is None:
        eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        for x in eas.get_all_level_actors():
            try:
                if x.get_actor_label() == label:
                    a = x
                    break
            except Exception:
                pass
    if a is None:
        return {"missing": True, "label": label}
    d = {"label": a.get_actor_label(), "class": a.get_class().get_name(), "comps": []}
    loc = a.get_actor_location()
    d["loc"] = {"x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1)}
    for c in a.get_components_by_class(unreal.ActorComponent):
        d["comps"].append(c.get_name() + "|" + c.get_class().get_name())
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m = c.get_editor_property("static_mesh")
        d["mesh"] = m.get_path_name() if m else None
        mats = []
        try:
            for i in range(c.get_num_materials()):
                mat = c.get_material(i)
                mats.append(mat.get_path_name() if mat else None)
        except Exception as e:
            mats.append(str(e))
        d["mats"] = mats
    lights = []
    for c in a.get_components_by_class(unreal.PointLightComponent):
        lights.append({
            "name": c.get_name(),
            "intensity": c.get_editor_property("intensity"),
            "radius": c.get_editor_property("attenuation_radius"),
            "visible": bool(c.is_visible()),
            "hidden": bool(c.get_editor_property("b_hidden_in_game")) if False else None,
        })
    d["lights"] = lights
    return d

payload = {
    "living": info("BookPickup_LivingRoom"),
    "library": info("BookPickup"),
}
