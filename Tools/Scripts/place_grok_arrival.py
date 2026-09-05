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

# WHERE NYRA STANDS - Walt's own placement, read back off the Details panel after he
# nudged her in the viewport on 2026-09-05.
#
# The script first put her 500 cm straight ahead, which is the conversational distance the
# city guide uses. He moved her to 685 cm and turned her to yaw 14.6 - further out, and
# not square-on. That is a framing decision made by eye, which is the only way framing
# decisions get made, so it is recorded here as an explicit position rather than
# re-derived from an angle and a distance.
#
# WHY IT HAD TO BE COPIED BACK. L_Grok.umap is gitignored. His nudge existed only in the
# level, on one machine, until it was written down here.
NYRA_LOCATION = unreal.Vector(194099.5, 148363.0, -1525.1)
NYRA_YAW = 14.6

PLAYER_CAPSULE_HALF_HEIGHT = 96.0   # ASibeliusGameCharacter's own value

NYRA_BP = "/Game/MetaHumans/MHC_NyraSolmere/BP_MHC_NyraSolmere"

TAG_START = "GrokArrival"
TAG_WORMHOLE = "GrokWormhole"
TAG_NYRA = "GrokNyra"

# THE TAG THAT MAKES HER A GUIDE RATHER THAN A POWER GRANTER.
#
# UDancerAgentComponent::IsGuide() is exactly
#     Owner && !GuideTag.IsNone() && Owner->ActorHasTag(GuideTag)
# and GuideTag defaults to "CityDancer". Without it she is not a guide, GuideStage()
# returns 0 before it ever looks at the level, and she gives the power-grant speech -
# which is what Walt got on Grok: "her speech is about having given a power use it wisely".
#
# The first version of this script did `actor.tags = [TAG_NYRA]`, which REPLACES the tag
# list. Setting a tag to find her by quietly removed the tag that made her herself.
GUIDE_TAG = "CityDancer"

r = {"arrival": [ARRIVAL.x, ARRIVAL.y, ARRIVAL.z], "yaw": FACING_YAW}


def ground_at(world, x, y, z_hint, ignore=None):
    # TRACE DOWN AND STAND ON WHAT IS THERE. Three separate floating-character bugs in
    # this project were all the same mistake: a height derived from something that is not
    # the ground. The landscape here is at negative Z, so the trace starts well above the
    # hint and runs a long way past it.
    #
    # IGNORE is not optional, it is the fourth bug (Walt, 2026-09-05: "Nyra is several feet
    # in the air"). This script MOVES actors it finds rather than respawning them, so on a
    # re-run the trace starts above Nyra and hits NYRA - and stands her on top of herself,
    # 157 cm up. Run it again and she climbs again. Whatever is being placed, and the
    # player, must be invisible to the trace that decides where it goes.
    start = unreal.Vector(x, y, z_hint + 5000.0)
    end = unreal.Vector(x, y, z_hint - 20000.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, ignore or [], unreal.DrawDebugTrace.NONE, True,
        unreal.LinearColor.RED, unreal.LinearColor.GREEN, 0.0)
    if hit:
        return hit.to_tuple()[4].z, True
    return z_hint, False


def find_tagged(world, tag):
    for a in unreal.GameplayStatics.get_all_actors_with_tag(world, tag):
        return a
    return None


def tags_for(tag):
    # GUIDE_TAG belongs to Nyra alone. place() is shared with the PlayerStart and the
    # wormhole actor, and neither should be answering [E] with a guide speech.
    return (tag, GUIDE_TAG) if tag == TAG_NYRA else (tag,)


def place(world, tag, cls_or_path, loc, yaw, sink):
    existing = find_tagged(world, tag)
    # unreal.Rotator is (ROLL, PITCH, YAW) in Python - NOT the C++ (Pitch, Yaw, Roll)
    # order. Getting this wrong once put a character face-down on the floor.
    rot = unreal.Rotator(0.0, 0.0, yaw)

    if existing:
        existing.set_actor_location(loc, False, False)
        existing.set_actor_rotation(rot, False)
        # A re-run must repair a Nyra placed before GUIDE_TAG existed, not just move her.
        tags = [t for t in existing.tags] if existing.tags else []
        for t in tags_for(tag):
            if t not in tags:
                tags.append(t)
        existing.tags = tags
        sink["moved"] = existing.get_name()
        sink["tags"] = [str(t) for t in tags]
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
        # ADD, never replace - see GUIDE_TAG. Keep whatever the blueprint already carries.
        existing_tags = [t for t in actor.tags] if actor.tags else []
        for t in tags_for(tag):
            if t not in existing_tags:
                existing_tags.append(t)
        actor.tags = existing_tags
        sink["spawned"] = actor.get_name()
    return actor


