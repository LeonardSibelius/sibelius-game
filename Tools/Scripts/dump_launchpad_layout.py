# dump_launchpad_layout.py - steal the artist's composition, honestly.
#
# *** OPEN Content/Rocket_Launch_Pad/Levels/L_Rocket_Launch_Pad FIRST, THEN RUN THIS ***
#
# It reads whatever level is already open and never changes levels itself. Python that
# calls load_level fatals this editor at EditorServer.cpp:2524 - see
# make_city_from_downtown.py for the crash that taught us.
#
# ===========================================================================
# WHY THIS EXISTS.
#
# The first ASpaceport layout was twelve greybox parts at positions I guessed from a
# file listing, and Walt's verdict was that it looked like hell. He was right: you
# cannot compose a launch complex from mesh NAMES.
#
# But PackDev already composed one. L_Rocket_Launch_Pad is their showcase map - the pad,
# the rocket holder, the observation tower, the cooling towers and the pipework, arranged
# by the person who modelled them. Reading those transforms out and feeding them to
# MakeDefaultLayout means the spaceport materialises in the artist's arrangement instead
# of my arithmetic.
#
# This is a READ. It edits nothing, saves nothing, and spawns nothing.
#
# ---------------------------------------------------------------------------
# WHAT COMES BACK. One JSON entry per static mesh actor:
#
#     mesh      /Game/... path, ready to paste into a TSoftObjectPtr
#     loc       location RELATIVE to the level's own centre of mass, so the spaceport
#               is centred on the player's chosen spot rather than inheriting whatever
#               world coordinates the showcase map happened to use
#     rot/scale verbatim
#
# The re-centring matters: showcase maps are built wherever was convenient, and a layout
# copied with absolute coordinates would put the spaceport a kilometre from the lawn.

import json
import traceback

import unreal

TAG = "###LAUNCHPAD###"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/launchpad_layout.json"

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    r["open_level"] = ues.get_editor_world().get_path_name().split(".")[0]

    found = []
    for a in eas.get_all_level_actors():
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
        for c in comps:
            mesh = c.get_editor_property("static_mesh")
            if not mesh:
                continue
            path = mesh.get_path_name().split(".")[0]
            # Only the pack's own meshes. A showcase map usually also carries floor
            # planes, sky spheres and light rigs that are not part of the structure.
            if "/Rocket_Launch_Pad/" not in path:
                continue

            t = c.get_world_transform()
            loc = t.translation
            rot = t.rotation.rotator()
            scale = t.scale3d
            found.append({
                "actor": a.get_actor_label(),
                "mesh": "%s.%s" % (path, path.rsplit("/", 1)[1]),
                "loc": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
                "rot": [round(rot.pitch, 2), round(rot.yaw, 2), round(rot.roll, 2)],
                "scale": [round(scale.x, 4), round(scale.y, 4), round(scale.z, 4)],
            })

    r["count"] = len(found)

    if found:
        # Re-centre on the horizontal middle of the structure, and put Z=0 at its LOWEST
        # point - so the spaceport sits ON the lawn the player is standing on rather than
        # half-buried or hovering, whatever height the showcase map was built at.
        xs = [f["loc"][0] for f in found]
        ys = [f["loc"][1] for f in found]
        zs = [f["loc"][2] for f in found]
        cx = (min(xs) + max(xs)) / 2.0
        cy = (min(ys) + max(ys)) / 2.0
        cz = min(zs)
        r["recentred_from"] = [round(cx, 1), round(cy, 1), round(cz, 1)]
        r["extent"] = [round(max(xs) - min(xs), 1),
                       round(max(ys) - min(ys), 1),
                       round(max(zs) - min(zs), 1)]
        for f in found:
            f["loc"] = [round(f["loc"][0] - cx, 1),
                        round(f["loc"][1] - cy, 1),
                        round(f["loc"][2] - cz, 1)]

    r["parts"] = found
    r["next"] = ("Paste Saved/launchpad_layout.json back to Claude - it becomes "
                 "MakeDefaultLayout()." if found else
                 "NOTHING FOUND - is L_Rocket_Launch_Pad the open level?")

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("%s %d part(s) -> %s" % (TAG, r.get("count", 0), OUT))
print(json.dumps({k: v for k, v in r.items() if k != "parts"}, indent=2))
