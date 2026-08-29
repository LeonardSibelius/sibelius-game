# make_placeholder_city.py - the freed world, as a box city, so [>] has somewhere to go.
#
# *** RUN FROM THE OPEN EDITOR. It creates or rebuilds /Game/Maps/L_City. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_placeholder_city.py"
#
# ===========================================================================
# WHAT THIS IS FOR, AND WHAT IT IS DELIBERATELY NOT.
#
# ABattleArrival now offers two doors when the field is empty: [O] back to the office
# and [>] onward. [>] travels to OnwardLevelName, and OpenLevel on a map that does not
# exist is a SILENT no-op - the fade would go to black, the watchdog would fire sixty
# seconds later, and the ending would look broken. So the destination has to exist
# before the choice is worth playing. This makes it exist.
#
# IT IS GREY BOXES ON PURPOSE. The point of a placeholder is to be honest about being
# one; a half-dressed city invites judgement of the dressing instead of the shape. What
# IS real here is the shape, because the shape is the part that has to be decided before
# any asset is bought:
#
#   - a STRAIGHT STREET running away from the player, 600 m of it
#   - towers close on both sides, so it is a corridor with a sky and not a field
#   - heights that vary, so the skyline is a line and not a wall
#   - a LOW SUN down the street's axis, throwing long shadows across it
#   - HEAVY FOG, which is doing the real work: it hides where the boxes stop
#
# That last one is the whole trick, and it is why four blocks can stand in for a city.
# What makes City Sample thrilling to walk around is density plus DISTANCE - streets
# continuing past where you can see. Fog buys the distance for nothing. Judge this level
# on whether the walk down it feels open; if it does, the same composition will feel
# open with real buildings, and if it does not, no amount of Fab money will fix it.
#
# HEIGHTS ARE SEEDED, not random. Re-running gives the same skyline, so a change you
# make to the lighting is a change to the lighting and not to a new city.

import json
import random
import traceback

import unreal

LEVEL = "/Game/Maps/L_City"
CUBE = "/Game/LevelPrototyping/Meshes/SM_Cube"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/make_placeholder_city.json"
TAG = "City_"          # every actor this script owns; re-running clears them first
SEED = 20260829

