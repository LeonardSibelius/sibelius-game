# make_kart_city.py — L_Kart, the Transhuman Grand Prix: Transhuman City's streets with a
# race laid on them.
#
# Walt, 2026-10-09: "Transhuman City streets, go ahead and build it." As with the chess park
# (make_chess_park.py), L_Kart is L_City SAVED AS a new map with the story taken out - the
# dancers and Nyra, the doors, the spaceport site, the grants, the player starts - so the
# race cannot skip the city's story. Then an AKartRace goes on the streets: its TrackSpline
# gets the corners from Saved/KartSurvey/track.json (Tools/Scripts/plan_kart_track.py, from
# the survey), and anything small standing in the road - a parked car, a bench, a bin - is
# taken out of the karts' way. The road, the kerbs and the buildings stay.
#
# RUN, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_kart_city.py"
# With L_City open: DRY_RUN lists what would go; built, it saves the copy and stops (never
# load a level from a script - the UnrealClaude plugin crashed that way, 2026-09-28). Walt
# then opens L_Kart (File > Open Level) and it runs again to clear it and lay the track.
# With L_Kart open it is safe to re-run: it replaces its own race, start and removals.

import json
import math
import os
import re
import unreal

TAG = "###KARTCITY###"
DRY_RUN = True
SOURCE_MAP = "/Game/Maps/L_City"
KART_MAP = "/Game/Maps/L_Kart"
ARRIVAL_TAG = "KartRace"
TRACK = os.path.join(unreal.Paths.project_saved_dir(), "KartSurvey", "track.json")

STORY_CLASSES = re.compile(r"Door|BuildSite|Spaceport|Hint|Grant|Shrine|Travel|Guide|Nyra|Dancer|Agent|Portal|"
                           r"Altar|Placard|Refuser|Pickup|Shop|Purchase|PlayerStart|Trigger|Volume_Story|Chess|Kart",
                           re.IGNORECASE)
STORY_COMPONENTS = ("DancerAgentComponent", "NpcChatComponent", "InteractorComponent")
KEEP_CLASSES = ("StaticMeshActor", "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog",
                "VolumetricCloud", "PostProcessVolume", "WorldSettings", "Brush", "LightmassImportanceVolume")
# A thing in the road, not the road: smaller than this across, and standing up off it.
CLUTTER_MAX = 900.0
CLUTTER_RISE = 25.0

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def world_path():
    return ues.get_editor_world().get_path_name()


def story_actors():
    found = []
    for actor in eas.get_all_level_actors():
        cls = actor.get_class().get_name()
        if cls in KEEP_CLASSES:
            continue
        comps = [c.get_class().get_name() for c in actor.get_components_by_class(unreal.ActorComponent)]
        if STORY_CLASSES.search(cls) or any(c in STORY_COMPONENTS for c in comps):
            found.append(actor)
    return found


def load_track():
    with open(TRACK) as f:
        return json.load(f)


def seg_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    l2 = dx * dx + dy * dy
    t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / l2))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy)), t


def near_track(x, y, corners, halves, half):
    """Distance to the corner-to-corner polyline, the road height there, and the road's
    half-width there (each corner's, changing evenly along the straight, as the race does)."""
    best, z, w = 1e18, 0.0, half
    for k in range(len(corners)):
        a, b = corners[k], corners[(k + 1) % len(corners)]
        d, t = seg_distance(x, y, a[0], a[1], b[0], b[1])
        if d < best:
            ha = halves[k] if k < len(halves) and halves[k] > 0 else half
            hb = halves[(k + 1) % len(corners)] if (k + 1) % len(corners) < len(halves) and halves[(k + 1) % len(corners)] > 0 else half
            best, z, w = d, a[2] + (b[2] - a[2]) * t, ha + (hb - ha) * t
    return best, z, w


def clutter(corners, halves, half):
    """Things standing in the road itself: their footprint reaches inside its edge there.
    Never the tall things at the kerb - lamps, trees, signs - whose arms and canopies lean
    out over it: anything taller than 3 m stays."""
    found = []
    for a in eas.get_all_level_actors():
        if a.get_class().get_name() != "StaticMeshActor":
            continue
        o, e = a.get_actor_bounds(False)
        if max(e.x, e.y) * 2.0 > CLUTTER_MAX or e.z < 1.0 or e.z * 2.0 > 300.0:
            continue
        d, z, w = near_track(o.x, o.y, corners, halves, half)
        if d - min(e.x, e.y) > w - 20.0:
            continue
        if o.z + e.z < z + CLUTTER_RISE or o.z - e.z > z + 150.0:
            continue    # flat on the road (a marking, a drain) or up off it
        mesh = a.static_mesh_component.static_mesh
        found.append((a, mesh.get_name() if mesh else "?", d))
    return found


