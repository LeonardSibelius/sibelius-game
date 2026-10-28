# place_lantern_burst.py - hang Lantern Burst (ALanternBurst) in Kaia's jungle.
#
# Walt, 2026-10-27: a new side game in Kaia's home, "addictive and attractive", not tied to her
# home's look or the story. The game is one actor: a lantern of orbs about 2.9 m across hanging
# from a hook 8.3 m up, its embers 1.15 m over the ground, and at its foot (actor -X) the launcher
# where E is pressed. The camera that plays it stands 10.5 m out along -X, 2 m up. Nothing of it
# collides with the level but the launcher (and a capsule that keeps pawns out of the lantern), so
# what matters is what he SEES: no tree or rock in the lantern's own space or above it, and nothing
# between the lantern and the camera.
#
# WHERE: candidates 7 to 13 m from where he arrives, the launcher facing the arrival (or up to 40
# degrees off it), so he walks straight up to it. Each is scored by traces from high above: the
# lantern's footprint and the space over it, then a corridor 5 m wide out to the camera. Anything
# not landscape is an obstruction; so is ground under the lantern more than 50 cm off its foot, or
# ground in the corridor high enough to block the view. The fewest obstructions wins, then the
# flattest, then the nearest to 10 m. Not within 5 m of Kaia.
#
# A LANTERN ALREADY HUNG IS LEFT WHERE IT IS (place_home_portal.py's rule). Saves only a clean
# map, as place_orbit_golf.py does.
#
# *** RUN WITH L_KaiaHome OPEN, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_lantern_burst.py"

import math
import unreal

TAG = "###LANTERNBURST###"
MAP = "/Game/Maps/L_KaiaHome"
LABEL = "LanternBurst"
ARRIVAL_TAG = "KaiaHome"
AGENT = "Kaia"
RINGS = (700.0, 850.0, 1000.0, 1150.0, 1300.0)
TURNS = (0.0, -20.0, 20.0, -40.0, 40.0)
CLEAR_OF_AGENT = 500.0
FOOT_SLACK = 50.0          # ground under the lantern may be this far off its foot
CAMERA = (-1050.0, 200.0)  # ALanternBurst::CameraDistance, CameraHeight
LANTERN = [(0.0, 0.0)] + [(r * math.cos(k * math.pi / 4.0), r * math.sin(k * math.pi / 4.0))
                          for r in (110.0, 200.0) for k in range(8)]
CORRIDOR = [(-x, y) for x in range(260, 1151, 110) for y in (-250.0, 0.0, 250.0)]

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def say(text):
    print("%s %s" % (TAG, text))


def ground(x, y, top):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, top), unreal.Vector(x, y, top - 30000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return t[4].z, ("Landscape" in t[9].get_class().get_name()) if t[9] else False


def main():
    if MAP not in world.get_path_name():
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (world.get_path_name(), MAP))
        return
    actors = eas.get_all_level_actors()
    if any(a.get_actor_label() == LABEL for a in actors):
        say("%s already hung - left where it is." % LABEL)
        return
    cls = unreal.load_class(None, "/Script/SibeliusGame.LanternBurst")
    if not cls:
        say("no LanternBurst class - build the game module first. Nothing changed.")
        return
    start = next((a for a in actors if a.get_class().get_name() == "PlayerStart"
                  and str(a.get_editor_property("player_start_tag")) == ARRIVAL_TAG), None)
    agent = next((a for a in actors if a.get_actor_label() == AGENT), None)
    if not start:
        say("no PlayerStart tagged %s. Nothing changed." % ARRIVAL_TAG)
        return
    arrive = start.get_actor_location()
    she = agent.get_actor_location() if agent else None
    top = arrive.z + 6000.0
    was_dirty = any(MAP in str(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())

    best = None
    for ring in RINGS:
        for step in range(24):
            ang = step * math.pi / 12.0
            x, y = arrive.x + ring * math.cos(ang), arrive.y + ring * math.sin(ang)
            if she and math.hypot(x - she.x, y - she.y) < CLEAR_OF_AGENT:
                continue
            foot = ground(x, y, top)
            if foot is None or not foot[1]:
                continue
            base = foot[0]
            for turn in TURNS:
                # The launcher (actor -X) faces the arrival: +X points from the arrival out through the lantern.
                yaw = math.degrees(ang) + turn
                c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
                blocked, uneven = 0, 0.0
                for lx, ly in LANTERN:
                    h = ground(x + lx * c - ly * s, y + lx * s + ly * c, top)
                    if h is None or not h[1] or abs(h[0] - base) > FOOT_SLACK:
                        blocked += 1
                    else:
                        uneven += abs(h[0] - base)
                for lx, ly in CORRIDOR:
                    h = ground(x + lx * c - ly * s, y + lx * s + ly * c, top)
                    # The view runs from the camera down to the launcher: ground over that line blocks it.
                    sight = CAMERA[1] * (lx / CAMERA[0]) if ly == 0.0 else CAMERA[1] * 0.6
                    if h is None or not h[1] or h[0] - base > sight:
                        blocked += 1
                score = (blocked, uneven / 100.0, abs(ring - 1000.0) / 100.0, abs(turn))
                if best is None or score < best[0]:
                    best = (score, x, y, base, yaw, ring)
    if not best:
        say("no open, landscape-only ground found round the arrival. Nothing changed.")
        return

    (blocked, _, _, _), x, y, base, yaw, ring = best
    lantern = eas.spawn_actor_from_class(cls, unreal.Vector(x, y, base), unreal.Rotator(0.0, 0.0, yaw))
    lantern.set_actor_label(LABEL)
    say("hung %s at (%.0f, %.0f, %.0f), yaw %.0f: %.1f m from the arrival, %d of %d points obstructed%s." % (
        LABEL, x, y, base, yaw, ring / 100.0, blocked, len(LANTERN) + len(CORRIDOR),
        (", %.1f m from Kaia" % (math.hypot(x - she.x, y - she.y) / 100.0)) if she else ""))

    if was_dirty:
        say("NOT SAVED: the map already had unsaved changes - look them over and save it yourself (Ctrl+S).")
    else:
        les.save_current_level()
        say("saved %s." % MAP)


main()
