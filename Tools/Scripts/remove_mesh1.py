# remove_mesh1.py — BookPickup_LivingRoom grew a second StaticMesh (Mesh1).
# Delete it, snap Glow onto the root Mesh, save.
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
    notes = []
    mesh1 = None
    glow = None
    for c in pickup.get_components_by_class(unreal.ActorComponent):
        if c.get_name() == "Mesh1":
            mesh1 = c
        if isinstance(c, unreal.PointLightComponent):
            glow = c
    if mesh1:
        mesh1.destroy_component(mesh1)
        notes.append("destroyed Mesh1")
    else:
        notes.append("Mesh1 already gone")

    loc = pickup.get_actor_location()
    if glow:
        try:
            glow.set_editor_property("absolute_location", False)
            glow.set_editor_property("absolute_rotation", False)
        except Exception:
            pass
        glow.set_world_location(unreal.Vector(loc.x, loc.y, loc.z + 8.0), False, False)
        glow.set_relative_location(unreal.Vector(0.0, 0.0, 8.0), False, False)
        glow.set_relative_rotation(unreal.Rotator(0.0, 0.0, 0.0), False, False)
        notes.append("Glow on root Mesh")

    left = [c.get_name() for c in pickup.get_components_by_class(unreal.StaticMeshComponent)]
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saved = bool(les.save_current_level()) if les else False
    payload = {"ok": True, "saved": saved, "notes": notes, "meshes_left": left}