def actor_ignore_list(world, *tags):
    # Everything this script owns, plus the player. A trace that can see what it is about
    # to move will stand it on itself; a trace that can see the pawn will stand things on
    # the pawn's head.
    out = []
    for t in tags:
        a = find_tagged(world, t)
        if a:
            out.append(a)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn:
        out.append(pawn)
    return out


def stand_on(actor, ground_z):
    # STAND THE RENDERED BODY ON THE GROUND, NOT THE ORIGIN.
    #
    # A MetaHuman's origin is usually at its feet, but "usually" is how a character ends
    # up hovering. UDancerAgentComponent already solved this properly for the city guides:
    # measure how far below the actor's origin the body actually reaches, then stand THAT
    # on the floor. Read off the bounds, so a different rig or a scaled one both land.
    if not actor:
        return ground_z, 0.0
    origin, extent = actor.get_actor_bounds(False)
    feet_below = actor.get_actor_location().z - (origin.z - extent.z)
    if feet_below < 0.0:
        feet_below = 0.0
    loc = actor.get_actor_location()
    loc.z = ground_z + feet_below
    actor.set_actor_location(loc, False, False)
    return loc.z, feet_below


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        r["error"] = "no editor world"
        return

    if "L_Grok" not in world.get_name():
        r["error"] = "open L_Grok first - this is %s" % world.get_name()
        return

    ignore = actor_ignore_list(world, TAG_START, TAG_NYRA, TAG_WORMHOLE)
    r["ignored"] = [a.get_name() for a in ignore]

    # --- Leonard's arrival point ------------------------------------------------------
    gz, hit = ground_at(world, ARRIVAL.x, ARRIVAL.y, ARRIVAL.z, ignore)
    r["ground_hit"] = hit
    r["ground_z"] = round(gz, 1)

    start_info = {}
    place(world, TAG_START, unreal.PlayerStart,
          unreal.Vector(ARRIVAL.x, ARRIVAL.y, gz + PLAYER_CAPSULE_HALF_HEIGHT),
          FACING_YAW, start_info)
    r["player_start"] = start_info

    # --- Nyra, where Walt put her ----------------------------------------------------
    # Her XY is his; her Z is re-traced rather than trusted, so a later terrain edit or a
    # rebuilt landscape cannot leave her hovering. The trace is the authority on ground.
    nx, ny = NYRA_LOCATION.x, NYRA_LOCATION.y
    nz, nhit = ground_at(world, nx, ny, gz, ignore)
    r["nyra_ground_hit"] = nhit

    nyra_info = {}
    nyra = place(world, TAG_NYRA, NYRA_BP, unreal.Vector(nx, ny, nz), NYRA_YAW, nyra_info)
    # Placed first, THEN stood on the ground: her bounds are only readable once she exists.
    final_z, feet = stand_on(nyra, nz)
    r["nyra"] = nyra_info
    r["nyra_feet_below_origin"] = round(feet, 1)
    r["nyra_location"] = [round(nx, 1), round(ny, 1), round(final_z, 1)]

    # --- the wormhole arrival, at his feet --------------------------------------------
    # AWormholeArrival centres its cloud on the PAWN rather than on itself, so this only
    # has to exist in the level - but it is placed at the arrival point anyway so that
    # anyone opening L_Grok can see where the effect belongs.
    wh_info = {}
    wh_cls = unreal.load_class(None, "/Script/SibeliusGame.WormholeArrival")
    if wh_cls:
        place(world, TAG_WORMHOLE, wh_cls,
              unreal.Vector(ARRIVAL.x, ARRIVAL.y, gz + PLAYER_CAPSULE_HALF_HEIGHT),
              FACING_YAW, wh_info)
    else:
        wh_info["error"] = "WormholeArrival class not found - build the editor target first"
    r["wormhole"] = wh_info


def save_level():
    # SAVE IT, because forgetting to has now cost three playtests.
    #
    # Travelling from L_City to L_Grok LOADS THE LEVEL FROM DISK - it does not use the
    # editor's in-memory copy - so an unsaved placement is invisible to every playthrough
    # that arrives by portal, while looking perfectly correct if you just open the level.
    # Walt saved twice and a locked asset took the whole Save All down with it both times.
    #
    # Saving is not loading or deleting a level, so this does not go anywhere near the
    # rule that editor Python must not do those.
    try:
        ok = unreal.EditorLevelLibrary.save_current_level()
        r["saved"] = bool(ok)
    except Exception as e:
        r["saved"] = "FAILED: %s" % e


try:
    main()
    if "error" not in r:
        save_level()
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###GROK-ARRIVAL### %s" % json.dumps(r))
