# survey_court_site.py - where in the Transhuman Chess Park a basketball half-court fits.
#
# Walt, 2026-10-15: the three-point contest's court goes "in the chess park". The park is
# bought scenery and is not rendered, so the place is found by measuring: a height map of the
# ground round the chess table (one trace straight down every metre), then every candidate
# court - its footprint, in four orientations, at points every 4 m - scored for how flat and
# empty that footprint is. Trees, benches, walls and steps all show as jumps in the height.
#
# READ-ONLY. RUN with L_Chess open, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/survey_court_site.py"
# It prints the best sites; place_basketball_court.py places the court at the one chosen.

import math
import unreal

TAG = "###COURTSITE###"
PARK_MAP = "/Game/Maps/L_Chess"
RADIUS = 6000.0      # cm round the chess table to search
CELL = 100.0         # height map spacing
STEP = 400.0         # candidate spacing
FLAT = 25.0          # cm: the most the ground may rise or fall across a footprint

# Footprints in the court's own frame (origin under the rim, +X toward half court), with the
# slab's apron: the full half-court, and the three-point area only (what the contest uses).
FOOTPRINTS = {
    "full half-court": (-360.0, 1373.0, -912.0, 912.0),
    "three-point area": (-360.0, 1050.0, -830.0, 830.0),
}

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def main():
    world = ues.get_editor_world()
    if PARK_MAP not in world.get_path_name():
        say("REFUSING: the open map is %s, not %s." % (world.get_path_name(), PARK_MAP))
        return
    actors = eas.get_all_level_actors()
    chess = next((a for a in actors if a.get_class().get_name() == "ChessTable"), None)
    if not chess:
        say("no ChessTable here.")
        return
    anchor = chess.get_actor_location()
    say("chess table at (%.0f, %.0f, %.0f)" % (anchor.x, anchor.y, anchor.z))

    # The height map: the first thing a ray straight down meets, every CELL cm.
    n = int(RADIUS * 2 / CELL) + 1
    heights = {}
    names = {}
    top, bottom = anchor.z + 3000.0, anchor.z - 3000.0
    for i in range(n):
        for j in range(n):
            x = anchor.x - RADIUS + i * CELL
            y = anchor.y - RADIUS + j * CELL
            hit = unreal.SystemLibrary.line_trace_single(
                world, unreal.Vector(x, y, top), unreal.Vector(x, y, bottom),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
            t = hit.to_tuple() if hit else None
            if t and t[0]:
                heights[(i, j)] = t[5].z
                names[(i, j)] = t[9].get_actor_label() if t[9] else "?"
    say("height map: %d of %d points hit something" % (len(heights), n * n))

    def cell_of(x, y):
        return int(round((x - anchor.x + RADIUS) / CELL)), int(round((y - anchor.y + RADIUS) / CELL))

    results = []
    steps = int(RADIUS * 2 / STEP) + 1
    for label, (x0, x1, y0, y1) in FOOTPRINTS.items():
        for si in range(steps):
            for sj in range(steps):
                cx = anchor.x - RADIUS + si * STEP
                cy = anchor.y - RADIUS + sj * STEP
                for yaw in (0, 90, 180, 270):
                    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
                    zs = []
                    seen = {}
                    missing = 0
                    u = x0
                    while u <= x1:
                        v = y0
                        while v <= y1:
                            wx, wy = cx + u * c - v * s, cy + u * s + v * c
                            key = cell_of(wx, wy)
                            if key in heights:
                                zs.append(heights[key])
                                seen[names[key]] = seen.get(names[key], 0) + 1
                            else:
                                missing += 1
                            v += CELL
                        u += CELL
                    if missing or not zs:
                        continue
                    zs.sort()
                    rise = zs[-1] - zs[0]
                    if rise > FLAT:
                        continue
                    # At the park's own level, not on a roof: the first survey's "flat" sites were
                    # all on the cafe's roof, 11 m up (2026-10-15). The chess table stands on the
                    # ground; its top is under a metre up.
                    if not (anchor.z - 300.0 < zs[len(zs) // 2] < anchor.z + 30.0):
                        continue
                    dist = math.hypot(cx - anchor.x, cy - anchor.y)
                    results.append((label, dist, cx, cy, yaw, rise, zs[len(zs) // 2], seen))
    say("%d flat, empty footprints found" % len(results))
    for label in FOOTPRINTS:
        best = sorted([r for r in results if r[0] == label], key=lambda r: r[1])[:5]
        say("--- %s: %d sites; nearest to the chess table:" % (label, len([r for r in results if r[0] == label])))
        for _, dist, cx, cy, yaw, rise, z, seen in best:
            ground = ", ".join("%s x%d" % (k, v) for k, v in sorted(seen.items(), key=lambda kv: -kv[1])[:3])
            say("  rim at (%.0f, %.0f) z %.0f, court facing yaw %d, %.0f m from the chess table, ground varies %.0f cm; ground is %s" % (
                cx, cy, z, yaw, dist / 100.0, rise, ground))


main()
