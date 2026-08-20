# fix_playerstart_capsule.py — "BAD size" = PlayerStart capsule != pawn.
# Pawn is 34 radius / 96 half-height (SibeliusGameCharacter). Seat on the floor.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

ps = None
for a in eas.get_all_level_actors():
    try:
        if a.get_actor_label() == "PlayerStart":
            ps = a
            break
    except Exception:
        pass

if ps is None:
    payload = {"ok": False, "error": "no PlayerStart"}
else:
    loc = ps.get_actor_location()
    # Keep their XY (moved off the book). Reseat Z on the carpet.
    start = unreal.Vector(loc.x, loc.y, 200.0)
    end = unreal.Vector(loc.x, loc.y, -50.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True, [],
        unreal.DrawDebugTrace.NONE,
        True, unreal.LinearColor.RED, unreal.LinearColor.GREEN, 1.0,
    )
    floor_z = None
    t = hit.to_tuple()
    if t and bool(t[0]):
        floor_z = t[4].z
    seat_z = (floor_z + 96.0) if floor_z is not None else 158.0

    notes = []
    for c in ps.get_components_by_class(unreal.CapsuleComponent):
        c.set_capsule_radius(34.0, True)
        c.set_capsule_half_height(96.0, True)
        notes.append("capsule 34 x 96")

    ps.set_actor_location(unreal.Vector(loc.x, loc.y, seat_z), False, False)
    ps.set_actor_rotation(unreal.Rotator(0.0, 0.0, 90.0), False)

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saved = bool(les.save_current_level()) if les else False
    wl = ps.get_actor_location()
    payload = {
        "ok": True,
        "saved": saved,
        "floor_z": round(floor_z, 1) if floor_z is not None else None,
        "location": {"x": round(wl.x, 1), "y": round(wl.y, 1), "z": round(wl.z, 1)},
        "notes": notes,
    }
