# fix_book_components.py — BookPickup_LivingRoom looks like two books because
# Glow (lightbulb widget) is offset from Mesh (transform gizmo). Snap the lamp
# onto the mesh. Report every component so we can see a duplicate mesh if any.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
pickup = None
for a in eas.get_all_level_actors():
    try:
        if a.get_actor_label() == "BookPickup_LivingRoom":
            pickup = a
            break
    except Exception:
        pass

if pickup is None:
    payload = {"ok": False, "error": "no BookPickup_LivingRoom"}
else:
    loc = pickup.get_actor_location()
    comps = []
    meshes = []
    glow = None
    for c in pickup.get_components_by_class(unreal.ActorComponent):
        rec = {"name": c.get_name(), "class": c.get_class().get_name()}
        try:
            w = c.get_world_location()
            rec["wx"] = round(w.x, 1)
            rec["wy"] = round(w.y, 1)
            rec["wz"] = round(w.z, 1)
        except Exception:
            pass
        if isinstance(c, unreal.StaticMeshComponent):
            m = c.get_editor_property("static_mesh")
            rec["mesh"] = m.get_name() if m else None
            meshes.append(c)
        if isinstance(c, unreal.PointLightComponent):
            glow = c
        comps.append(rec)

    notes = []
    extra_hidden = []
    root = pickup.root_component
    for c in meshes:
        if c == root:
            continue
        # A second mesh on this actor would draw the "other book".
        c.set_visibility(False, False)
        extra_hidden.append(c.get_name())
        notes.append("hid extra mesh " + c.get_name())

    if glow:
        try:
            glow.set_editor_property("absolute_location", False)
            glow.set_editor_property("absolute_rotation", False)
        except Exception:
            pass
        glow.set_world_location(unreal.Vector(loc.x, loc.y, loc.z + 8.0), False, False)
        glow.set_relative_location(unreal.Vector(0.0, 0.0, 8.0), False, False)
        glow.set_relative_rotation(unreal.Rotator(0.0, 0.0, 0.0), False, False)
        notes.append("Glow world-snapped to mesh")

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saved = bool(les.save_current_level()) if les else False
    payload = {
        "ok": True,
        "saved": saved,
        "actor": {"x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1)},
        "comps": comps,
        "extra_hidden": extra_hidden,
        "notes": notes,
    }
