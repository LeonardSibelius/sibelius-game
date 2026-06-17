# build_kitchen_loop.py — SIB-47 Sauce Door: wire the kitchen loop into L_Office_v02.
#
# Places (idempotent — deletes any prior copies first, so re-running is safe):
#   - ASauceDoor "SauceDoor_Kitchen" at the kitchen centre (open floor, so it's
#     findable). It's an AHiddenDoor subclass: HOLD Code Vision (V) to shimmer it into
#     view, then E steps through to L_Elsewhere. No bStartArmed — reveal == armed, like
#     every other hidden door.
#   - ACabinetOfCuriosities near the kitchen.
# The return door comes home to L_Office_v02 (its default). Walt nudges exact positions
# by eye after (e.g. slide the door flush to a wall).
#
# Headless (the live-editor bridge kept dropping): loads L_Office_v02, anchors to the
# kitchen-set centroid, spawns + labels the actors, saves the level. Run:
#     UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript -script="...build_kitchen_loop.py"

import unreal

MAP = "/Game/L_Office_v02"
TAG = "###KITCHEN-LOOP###"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

door_cls = unreal.load_class(None, "/Script/SibeliusGame.SauceDoor")
cab_cls = unreal.load_class(None, "/Script/SibeliusGame.CabinetOfCuriosities")
if door_cls is None or cab_cls is None:
    unreal.log_error("%s C++ classes not found — Build.bat the editor target first." % TAG)
else:
    les.load_level(MAP)
    world = ues.get_editor_world()

    # Idempotent: remove any prior copies (the door's class changed to an AHiddenDoor
    # subclass, so a stale instance would be wrong anyway).
    for a in list(eas.get_all_level_actors()):
        if a.get_actor_label() in ("SauceDoor_Kitchen", "CabinetOfCuriosities"):
            eas.destroy_actor(a)

    # Anchor to the kitchen: centroid of the kitchen-set actors, floor = their min Z.
    kx = ky = 0.0
    minz = 1e9
    n = 0
    for a in eas.get_all_level_actors():
        lbl = a.get_actor_label()
        if "Kitchen" in lbl:
            loc = a.get_actor_location()
            kx += loc.x; ky += loc.y; minz = min(minz, loc.z); n += 1
    if n == 0:
        unreal.log_error("%s no kitchen actors found in %s" % (TAG, MAP))
    else:
        cx = kx / n; cy = ky / n; fz = minz
        unreal.log("%s kitchen centroid=(%.0f, %.0f) floorZ=%.0f from %d actors" % (TAG, cx, cy, fz, n))

        # Door: at the kitchen CENTRE on open floor, so it's findable (the old +300 X
        # spot put it into geometry — Walt saw nothing). Hold V to reveal it; nudge it
        # flush to a wall after. DoorMesh slab is ~220cm tall (centred pivot) -> lift
        # ~110cm so it rests on the floor.
        door_loc = unreal.Vector(cx, cy, fz + 110.0)
        door = eas.spawn_actor_from_class(door_cls, door_loc, unreal.Rotator(0.0, 0.0, 0.0))
        door.set_actor_label("SauceDoor_Kitchen")

        # Cabinet: a bit away, accessible. ISM slots grow along +Y from its origin.
        cab_loc = unreal.Vector(cx - 300.0, cy + 300.0, fz)
        cab = eas.spawn_actor_from_class(cab_cls, cab_loc, unreal.Rotator(0.0, 0.0, 0.0))
        cab.set_actor_label("CabinetOfCuriosities")

        les.save_current_level()
        unreal.log("%s placed SauceDoor_Kitchen (hidden-door, reveal on V) at (%.0f,%.0f,%.0f); CabinetOfCuriosities at (%.0f,%.0f,%.0f). Saved." % (
            TAG, door_loc.x, door_loc.y, door_loc.z, cab_loc.x, cab_loc.y, cab_loc.z))
