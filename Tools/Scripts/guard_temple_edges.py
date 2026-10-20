# guard_temple_edges.py - invisible walls along every edge of the temple that drops into the void.
#
# Walt, 2026-10-19, in L_Archaic_Temple: "i was able to fall out of the world by going up the stairs
# and turning the corner." Mapped (numbers only - the level is a bought pack's) in 1 m cells: the
# upper floor at the top of the east stairs runs sideways to the building's outer edge with nothing
# there; a one-metre ledge runs along the outside of both long walls; the giants' hall, the
# terraces' corners and an open court in the west upper floor end in sky. Olympus was built to be
# looked at, not walked - and since the stairs were made climbable (fix_temple_stairs.py), all of
# it can be reached.
#
# So: wherever a walkable floor - the lower level (-200..120) or the upper floor (480..540) - meets a
# cell with nothing below it at all, and nothing at knee or chest height already stops a walker
# crossing, a thin invisible wall goes on the boundary, 4 m high. Drops INTO the temple (the
# galleries over the hall, the stairwells, the garden) are left open: falling there lands on floor.
# Walls are merged into straight runs, collision only, never seen, in the outliner folder VoidGuards.
#
# Walt, of the windows: "it is fine if i am stopped i don't want to walk out of the world."
#
# One transaction: a single Ctrl+Z removes them all. Does NOT save - Walt walks it, then Ctrl+S.
# Safe to re-run: it replaces the walls it made.
#
# *** RUN WITH L_Archaic_Temple OPEN, PIE stopped ***

import unreal

TAG = "###GUARD###"
MAP = "/Game/Maps/L_Archaic_Temple"
PREFIX = "VoidGuard_"
X0, X1, Y0, Y1, STEP = -7000, 16000, -6000, 6000, 100
LAYERS = ((-200.0, 120.0), (480.0, 540.0))    # the walkable floors
HEIGHT, THICK = 400.0, 20.0

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def say(msg):
    print("%s %s" % (TAG, msg))


def main():
    if not world.get_path_name().startswith(MAP + "."):
        say("open %s first - this is %s; nothing done" % (MAP, world.get_path_name()))
        return
    actors = eas.get_all_level_actors()
    old = [a for a in actors if a.get_actor_label().startswith(PREFIX)]
    skip = old + [a for a in actors if "Chandellier" in a.get_actor_label() or a.get_actor_label().startswith("SunShade")]

    def down(x, y):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 1000.0), unreal.Vector(x, y, -6000.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, skip,
                                                     unreal.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        return t[4].z if t and t[0] else None

    def open_between(a, b, z):
        for dz in (50.0, 130.0):
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(a[0], a[1], z + dz), unreal.Vector(b[0], b[1], z + dz),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, skip,
                                                         unreal.DrawDebugTrace.NONE, True)
            if hit is not None and hit.to_tuple()[0]:
                return False
        return True

    xs = list(range(X0, X1 + 1, STEP))
    ys = list(range(Y0, Y1 + 1, STEP))
    floor = {(x, y): down(x, y) for x in xs for y in ys}
    walkable = lambda z: z is not None and any(lo <= z <= hi for lo, hi in LAYERS)

    # Unguarded edges, as boundary segments: (axis, line, from, to, floor z).
    segs = []
    for (x, y), z in floor.items():
        if not walkable(z):
            continue
        for dx, dy in ((STEP, 0), (-STEP, 0), (0, STEP), (0, -STEP)):
            n = (x + dx, y + dy)
            if n in floor and floor[n] is None and open_between((x, y), n, z):
                if dx:
                    segs.append(("x", x + dx / 2.0, y - STEP / 2.0, y + STEP / 2.0, z))
                else:
                    segs.append(("y", y + dy / 2.0, x - STEP / 2.0, x + STEP / 2.0, z))
    # The map's own border: a walkable cell there may have more floor beyond, or sky - say so.
    border = [(x, y) for (x, y), z in floor.items() if walkable(z) and (x in (X0, X1) or y in (Y0, Y1))]
    if border:
        say("WARNING: %d walkable cells on the map's border - widen X0/X1/Y0/Y1: %s" % (len(border), border[:12]))

    # Merge into straight runs: same axis, same line, same floor (within 30 cm), touching.
    segs.sort(key=lambda s: (s[0], s[1], round(s[4] / 30.0), s[2]))
    runs = []
    for s in segs:
        r = runs[-1] if runs else None
        if r and r[0] == s[0] and r[1] == s[1] and abs(r[4] - s[4]) <= 30.0 and abs(r[3] - s[2]) < 1.0:
            runs[-1] = (r[0], r[1], r[2], s[3], max(r[4], s[4]))
        else:
            runs.append(s)
    say("%d unguarded edge(s) onto the void, merged into %d wall run(s)" % (len(segs), len(runs)))

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    with unreal.ScopedEditorTransaction("Invisible walls along the temple's edges onto the void"):
        for a in old:
            eas.destroy_actor(a)
        for i, (axis, line, a, b, z) in enumerate(runs):
            length = b - a
            if axis == "x":     # a wall across X at x=line, running along Y
                centre = unreal.Vector(line, 0.5 * (a + b), z - 20.0 + HEIGHT / 2.0)
                scale = unreal.Vector(THICK / 100.0, length / 100.0, HEIGHT / 100.0)
            else:               # at y=line, running along X
                centre = unreal.Vector(0.5 * (a + b), line, z - 20.0 + HEIGHT / 2.0)
                scale = unreal.Vector(length / 100.0, THICK / 100.0, HEIGHT / 100.0)
            w = eas.spawn_actor_from_object(cube, centre, unreal.Rotator(0.0, 0.0, 0.0))
            w.set_actor_label("%s%02d" % (PREFIX, i + 1))
            w.set_folder_path("VoidGuards")
            w.set_actor_scale3d(scale)
            c = w.get_component_by_class(unreal.StaticMeshComponent)
            c.set_collision_profile_name("BlockAll")
            c.set_editor_property("cast_shadow", False)
            c.set_editor_property("affect_distance_field_lighting", False)
            c.set_visibility(False)
            say("  %s%02d: %s = %.0f, from %.0f to %.0f (%.0f m), floor %.0f" % (PREFIX, i + 1, axis, line, a, b, length / 100.0, z))
    say("%d invisible wall(s) placed in VoidGuards. One Ctrl+Z removes them all. Not saved - walk it, then Ctrl+S." % len(runs))


main()
