# dump_supermarket_layout.py - is the showcase map a ROOM, or a display shelf?
#
# *** OPEN Content/Poly_Supermarket_01/Levels/L_Showcase FIRST, THEN RUN THIS ***
# (L_Overview works too - it reports whichever level is already open.)
#
# It reads whatever level is already open and NEVER changes levels itself. Python that
# calls load_level fatals this editor at EditorServer.cpp:2524 - see
# make_city_from_downtown.py for the crash that taught us that.
#
# ===========================================================================
# WHY THIS EXISTS, and why it asks a different question than its sibling.
#
# dump_launchpad_layout.py read PackDev's showcase map to steal a COMPOSITION, because
# ASpaceport rebuilds the launch complex from parts in C++ and needed 14 transforms.
#
# uFoods is not that. uFoods is a LEVEL, and the game already has the pattern for one:
# L_Cafe is committed to the repo and references RestaurantScene meshes that are
# gitignored. A .umap holds asset PATHS, not vendor bytes, so a level of ours built from
# a bought kit is fine to commit while the kit itself stays local.
#
# Which means the cheapest honest path is to DUPLICATE the pack's showcase level and
# redress it - IF the showcase is a real room. Showcase maps often are not; they are
# frequently every asset in the pack lined up in a row on an infinite grey plane, which
# is useless as a shop and worse than starting empty.
#
# So this script answers exactly that: room, or shelf?
#
#   extent          how big the thing is. A shop is tens of metres; a display row is
#                   long and thin and one mesh deep.
#   shell           floors, walls, columns, ceilings - the pieces that make a ROOM.
#                   Lots of these means the map is architecture, not a catalogue.
#   distinct        every mesh used and how many times. A display map tends to use each
#                   mesh exactly once; a real interior repeats floors and walls.
#   parts           full transforms, re-centred, in case we do want to rebuild by hand.
#
# This is a READ. It edits nothing, saves nothing, spawns nothing, and loads nothing.

import json
import traceback

import unreal

TAG = "###SUPERMARKET###"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/supermarket_layout.json"

# Pieces whose NAME says "this is the building, not the merchandise". Used only to
# summarise - every mesh is reported either way, so a bad guess here hides nothing.
SHELL_WORDS = ("floor", "wall", "column", "ceiling", "roof", "door", "window",
               "stair", "pillar", "beam")

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    r["open_level"] = ues.get_editor_world().get_path_name().split(".")[0]

    found = []
    foreign = 0
    for a in eas.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            mesh = c.get_editor_property("static_mesh")
            if not mesh:
                continue
            path = mesh.get_path_name().split(".")[0]
            # Only the pack's own meshes. Showcase maps carry sky spheres, backdrop
            # planes and light rigs that are not part of the shop.
            if "/Poly_Supermarket_01/" not in path:
                foreign += 1
                continue

            t = c.get_world_transform()
            loc, rot, scale = t.translation, t.rotation.rotator(), t.scale3d
            found.append({
                "actor": a.get_actor_label(),
                "mesh": "%s.%s" % (path, path.rsplit("/", 1)[1]),
                "loc": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
                "rot": [round(rot.pitch, 2), round(rot.yaw, 2), round(rot.roll, 2)],
                "scale": [round(scale.x, 4), round(scale.y, 4), round(scale.z, 4)],
            })

    r["count"] = len(found)
    r["non_pack_meshes_ignored"] = foreign

    if found:
        # How many times each mesh appears. Repetition is the signature of architecture:
        # you place one hero prop once, but you tile a floor twenty times.
        tally = {}
        for f in found:
            short = f["mesh"].rsplit("/", 1)[1].split(".")[0]
            tally[short] = tally.get(short, 0) + 1
        r["distinct"] = dict(sorted(tally.items(), key=lambda kv: (-kv[1], kv[0])))

        shell = {k: v for k, v in tally.items()
                 if any(w in k.lower() for w in SHELL_WORDS)}
        r["shell"] = dict(sorted(shell.items(), key=lambda kv: (-kv[1], kv[0])))
        r["shell_piece_count"] = sum(shell.values())

        xs = [f["loc"][0] for f in found]
        ys = [f["loc"][1] for f in found]
        zs = [f["loc"][2] for f in found]
        r["extent_cm"] = [round(max(xs) - min(xs), 1),
                          round(max(ys) - min(ys), 1),
                          round(max(zs) - min(zs), 1)]
        r["extent_m"] = [round((max(xs) - min(xs)) / 100.0, 1),
                         round((max(ys) - min(ys)) / 100.0, 1),
                         round((max(zs) - min(zs)) / 100.0, 1)]

        # Re-centre horizontally, floor at Z=0 - same convention as the launchpad dump,
        # so if we DO rebuild by hand the numbers drop straight into place.
        cx = (min(xs) + max(xs)) / 2.0
        cy = (min(ys) + max(ys)) / 2.0
        cz = min(zs)
        r["recentred_from"] = [round(cx, 1), round(cy, 1), round(cz, 1)]
        for f in found:
            f["loc"] = [round(f["loc"][0] - cx, 1),
                        round(f["loc"][1] - cy, 1),
                        round(f["loc"][2] - cz, 1)]

        # The verdict, stated plainly so it survives being pasted back.
        w, d, _h = r["extent_m"]
        looks_like_room = r["shell_piece_count"] >= 8 and min(w, d) >= 6.0
        r["verdict"] = ("ROOM - duplicating this level and redressing it is the cheap path"
                        if looks_like_room else
                        "DISPLAY/CATALOGUE - build L_uFoods from the Modular kit instead")

    r["parts"] = found
    r["next"] = ("Paste Saved/supermarket_layout.json back to Claude."
                 if found else
                 "NOTHING FOUND - is a Poly_Supermarket_01 level the OPEN level?")

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("%s %d part(s) -> %s" % (TAG, r.get("count", 0), OUT))
print(json.dumps({k: v for k, v in r.items() if k != "parts"}, indent=2))
