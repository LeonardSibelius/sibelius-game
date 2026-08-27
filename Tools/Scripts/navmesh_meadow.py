# navmesh_meadow.py — give the meadow a floor the AI can see.
#
# *** RUN FROM THE OPEN EDITOR with L_Meadow loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/navmesh_meadow.py"
#
# Safe to re-run: reuses a volume if one is already here.
#
# ===========================================================================
# THE BUG THIS FIXES, AND THE ONE IT EXPLAINS.
#
# L_Meadow has no NavMeshBoundsVolume and no RecastNavMesh. L_Office_v02 has three of
# each. So every Refuser in the meadow calls MoveToActor, gets Failed, and stands there -
# which is why 30 Gideons with their AI switched on just stood in a ring at 15 m while
# Walt waited for a charge that could never come.
#
# INVOKER-ONLY GENERATION STILL NEEDS A BOUNDS VOLUME. DefaultEngine.ini sets
# bGenerateNavigationOnlyAroundNavigationInvokers=True, and it is easy to read that as
# "the invokers bring their own navmesh with them". They do not. The volume declares
# WHERE navmesh is permitted to exist; the invokers decide WHICH TILES inside it actually
# get built. With no volume, the permitted region is empty and no invoker can do anything
# about it.
#
# ---------------------------------------------------------------------------
# AND IT SETTLES AN EARLIER MISDIAGNOSIS, which is worth writing down because the wrong
# answer is still in a commit message.
#
# When 150 chasing Refusers ran at 3.7 fps, that was blamed on the navigation invokers -
# 150 of them supposedly asking for navmesh across a square kilometre of hillside. It was
# not. There was no volume, so they were asking for nothing. The cost was the
# [RefuserChase] log line at Display verbosity: 43,800 formatted lines in 145 seconds,
# into a Slate output-log widget.
#
# The proof came later and by accident: 30 chasing Refusers with invokers active and the
# log silenced measured 118 fps. Two things were changed in one build and the win was
# credited to the wrong one.
#
# So the AI cost of a real charge is STILL UNMEASURED - nothing in this project has ever
# actually pathed across the meadow. That measurement becomes possible for the first time
# after this script runs.
#
# ---------------------------------------------------------------------------
# AND DO NOT ADD A RECASTNAVMESH BY HAND - see the block below. The volume is the
# whole job; the navigation system makes its own nav data from it, and a factory-spawned
# one has no agent config and blocks the real one from ever being created.
#
# WHY THE VOLUME COVERS EVERYTHING, HILLS INCLUDED.
#
# It costs nothing to permit a large region under invoker-only generation - tiles are
# only built where a pawn actually is. Sizing it to the flat floor would save no frame
# time and would silently break the first fight that spills onto a slope.
#
# Z is generous for the same reason: the bowl is 60 m from floor to rim, so the volume
# spans well beyond that in both directions rather than clipping the hills at some
# height nobody would think to check.

import json
import traceback

import unreal

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/navmesh_meadow.json"

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    actors = eas.get_all_level_actors()
    landscape = next((a for a in actors if isinstance(a, unreal.Landscape)), None)
    if not landscape:
        raise Exception("no Landscape in this level - open L_Meadow")

    origin, extent = landscape.get_actor_bounds(False)
    r["landscape"] = {
        "centre": [round(origin.x, 1), round(origin.y, 1), round(origin.z, 1)],
        "extent": [round(extent.x, 1), round(extent.y, 1), round(extent.z, 1)],
    }

    # ---- the bounds volume -------------------------------------------------
    vol = next((a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume)), None)
    if not vol:
        vol = eas.spawn_actor_from_class(
            unreal.NavMeshBoundsVolume, unreal.Vector(origin.x, origin.y, origin.z))
        vol.set_actor_label("NavMeshBounds_Meadow")
        r["volume"] = "spawned"
    else:
        r["volume"] = "reused"

    vol.set_actor_location(unreal.Vector(origin.x, origin.y, origin.z), False, False)

    # A brush volume's default box is 200 uu on a side (half-extent 100), so the scale
    # needed is the wanted half-extent over 100. Margin on XY so the rim is inside; Z
    # spans far past the 60 m bowl rather than clipping it somewhere unmemorable.
    scale_xy = (max(extent.x, extent.y) * 1.05) / 100.0
    scale_z = max(extent.z * 2.0, 10000.0) / 100.0
    vol.set_actor_scale3d(unreal.Vector(scale_xy, scale_xy, scale_z))
    r["volume_scale"] = [round(scale_xy, 2), round(scale_xy, 2), round(scale_z, 2)]

    # ---- read the result back rather than trusting the set -----------------
    vo, ve = vol.get_actor_bounds(False)
    r["volume_bounds"] = {
        "centre": [round(vo.x, 1), round(vo.y, 1), round(vo.z, 1)],
        "extent": [round(ve.x, 1), round(ve.y, 1), round(ve.z, 1)],
    }
    r["covers_landscape_xy"] = bool(ve.x >= extent.x and ve.y >= extent.y)

    # ---- DO NOT SPAWN A RECASTNAVMESH. This script did, and it made things worse.
    #
    # A RecastNavMesh dropped in by the actor factory carries no NavDataConfig, so the
    # navigation system does not recognise it as nav data for any agent - and its mere
    # presence stops the system from creating the real one, because as far as
    # bAutoCreateNavigationData is concerned the level already has nav data.
    #
    # The symptom was exact and is worth recognising again:
    #     LogCrowdFollowing: Warning: Unable to find RecastNavMesh instance
    #     while trying to create UCrowdManager instance
    # ...logged at PIE start with a perfectly good RecastNavMesh sitting in the outliner.
    #
    # The navigation system builds its own from the bounds volume alone. Leave it to.
    after = eas.get_all_level_actors()
    strays = [a for a in after if isinstance(a, unreal.RecastNavMesh)]
    r["recast_strays_found"] = len(strays)
    for a in strays:
        # Only ones this script created. A hand-placed navmesh with a real config is
        # somebody's deliberate work and is not ours to remove.
        if a.get_actor_label() == "RecastNavMesh_Meadow":
            eas.destroy_actor(a)
            r["recast"] = "removed the stray this script used to spawn"
    r.setdefault("recast", "left to the navigation system, which is the point")

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###NAVMESH### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
