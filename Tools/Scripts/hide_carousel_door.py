# hide_carousel_door.py — FUN: the casino becomes the kitchen's second secret.
#
# Replaces the placed ACathedralDoor "CarouselDoor_Kitchen" (placeholder cube)
# with an AHiddenDoor: invisible wall until Code Vision reveals it, then E
# travels to L_Carousel (SIB-44's travel path — the Many Worlds door's own
# pattern). Copies the look (DoorMesh mesh + scale) from SauceDoor_Kitchen so
# the kitchen's two secrets read as siblings. Idempotent. Run via bridge
# (editor open) or -run=pythonscript (editor closed).

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/L_Office_v02")  # self-contained: the door lives here
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()
results = []


def find(label):
    for a in actors:
        if a.get_actor_label() == label:
            return a
    return None


existing = find("CarouselDoor_Kitchen")
if existing and existing.get_class().get_name() == "HiddenDoor":
    results.append("already a HiddenDoor — nothing to do")
else:
    # Remember where the old placeholder stood, then remove it.
    loc = unreal.Vector(-1850.0, 9240.0, 170.0)
    rot = unreal.Rotator(0.0, 0.0, 0.0)
    if existing:
        loc = existing.get_actor_location()
        rot = existing.get_actor_rotation()
        eas.destroy_actor(existing)
        results.append("old cube door removed")

    # Sibling look: borrow the Many Worlds door's panel mesh + scale.
    door_mesh = None
    mesh_scale = None
    sauce_door = find("SauceDoor_Kitchen")
    if sauce_door:
        for c in sauce_door.get_components_by_class(unreal.StaticMeshComponent):
            if c.get_name() == "DoorMesh" and c.get_editor_property("StaticMesh"):
                door_mesh = c.get_editor_property("StaticMesh")
                mesh_scale = c.get_editor_property("RelativeScale3D")
                break

    cls = unreal.load_class(None, "/Script/SibeliusGame.HiddenDoor")
    door = eas.spawn_actor_from_class(cls, loc, rot)
    door.set_actor_label("CarouselDoor_Kitchen")
    door.set_editor_property("TravelTargetLevel", "L_Carousel")
    door.set_editor_property("TravelPromptText",
                             unreal.Text("Ride the Carousel of Fates [E]"))
    if door_mesh:
        for c in door.get_components_by_class(unreal.StaticMeshComponent):
            if c.get_name() == "DoorMesh":
                c.set_editor_property("StaticMesh", door_mesh)
                if mesh_scale:
                    c.set_editor_property("RelativeScale3D", mesh_scale)
        results.append("panel mesh copied from SauceDoor_Kitchen")
    else:
        results.append("no SauceDoor mesh found — assign DoorMesh by hand")

    results.append("HiddenDoor placed at %s -> L_Carousel" % loc)

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print("RESULT: " + " | ".join(results))
