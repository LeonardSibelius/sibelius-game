# place_orbit_golf.py - lay out the orbital golf course (AOrbitGolfCourse) on Elise's grassland.
#
# Walt, 2026-10-25: "let's do the orbital golf". The course is one actor: a marble tee platform
# (6 m across, its top the actor's origin, 1.5 m deep) and, down its +X axis, the hole's moons
# hanging 2.5 to 20 m up and out to 45 m away. Nothing of it collides with the level but the
# platform - the ball is the sim's - so what matters is what he SEES: no tree or rock standing in
# the moons, and no hill rising into the lowest of them (the Slingshot's first moon hangs 2.5 m
# over the grass).
#
# WHERE: candidates 10 to 15 m from where he arrives, the course pointing away from him or up to
# 60 degrees off that. Each is scored by traces - the platform's centre and rim, then a grid over
# the course's whole footprint: anything not landscape is an obstruction, and so is ground higher
# than the moons above it allow. The fewest obstructions wins, then the flattest platform.
# Not within 4 m of Elise.
#
# A COURSE ALREADY LAID OUT IS LEFT WHERE IT IS (place_home_portal.py's rule). Saves only a clean
# map, as that script does.
#
# *** RUN WITH L_EliseHome OPEN, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_orbit_golf.py"

import math
import unreal

TAG = "###ORBITGOLF###"
MAP = "/Game/Maps/L_EliseHome"
LABEL = "OrbitalGolf"
ARRIVAL_TAG = "EliseHome"
AGENT = "Elise"
PLATFORM_RADIUS = 300.0
RINGS = (1000.0, 1250.0, 1500.0)
TURNS = (0.0, -30.0, 30.0, -60.0, 60.0)    # off "straight away from the arrival", degrees
CLEAR_OF_AGENT = 400.0
LIFT = 20.0
HILLS = 300.0          # ground may rise this far over the tee where no moon hangs low
MOON_CLEARANCE = 100.0
# Every moon of the three holes, course space (OrbitGolfSim.cpp, FOrbitCourse::MakeDefault).
MOONS = ((2500.0, 0.0, 900.0, 400.0),
         (1800.0, 700.0, 800.0, 550.0), (3800.0, -300.0, 1100.0, 380.0),
         (2300.0, 0.0, 1500.0, 450.0))
GRID_X = [500.0 + 350.0 * i for i in range(12)]
GRID_Y = (-700.0, 0.0, 700.0, 1300.0)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def say(text):
    print("%s %s" % (TAG, text))


def ground(x, y, top):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, top), unreal.Vector(x, y, top - 20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return t[4].z, ("Landscape" in t[9].get_class().get_name()) if t[9] else False


def allowed_rise(lx, ly):
    """How far over the tee the ground may stand at (lx, ly): under a moon, its underside less a margin."""
    rise = HILLS
    for mx, my, mz, r in MOONS:
        d = math.hypot(lx - mx, ly - my)
        if d < r:
            rise = min(rise, mz - math.sqrt(r * r - d * d) - MOON_CLEARANCE)
    return rise


def main():
    if MAP not in world.get_path_name():
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (world.get_path_name(), MAP))
        return
    actors = eas.get_all_level_actors()
    if any(a.get_actor_label() == LABEL for a in actors):
        say("%s already laid out - left where it is." % LABEL)
        return
    cls = unreal.load_class(None, "/Script/SibeliusGame.OrbitGolfCourse")
    if not cls:
        say("no OrbitGolfCourse class - build the game module first. Nothing changed.")
        return
    start = next((a for a in actors if a.get_class().get_name() == "PlayerStart"
                  and str(a.get_editor_property("player_start_tag")) == ARRIVAL_TAG), None)
    agent = next((a for a in actors if a.get_actor_label() == AGENT), None)
    if not start:
        say("no PlayerStart tagged %s. Nothing changed." % ARRIVAL_TAG)
        return
    arrive = start.get_actor_location()
    she = agent.get_actor_location() if agent else None
    top = arrive.z + 5000.0
    was_dirty = any(MAP in str(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())

    best = None
    for ring in RINGS:
        for step in range(16):
            ang = step * math.pi / 8.0
            x, y = arrive.x + ring * math.cos(ang), arrive.y + ring * math.sin(ang)
            if she and math.hypot(x - she.x, y - she.y) < CLEAR_OF_AGENT + PLATFORM_RADIUS:
                continue
            rim = [(x, y)] + [(x + PLATFORM_RADIUS * math.cos(k * math.pi / 4.0), y + PLATFORM_RADIUS * math.sin(k * math.pi / 4.0)) for k in range(8)]
            hits = [ground(px, py, top) for px, py in rim]
            if any(h is None or not h[1] for h in hits):
                continue   # off the landscape, or something standing on the platform's ground
            zs = [h[0] for h in hits]
            spread, tee = max(zs) - min(zs), max(zs) + LIFT
            for turn in TURNS:
                yaw = math.degrees(ang) + turn    # ang points from the arrival out through the tee
                c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
                blocked, uneven = 0, 0.0
                for lx in GRID_X:
                    for ly in GRID_Y:
                        h = ground(x + lx * c - ly * s, y + lx * s + ly * c, top)
                        if h is None or not h[1] or h[0] - tee > allowed_rise(lx, ly):
                            blocked += 1
                        elif h is not None:
                            uneven += abs(h[0] - tee)
                score = (blocked, spread + uneven / 100.0, abs(turn))
                if best is None or score < best[0]:
                    best = (score, x, y, tee, yaw)
    if not best:
        say("no open, landscape-only ground found round the arrival. Nothing changed.")
        return

    (blocked, _, _), x, y, tee, yaw = best
    course = eas.spawn_actor_from_class(cls, unreal.Vector(x, y, tee), unreal.Rotator(0.0, 0.0, yaw))
    course.set_actor_label(LABEL)
    say("placed %s at (%.0f, %.0f, %.0f), yaw %.0f: %.0f m from the arrival, %d of %d course points obstructed%s." % (
        LABEL, x, y, tee, yaw, math.hypot(x - arrive.x, y - arrive.y) / 100.0, blocked, len(GRID_X) * len(GRID_Y),
        (", %.0f m from Elise" % (math.hypot(x - she.x, y - she.y) / 100.0)) if she else ""))

    if was_dirty:
        say("NOT SAVED: the map already had unsaved changes - look them over and save it yourself (Ctrl+S).")
    else:
        les.save_current_level()
        say("saved %s." % MAP)


main()
