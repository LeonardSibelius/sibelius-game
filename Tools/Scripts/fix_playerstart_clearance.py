# fix_playerstart_clearance.py — "BAD size" is the BadSprite on APlayerStart.
# It appears when Validate() would have to shove the pawn sideways (capsule in the table).
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# Open floor east of the table, looking +Y down the hall. Table is ~(-2138, 9445).
XY = unreal.Vector(-2000.0, 9480.0, 0.0)
YAW = 90.0

ps = None
table = None
for a in eas.get_all_level_actors():
    try:
        lbl = a.get_actor_label()
    except Exception:
        continue
    if lbl == "PlayerStart":
        ps = a
    if lbl == "SM_Table_A2":
        table = a

if ps is None:
    payload = {"ok": False, "error": "no PlayerStart"}
else:
    start = unreal.Vector(XY.x, XY.y, 200.0)
    end = unreal.Vector(XY.x, XY.y, -50.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True, [],
        unreal.DrawDebugTrace.NONE,
        True, unreal.LinearColor.RED, unreal.LinearColor.GREEN, 1.0,
    )
    floor_z = 60.7
    t = hit.to_tuple()
    if t and bool(t[0]):
        floor_z = t[4].z
    seat = unreal.Vector(XY.x, XY.y, floor_z + 96.0)
    ps.set_actor_location(seat, False, False)
    ps.set_actor_rotation(unreal.Rotator(0.0, 0.0, YAW), False)
    for c in ps.get_components_by_class(unreal.CapsuleComponent):
        c.set_capsule_radius(34.0, True)
        c.set_capsule_half_height(96.0, True)

    # Force the Good/Bad sprite update (same as a viewport nudge).
    try:
        ps.post_edit_move(True)
    except Exception:
        pass

    dtable = None
    if table:
        tl = table.get_actor_location()
        dtable = round(((seat.x - tl.x) ** 2 + (seat.y - tl.y) ** 2) ** 0.5, 1)

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saved = bool(les.save_current_level()) if les else False
    wl = ps.get_actor_location()
    payload = {
        "ok": True,
        "saved": saved,
        "location": {"x": round(wl.x, 1), "y": round(wl.y, 1), "z": round(wl.z, 1)},
        "dist_to_table": dtable,
        "floor_z": round(floor_z, 1),
    }
