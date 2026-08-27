# dress_meadow.py — put the player on the ground, give the bench something to spawn,
# and drop the sun so the hills have shape.
#
# *** RUN FROM THE OPEN EDITOR with L_Meadow loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/dress_meadow.py"
#
# Safe to re-run. Moves the existing PlayerStart rather than adding another, and reuses
# a spawner if one is already here.
#
# ---------------------------------------------------------------------------
# WHY THE SPAWNER, IN AN EMPTY FIELD.
#
# USwarmBenchSubsystem reads the Refuser class off any ARefuserSpawner in the level
# rather than naming an asset, so the bench always measures whatever the game actually
# fights. No spawner, nothing to spawn, and swarm.Bench refuses to run. It does not need
# to be ARMED - the office one fires on the hall alarm, and there is no alarm out here -
# it only needs to exist and know what a Refuser is.
#
# ---------------------------------------------------------------------------
# THE SUN IS ANGLED, NOT BRIGHTENED. Deliberately.
#
# Earlier today three rounds went into Kaia's cutscene lighting because intensities were
# set to numbers nobody had checked the units of - 18000 on a rect light whose engine
# default is about 8 - and auto-exposure hid the error until it came out as a glowing
# face. So this touches ROTATION only. The Basic template's sun is already correct for
# its own sky; a low angle costs nothing and is what makes a ridge line read, because
# hills are shape and shape is shadow. If it wants to be brighter, that is a decision
# made while looking at it, with a number somebody has checked.

import json
import traceback

import unreal

REFUSER_BP = "/Game/Characters/BP_Gideon_Refuser"
SUN_PITCH = -14.0     # low. -90 is straight down and flattens everything.
SUN_YAW = 125.0
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/dress_meadow.json"

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world()

    actors = eas.get_all_level_actors()
    landscape = next((a for a in actors if isinstance(a, unreal.Landscape)), None)
    if not landscape:
        raise Exception("no Landscape in this level - open L_Meadow")

    # ---- where the middle of the meadow is ---------------------------------
    origin, extent = landscape.get_actor_bounds(False)
    centre_x, centre_y = origin.x, origin.y
    r["landscape_bounds"] = {
        "centre": [round(centre_x, 1), round(centre_y, 1)],
        "extent": [round(extent.x, 1), round(extent.y, 1), round(extent.z, 1)],
    }

    # THE FLOOR IS THE LANDSCAPE'S OWN ZERO, and no trace is needed to find it.
    # make_meadow_heightmap.py writes the meadow floor at the 16-bit mid value, which is
    # by definition the landscape actor's Z.
    #
    # A line trace was tried first and cost a whole run: SystemLibrary.line_trace_single
    # returns a TUPLE, not a HitResult, so `hit` was truthy and `hit.impact_point` threw
    # - after the bounds had been read but BEFORE the spawner and the sun, so neither
    # happened and swarm.Bench had nothing to read. Reaching for a trace to find a height
    # this script already knows was the mistake; the exception was just the bill.
    ground_z = landscape.get_actor_location().z
    r["ground_z"] = round(ground_z, 1)
    r["ground_from"] = "landscape actor Z - the heightmap's mid value IS the floor"

    # ---- the player, standing on it ----------------------------------------
    start = next((a for a in actors if isinstance(a, unreal.PlayerStart)), None)
    if not start:
        start = eas.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(centre_x, centre_y, ground_z + 200.0))
        r["player_start"] = "spawned"
    else:
        r["player_start"] = "moved"
    # 200cm up: capsule half-height plus room, and the floor wobbles +/-45cm by design.
    start.set_actor_location(unreal.Vector(centre_x, centre_y, ground_z + 200.0), False, False)
    start.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=SUN_YAW + 180.0), False)

    # ---- something for the bench to spawn -----------------------------------
    spawner_cls = unreal.load_class(None, "/Script/SibeliusGame.RefuserSpawner")
    spawner = next((a for a in actors if a.get_class() == spawner_cls), None)
    if not spawner:
        spawner = eas.spawn_actor_from_class(
            spawner_cls, unreal.Vector(centre_x, centre_y, ground_z + 50.0))
        spawner.set_actor_label("RefuserSpawner_Meadow")
        r["spawner"] = "spawned"
    else:
        r["spawner"] = "reused"

    refuser_cls = unreal.EditorAssetLibrary.load_blueprint_class(REFUSER_BP)
    if refuser_cls:
        spawner.set_editor_property("RefuserClass", refuser_cls)
        r["refuser_class"] = REFUSER_BP
    else:
        r["refuser_class"] = "FAILED to load %s - swarm.Bench will refuse to run" % REFUSER_BP

    # ---- the sun, low ------------------------------------------------------
    sun = next((a for a in actors if isinstance(a, unreal.DirectionalLight)), None)
    if sun:
        sun.set_actor_rotation(
            unreal.Rotator(roll=0.0, pitch=SUN_PITCH, yaw=SUN_YAW), False)
        r["sun"] = {"pitch": SUN_PITCH, "yaw": SUN_YAW, "intensity": "untouched, on purpose"}
    else:
        r["sun"] = "no DirectionalLight found"

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###MEADOW### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
