# clear_cauldron_dress.py — FUN-8.1: the stove and pots ARE the cauldron.
# Clears the placeholder cylinder meshes off SauceCauldron_Kitchen so only the
# invisible InteractZone remains (Walt then drags/scales it over the stove).
# Idempotent; safe to re-run.

import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

actor = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "SauceCauldron_Kitchen":
        actor = a
        break

if not actor:
    print("RESULT: SauceCauldron_Kitchen not found")
else:
    cleared = []
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() in ("CauldronMesh", "ContentsMesh") and c.get_editor_property("StaticMesh"):
            c.set_editor_property("StaticMesh", None)
            cleared.append(c.get_name())
    print("RESULT: cleared %s on SauceCauldron_Kitchen" % (cleared if cleared else "nothing (already bare)"))
