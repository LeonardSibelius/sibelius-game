# place_universe_seed.py - the seed (AUniverseSeed) where Walt is looking: the gap he made in
# the upstairs hall by deleting a cabinet (2026-10-15).
#
# *** L_Office_v02 OPEN, PIE stopped. Point the level viewport at the floor where the seed
#     should stand, THEN RUN ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_universe_seed.py"
#
# A trace from the viewport camera straight ahead finds the floor; the seed stands there,
# turned to face the camera. DRY_RUN = True (the default) only says where it would go and what
# is within a metre of it. False places it (or moves the one already there). It does NOT save:
# L_Office_v02 is only ever saved by Walt, with Ctrl+S, in a healthy editor
# (place_poker_at_glass.py's rule - the save-on-close prompt has corrupted it twice).

import math
import unreal

TAG = "###SEED###"
DRY_RUN = True
OFFICE = "/Game/L_Office_v02"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def main():
    world = ues.get_editor_world()
    if OFFICE not in world.get_path_name():
        say("REFUSING: the open level is %s, not %s." % (world.get_path_name(), OFFICE))
        return
    seed_class = unreal.load_class(None, "/Script/SibeliusGame.UniverseSeed")
    if not seed_class:
        say("UniverseSeed class not built yet - rebuild the editor first.")
        return
    info = ues.get_level_viewport_camera_info()   # (location, rotation), or None
    if not info:
        say("no level viewport camera.")
        return
    cam, rot = info
    fwd = rot.get_forward_vector()
    actors = eas.get_all_level_actors()
    existing = [a for a in actors if a.get_class().get_name() == "UniverseSeed"]
    hit = unreal.SystemLibrary.line_trace_single(
        world, cam, cam + fwd * 3000.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, existing,
        unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    if not t or not t[0]:
        say("the viewport is not looking at anything within 30 m.")
        return
    spot, normal = t[5], t[7]
    if normal.z < 0.7:
        # Pointed at the wall behind the spot (the natural way to point at a gap against a
        # wall): stand back from it by the seed's half-width and a hand, and drop to the floor.
        wall = t[9].get_actor_label() if t[9] else "?"
        back = spot - fwd * 0.0 + unreal.Vector(normal.x, normal.y, 0.0) * 45.0
        down = unreal.SystemLibrary.line_trace_single(
            world, back, back - unreal.Vector(0, 0, 400.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, existing,
            unreal.DrawDebugTrace.NONE, True)
        d = down.to_tuple() if down else None
        if not d or not d[0] or d[7].z < 0.7:
            say("the viewport is looking at a wall (%s), and there is no floor in front of it - aim at the floor." % wall)
            return
        say("aimed at the wall (%s) - standing 45 cm out from it, on the floor below." % wall)
        spot = d[5]
    yaw = math.degrees(math.atan2(cam.y - spot.y, cam.x - spot.x))
    near = []
    for a in actors:
        if a in existing:
            continue
        o, e = a.get_actor_bounds(False)
        if abs(o.x - spot.x) < e.x + 60 and abs(o.y - spot.y) < e.y + 60 and o.z - e.z < spot.z + 200 and o.z + e.z > spot.z + 5:
            if e.x < 3000 and e.y < 3000:
                near.append("%s (%s)" % (a.get_actor_label(), a.get_class().get_name()))
    say("the seed %s at (%.0f, %.0f, %.0f), facing yaw %.0f, on %s; within reach: %s" % (
        "would stand" if DRY_RUN else "stands", spot.x, spot.y, spot.z, yaw,
        t[9].get_actor_label() if t[9] else "?", ", ".join(near) or "nothing"))
    if DRY_RUN:
        return
    seed = existing[0] if existing else eas.spawn_actor_from_class(seed_class, spot, unreal.Rotator(0, 0, yaw))
    seed.set_actor_location(spot, False, False)
    seed.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
    seed.set_actor_label("UniverseSeed")
    say("placed. NOT SAVED - Ctrl+S when it looks right.")


main()
