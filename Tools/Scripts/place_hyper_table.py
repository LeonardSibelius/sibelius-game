# place_hyper_table.py - stand the 4D billiards table (AHyperBilliardsTable) in Aisling's snowfield.
#
# Walt, 2026-10-25: "let's do the 4D billiards in Aisling's snowfield but that snowfield fills with
# red grass after a few seconds except for a few spots". The grass is the landscape's own
# (LGT_Grass_Red), grown at runtime around the player from its grass layer, so nothing in the
# level can be cleared to make room: the table stands on its own marble dais (the actor's, 8 m
# across, its top the actor's origin, 1.5 m deep), and the grass grows round it, not through it.
#
# WHERE: the flattest open ground 8 to 12 m from where he arrives, measured by traces - the dais's
# centre and eight points round its rim - and not within 4 m of Aisling, who dances in front of
# him. The dais top stands 20 cm above the highest of those points, so no slope rises through it.
# The table's near (-Y) side faces the arrival point: its camera and its W-board are set for a
# player at that side.
#
# A TABLE ALREADY STANDING IS LEFT WHERE IT IS (place_home_portal.py's rule): anything moved by
# hand since stays moved. Saves only a clean map, as that script does.
#
# *** RUN WITH L_AislingHome OPEN, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_hyper_table.py"

import math
import unreal

TAG = "###HYPERTABLE###"
MAP = "/Game/Maps/L_AislingHome"
LABEL = "HyperBilliards"
ARRIVAL_TAG = "AislingHome"
DAIS_RADIUS = 400.0
RINGS = (800.0, 950.0, 1100.0, 1200.0)
CLEAR_OF_AGENT = 400.0
LIFT = 20.0

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
    return t[4].z, (t[9].get_class().get_name() if t[9] else "?")


def main():
    if MAP not in world.get_path_name():
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (world.get_path_name(), MAP))
        return
    actors = eas.get_all_level_actors()
    if any(a.get_actor_label() == LABEL for a in actors):
        say("%s already stands - left where it is." % LABEL)
        return
    cls = unreal.load_class(None, "/Script/SibeliusGame.HyperBilliardsTable")
    if not cls:
        say("no HyperBilliardsTable class - build the game module first. Nothing changed.")
        return
    start = next((a for a in actors if a.get_class().get_name() == "PlayerStart"
                  and str(a.get_editor_property("player_start_tag")) == ARRIVAL_TAG), None)
    agent = next((a for a in actors if a.get_actor_label() == "Aisling"), None)
    if not start:
        say("no PlayerStart tagged %s. Nothing changed." % ARRIVAL_TAG)
        return
    arrive = start.get_actor_location()
    she = agent.get_actor_location() if agent else None
    was_dirty = any(MAP in str(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())

    best = None
    for ring in RINGS:
        for step in range(24):
            ang = step * math.pi / 12.0
            x, y = arrive.x + ring * math.cos(ang), arrive.y + ring * math.sin(ang)
            if she and math.hypot(x - she.x, y - she.y) < CLEAR_OF_AGENT + DAIS_RADIUS:
                continue
            top = arrive.z + 3000.0
            points = [(x, y)] + [(x + DAIS_RADIUS * math.cos(k * math.pi / 4.0), y + DAIS_RADIUS * math.sin(k * math.pi / 4.0)) for k in range(8)]
            hits = [ground(px, py, top) for px, py in points]
            if any(h is None or h[1] != "Landscape" for h in hits):
                continue   # off the landscape, or a rock or a plant in the way
            zs = [h[0] for h in hits]
            spread = max(zs) - min(zs)
            if best is None or spread < best[0]:
                best = (spread, x, y, max(zs))
    if not best:
        say("no open, landscape-only ground found round the arrival. Nothing changed.")
        return

    spread, x, y, high = best
    # The table's -Y side toward the arrival: local -Y = (sin yaw, -cos yaw).
    dx, dy = arrive.x - x, arrive.y - y
    yaw = math.degrees(math.atan2(dx / math.hypot(dx, dy), -dy / math.hypot(dx, dy)))
    table = eas.spawn_actor_from_class(cls, unreal.Vector(x, y, high + LIFT), unreal.Rotator(0.0, 0.0, yaw))
    table.set_actor_label(LABEL)
    say("placed %s at (%.0f, %.0f, %.0f), yaw %.0f: %.0f m from the arrival, ground varying %.0f cm under the dais%s." % (
        LABEL, x, y, high + LIFT, yaw, math.hypot(dx, dy) / 100.0, spread,
        (", %.0f m from Aisling" % (math.hypot(x - she.x, y - she.y) / 100.0)) if she else ""))

    if was_dirty:
        say("NOT SAVED: the map already had unsaved changes - look them over and save it yourself (Ctrl+S).")
    else:
        les.save_current_level()
        say("saved %s." % MAP)


main()