def grid_start(corners):
    """Where the start line is (the middle of the first straight), its direction, and where he
    arrives: a row behind the back of the grid, on the side of his kart."""
    a, b = corners[0], corners[1]
    mx, my, mz = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2
    l = math.hypot(b[0] - a[0], b[1] - a[1])
    dx, dy = (b[0] - a[0]) / l, (b[1] - a[1]) / l
    lx, ly = -dy, dx      # the model's left
    back = 700.0 + 650.0 * 2 + 750.0
    sx, sy = mx - dx * back + lx * -250.0, my - dy * back + ly * -250.0
    return (mx, my, mz), math.degrees(math.atan2(dy, dx)), (sx, sy, mz)


def main():
    here = world_path()
    if not os.path.exists(TRACK):
        say("no %s - run survey_kart_streets.py and plan_kart_track.py first." % TRACK)
        return
    plan = load_track()
    corners = plan["corners"]
    half = float(plan.get("half_width", 600.0))
    radius = float(plan.get("corner_radius", 900.0))
    say("track: %d corners, half-width %.0f, corner radius %.0f, %s" % (len(corners), half, radius, plan.get("note", "")))

    if DRY_RUN:
        if SOURCE_MAP not in here and KART_MAP not in here:
            say("open L_City (or L_Kart) first (open: %s)" % here)
            return
        story = story_actors()
        say("DRY RUN in %s: %d story actors would go" % (here, len(story)))
        for a in story:
            loc = a.get_actor_location()
            say("  %-30s %-32s (%.0f, %.0f, %.0f)" % (a.get_class().get_name(), a.get_actor_label(), loc.x, loc.y, loc.z))
        junk = clutter(corners, plan.get("half_widths", []), half)
        say("%d things stand in the road and would go:" % len(junk))
        for a, mesh, d in junk:
            say("  %-40s %-40s %.0f cm from the line" % (a.get_actor_label(), mesh, d))
        line, yaw, start = grid_start(corners)
        say("start line at (%.0f, %.0f, %.0f) facing %.0f deg; he arrives at (%.0f, %.0f)" % (line + (yaw,) + start[:2]))
        return

    if SOURCE_MAP in here:
        ok = unreal.EditorLoadingAndSavingUtils.save_map(ues.get_editor_world(), KART_MAP)
        say("saved a copy as %s: %s. Now open L_Kart (File > Open Level) and run this again." % (KART_MAP, ok))
        return
    if KART_MAP not in here:
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (here, KART_MAP))
        return

    gone = story_actors()
    for a in gone:
        eas.destroy_actor(a)
    junk = clutter(corners, plan.get("half_widths", []), half)
    for a, _, _ in junk:
        eas.destroy_actor(a)
    say("removed %d story actors and %d things in the road" % (len(gone), len(junk)))

    line, yaw, start = grid_start(corners)
    race_class = unreal.load_class(None, "/Script/SibeliusGame.KartRace")
    if not race_class:
        say("KartRace class not built yet - rebuild the game, then run this again.")
        return
    race = eas.spawn_actor_from_class(race_class, unreal.Vector(*line), unreal.Rotator(0.0, 0.0, 0.0))
    race.set_actor_label("KartRace")
    spline = race.get_editor_property("track_spline")
    spline.clear_spline_points(False)
    for i, c in enumerate(corners):
        spline.add_spline_point(unreal.Vector(c[0], c[1], c[2]), unreal.SplineCoordinateSpace.WORLD, False)
        spline.set_spline_point_type(i, unreal.SplinePointType.LINEAR, False)
    spline.set_closed_loop(True, False)
    spline.update_spline()
    # The properties last: each set is an edit, and an edit re-runs the actor's construction,
    # which builds the track from the spline just laid and puts the kerbs on the street.
    race.set_editor_property("corner_half_widths", [float(w) for w in plan.get("half_widths", [])])
    race.set_editor_property("laps", int(plan.get("laps", 3)))
    race.set_editor_property("corner_radius", radius)
    race.set_editor_property("half_width", half)
    kerbs = race.get_editor_property("kerb_red").get_instance_count() + race.get_editor_property("kerb_white").get_instance_count()
    say("the race's kerbs: %d blocks along the edges" % kerbs)
    if kerbs == 0:
        say("NO KERBS: the track did not build from the spline - check the Output Log for '[Kart] the track will not build'.")

    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(start[0], start[1], start[2] + 100.0),
                                    unreal.Rotator(0.0, 0.0, yaw))
    ps.set_editor_property("player_start_tag", ARRIVAL_TAG)
    ps.set_actor_label("PlayerStart_KartRace")
    les.save_current_level()
    say("laid the track (%d corners) and his arrival; saved %s." % (len(corners), KART_MAP))


main()
