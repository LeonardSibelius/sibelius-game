# place_grok_arrival.py - where Leonard lands on Grok, and where Nyra is waiting.
#
# *** RUN FROM THE OPEN EDITOR, WITH L_Grok OPEN ***
#   Cmd box:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_grok_arrival.py"
#
# Report: Saved/grok_arrival.json
#
# ===========================================================================
# THIS SCRIPT IS THE POINT, NOT A CONVENIENCE.
#
# Content/Maps/L_Grok.umap is GITIGNORED - it is 647 MB of purchased terrain and this
# repo is public (see .gitignore). So the level itself can never be committed, and
# anything placed BY HAND in it exists on exactly one machine and is one disk failure
# from gone.
#
# The repo therefore holds the RECIPE. make_grok_from_alien.py rebuilds the terrain from
# the pack; this rebuilds what stands on it. Between them, a clean clone can reconstruct
# Grok. That is the pattern place_supply_counter.py and place_spaceport_lawn_marker.py
# already follow, and here it is not a style preference - it is the only way this level's
# contents survive at all.
#
# EDIT THE NUMBERS HERE, NOT IN THE VIEWPORT. A drag in the editor is lost; a number
# changed here is committed.
#
# ---------------------------------------------------------------------------
# IDEMPOTENT, AND IT NEVER DELETES ANYTHING.
#
# Actors are found by tag and MOVED if they already exist. Nothing is destroyed - the
# standing rule is that editor Python which deletes level content fatals the editor, and
# a re-run that quietly threw away a hand-adjusted pose would be its own kind of loss.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/grok_arrival.json"

# --- the shot, read off the viewport with dump_viewport_camera.py (2026-09-05) --------
# Walt flew to the spot he wanted and this is where he was standing. Yaw came back as
# 2814.6 - the viewport reports it unwrapped after a few full turns - so it is normalised
# here to the same heading, 294.6.
ARRIVAL = unreal.Vector(193891.4, 149016.0, -1229.3)
FACING_YAW = 294.6

# How far in front of him Nyra waits. 500 cm: the city guide talks at 320 and that is the
# conversational distance, but this is also the last shot of the game and wants enough air
# to see the landscape she is standing in.
NYRA_AHEAD = 500.0

PLAYER_CAPSULE_HALF_HEIGHT = 96.0   # ASibeliusGameCharacter's own value

NYRA_BP = "/Game/MetaHumans/MHC_NyraSolmere/BP_MHC_NyraSolmere"

TAG_START = "GrokArrival"
TAG_NYRA = "GrokNyra"

r = {"arrival": [ARRIVAL.x, ARRIVAL.y, ARRIVAL.z], "yaw": FACING_YAW}


def ground_at(world, x, y, z_hint):
    # TRACE DOWN AND STAND ON WHAT IS THERE. Three separate floating-character bugs in
    # this project were all the same mistake: a height derived from something that is not
    # the ground. The landscape here is at negative Z, so the trace starts well above the
    # hint and runs a long way past it.
    start = unreal.Vector(x, y, z_hint + 5000.0)
    end = unreal.Vector(x, y, z_hint - 20000.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [], unreal.DrawDebugTrace.NONE, True,
        unreal.LinearColor.RED, unreal.LinearColor.GREEN, 0.0)
    if hit:
        return hit.to_tuple()[4].z, True
    return z_hint, False


def find_tagged(world, tag):
    for a in unreal.GameplayStatics.get_all_actors_with_tag(world, tag):
        return a
    return None


def place(world, tag, cls_or_path, loc, yaw, sink):
    existing = find_tagged(world, tag)
    # unreal.Rotator is (ROLL, PITCH, YAW) in Python - NOT the C++ (Pitch, Yaw, Roll)
    # order. Getting this wrong once put a character face-down on the floor.
    rot = unreal.Rotator(0.0, 0.0, yaw)

    if existing:
        existing.set_actor_location(loc, False, False)
        existing.set_actor_rotation(rot, False)
        sink["moved"] = existing.get_name()
        return existing

    if isinstance(cls_or_path, str):
        loaded = unreal.EditorAssetLibrary.load_asset(cls_or_path)
        if loaded is None:
            sink["error"] = "could not load %s" % cls_or_path
            return None
        cls = loaded.generated_class()
    else:
        cls = cls_or_path

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    if actor:
        actor.tags = [tag]
        sink["spawned"] = actor.get_name()
    return actor


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        r["error"] = "no editor world"
        return

    if "L_Grok" not in world.get_name():
        r["error"] = "open L_Grok first - this is %s" % world.get_name()
        return

    # --- Leonard's arrival point ------------------------------------------------------
    gz, hit = ground_at(world, ARRIVAL.x, ARRIVAL.y, ARRIVAL.z)
    r["ground_hit"] = hit
    r["ground_z"] = round(gz, 1)

    start_info = {}
    place(world, TAG_START, unreal.PlayerStart,
          unreal.Vector(ARRIVAL.x, ARRIVAL.y, gz + PLAYER_CAPSULE_HALF_HEIGHT),
          FACING_YAW, start_info)
    r["player_start"] = start_info

    # --- Nyra, NYRA_AHEAD in front of him, turned to face him -------------------------
    fwd = unreal.Rotator(0.0, 0.0, FACING_YAW).get_forward_vector()
    nx = ARRIVAL.x + fwd.x * NYRA_AHEAD
    ny = ARRIVAL.y + fwd.y * NYRA_AHEAD
    nz, nhit = ground_at(world, nx, ny, gz)
    r["nyra_ground_hit"] = nhit

    # Her origin is at her FEET - assembled MetaHumans are AActor-derived - so she stands
    # ON the hit with no offset. Facing back down the arrival heading, at him.
    nyra_info = {}
    place(world, TAG_NYRA, NYRA_BP, unreal.Vector(nx, ny, nz), FACING_YAW + 180.0, nyra_info)
    r["nyra"] = nyra_info
    r["nyra_location"] = [round(nx, 1), round(ny, 1), round(nz, 1)]


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###GROK-ARRIVAL### %s" % json.dumps(r))