# The street, in centimetres.
STREET_LEN = 60000.0    # 600 m of it
STREET_HALF_W = 4000.0  # 40 m kerb to centre
BLOCK_SPACING = 4500.0  # a tower every 45 m
ROWS = 13               # per side

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    # ---- the level itself ------------------------------------------------
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        les.load_level(LEVEL)
        r["level"] = "opened existing"
    else:
        les.new_level(LEVEL)
        r["level"] = "created"

    cube = unreal.EditorAssetLibrary.load_asset(CUBE)
    if not cube:
        raise Exception("could not load %s" % CUBE)

    # Idempotent: drop everything this script made last time, keep everything a human
    # may have added by hand.
    killed = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith(TAG):
            eas.destroy_actor(a)
            killed += 1
    r["cleared"] = killed

    def box(label, loc, scale):
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc))
        a.set_actor_label(TAG + label)
        a.static_mesh_component.set_static_mesh(cube)
        a.set_actor_scale3d(unreal.Vector(*scale))
        # Static: this level is lit by one sun and nothing in it moves. Movable boxes
        # would cost shadow updates for nothing.
        a.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
        return a

    # ---- the ground ------------------------------------------------------
    # SM_Cube is 100 cm on a side with a centred pivot, so scale IS metres, and the z
    # placement is minus half the thickness to put the top face exactly at zero.
    #
    # IT RUNS 80 m BEHIND THE PLAYER START, and that is a bug fix, not a flourish. The
    # first version centred the slab on the street, so it began at x=0 - which is
    # exactly where the player spawns. Walt arrived standing on the lip and fell out of
    # the world looking up at the undersides of the boxes. A spawn point on the edge of
    # the floor is a spawn point off the floor.
    APRON = 8000.0
    box("Ground", ((STREET_LEN - APRON) * 0.5, 0.0, -10.0),
        ((STREET_LEN + APRON) / 100.0, 260.0, 0.2))

    # ---- the towers ------------------------------------------------------
    rand = random.Random(SEED)
    heights = []
    for i in range(ROWS):
        x = 6000.0 + i * BLOCK_SPACING
        for side in (-1.0, 1.0):
            h = rand.uniform(18.0, 95.0)          # metres tall
            w = rand.uniform(22.0, 34.0)
            d = rand.uniform(22.0, 34.0)
            # Deeper towers sit further back, so the kerb line is not a ruler.
            y = side * (STREET_HALF_W + d * 50.0)
            box("Tower_%02d_%s" % (i, "L" if side < 0 else "R"),
                (x, y, h * 50.0), (w, d, h))
            heights.append(round(h, 1))
    r["towers"] = len(heights)
    r["tallest_m"] = max(heights)
    r["shortest_m"] = min(heights)

    # ---- where he arrives ------------------------------------------------
    # Yaw 0 faces +X, which is straight down the street. He should be looking at the
    # long view the instant the fade lifts - not turning around to find it.
    start = next((a for a in eas.get_all_level_actors()
                  if isinstance(a, unreal.PlayerStart)), None)
    if not start:
        start = eas.spawn_actor_from_class(unreal.PlayerStart,
                                           unreal.Vector(0.0, 0.0, 120.0))
        r["player_start"] = "spawned"
    else:
        start.set_actor_location(unreal.Vector(0.0, 0.0, 120.0), False, False)
        r["player_start"] = "moved"
    start.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)

    # ---- light, sky, and the fog that does the real work -----------------
    sun = eas.spawn_actor_from_class(unreal.DirectionalLight,
                                     unreal.Vector(0.0, 0.0, 5000.0))
    sun.set_actor_label(TAG + "Sun")
    # Pitch -8 is a sun about to set, aimed BACK up the street so it throws long
    # shadows toward the player and lights the tower faces he is walking into.
    sun.set_actor_rotation(unreal.Rotator(0.0, -8.0, 175.0), False)
    sunc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if sunc:
        sunc.set_editor_property("intensity", 6.0)
        sunc.set_editor_property("light_color", unreal.Color(255, 214, 170))
        sunc.set_editor_property("atmosphere_sun_light", True)

    sky = eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label(TAG + "SkyAtmosphere")

    skl = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 3000.0))
    skl.set_actor_label(TAG + "SkyLight")
    sklc = skl.get_component_by_class(unreal.SkyLightComponent)
    if sklc:
        sklc.set_editor_property("real_time_capture", True)
        sklc.set_editor_property("intensity", 1.0)

    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog,
                                     unreal.Vector(0.0, 0.0, 0.0))
    fog.set_actor_label(TAG + "Fog")
    fogc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fogc:
        # THE DISTANCE KNOB. Turn this UP and the city gets bigger, because the eye
        # reads "I cannot see the end of it" as "there is more of it". Turn it down to
        # about 0.01 and you can see exactly where the boxes stop - which is the whole
        # illusion collapsing, and worth doing once, deliberately, to see what the fog
        # is actually buying.
        fogc.set_editor_property("fog_density", 0.035)
        fogc.set_editor_property("fog_height_falloff", 0.12)
        fogc.set_editor_property("start_distance", 800.0)
        fogc.set_editor_property("fog_inscattering_luminance",
                                 unreal.LinearColor(0.42, 0.46, 0.55, 1.0))

    # ---- read it back. Setters that lie cost this project an evening. ----
    made = [a.get_actor_label() for a in eas.get_all_level_actors()
            if a.get_actor_label().startswith(TAG)]
    r["spawned"] = len(made)
    r["readback"] = {
        "fog_density": float(fogc.get_editor_property("fog_density")) if fogc else None,
        "sun_intensity": float(sunc.get_editor_property("intensity")) if sunc else None,
        "start_loc": [start.get_actor_location().x,
                      start.get_actor_location().y,
                      start.get_actor_location().z],
        "start_yaw": start.get_actor_rotation().yaw,
    }

    les.save_current_level()
    r["saved"] = True
    r["next"] = ("Open /Game/Maps/L_City and walk down the street. Then win the meadow "
                 "battle and press > to arrive here properly.")

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###CITY### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
