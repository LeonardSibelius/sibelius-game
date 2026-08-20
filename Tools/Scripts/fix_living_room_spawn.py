# fix_living_room_spawn.py — find the living-room floor and seat PlayerStart on it.
# Player was falling through the world: Z=130 put the capsule bottom below the floor.
import unreal

XY = unreal.Vector(-2050.0, 9450.0, 0.0)
YAW = 90.0  # Python Rotator is (roll, pitch, yaw) in this project

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# Start BELOW the Z=320 upper floor / ceiling so we hit the living-room carpet (~Z=62),
# not the attic slab. A spawn at Z=130 put the capsule bottom under that carpet.
start = unreal.Vector(XY.x, XY.y, 200.0)
end = unreal.Vector(XY.x, XY.y, -50.0)
hit = unreal.SystemLibrary.line_trace_single(
    world, start, end,
    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
    True, [],
    unreal.DrawDebugTrace.NONE,
    True, unreal.LinearColor.RED, unreal.LinearColor.GREEN, 1.0,
)

floor_z = None
hit_actor = None
t = hit.to_tuple()
if t and bool(t[0]):
    floor_z = t[4].z
    try:
        hit_actor = t[9].get_actor_label() if t[9] else None
    except Exception:
        hit_actor = str(t[9])

# Pawn origin is the capsule CENTER. Half-height ~88–96. Old office spawn sat
# ~94 above its floor (413.7 on a 320 slab).
seat_z = (floor_z + 96.0) if floor_z is not None else 158.0

ps = None
for a in eas.get_all_level_actors():
    try:
        if a.get_actor_label() == "PlayerStart":
            ps = a
            break
    except Exception:
        pass

if ps is None:
    payload = {"error": "PlayerStart not found"}
else:
    loc = unreal.Vector(XY.x, XY.y, seat_z)
    rot = unreal.Rotator(0.0, 0.0, YAW)
    ps.set_actor_location(loc, False, False)
    ps.set_actor_rotation(rot, False)
    wl = ps.get_actor_location()
    wr = ps.get_actor_rotation()
    payload = {
        "ok": True,
        "floor_z": round(floor_z, 1) if floor_z is not None else None,
        "hit_actor": hit_actor,
        "location": {"x": round(wl.x, 1), "y": round(wl.y, 1), "z": round(wl.z, 1)},
        "rotation": {"pitch": round(wr.pitch, 1), "yaw": round(wr.yaw, 1), "roll": round(wr.roll, 1)},
    }
