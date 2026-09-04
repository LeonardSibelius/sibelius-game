# dump_rocket_interior_bounds.py - where is the crew compartment, actually?
#
# *** RUN FROM THE OPEN EDITOR ***
#   Cmd box:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/dump_rocket_interior_bounds.py"
#
# Output: Saved/rocket_interior_bounds.json  (and the same, summarised, in the log)
#
# ===========================================================================
# WHY THIS EXISTS.
#
# Boarding put Leonard at ASpaceport's BoardingOffset - the centroid of the seven interior
# meshes' PLACEMENTS, read off MakeDefaultLayout. The teleport landed to the centimetre
# (log: "Boarded at Z=12804.799", exactly BoardingOffset.Z + the 96 cm capsule), and he
# arrived in open sky looking at a mountain.
#
# So the placement is right and the ASSUMPTION under it is wrong: a mesh's PIVOT is not
# necessarily inside its own geometry. All seven interior pieces sit at the same local Z
# and within 35 cm of each other in XY, which is what a set of props exported from one
# scene with a SHARED origin looks like - not what a room looks like. The room itself is
# wherever those meshes' BOUNDS are, and bounds are not readable from a path string.
#
# This reads them. For each mesh it reports the bounds ORIGIN (the offset from the pivot to
# the centre of the geometry) and the box EXTENT (its half-size). Feed those back into
# BoardingOffset and Leonard stands in the room instead of beside it.
#
# NOTHING IS MODIFIED. This opens no level, saves no asset, and moves nothing - it loads
# meshes and measures them. (The standing rule that editor Python must not load_levels or
# delete a .umap is not even in play here, but it is why this only ever reads.)

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/rocket_interior_bounds.json"

ROOT = "/Game/Rocket_Launch_Pad/Meshes/Rocket/"

# The seven interior pieces, plus the rocket itself for context - it is the hull they are
# supposed to be inside, and its extent says whether 11900 is near its nose or past it.
NAMES = [
    "SM_Walls",
    "SM_Back_Wall",
    "SM_Interface",
    "SM_Power_Box",
    "SM_Storage_Bags",
    "SM_Seats",
    "SM_Controls",
    "SM_Rocket",
]

# What ASpaceport::MakeDefaultLayout places them at, so the report can do the arithmetic
# rather than leaving it to be done by hand a second time. Local space, cm.
PLACED = {
    "SM_Walls":        (-19.6, 314.1, 11900.5),
    "SM_Back_Wall":    (-19.6, 314.1, 11900.5),
    "SM_Interface":    (-19.6, 314.1, 11900.5),
    "SM_Power_Box":    (-19.6, 314.1, 11900.5),
    "SM_Storage_Bags": (-19.6, 314.1, 11900.5),
    "SM_Seats":        (-52.8, 294.4, 11900.5),
    "SM_Controls":     (-62.0, 287.2, 11930.0),
    "SM_Rocket":       (0.0, 360.2, 1433.2),
}

# The interior parts are placed at this scale; SM_Rocket and SM_Controls are not.
SCALED = {
    "SM_Walls": 1.352, "SM_Back_Wall": 1.352, "SM_Interface": 1.352,
    "SM_Power_Box": 1.352, "SM_Storage_Bags": 1.352, "SM_Seats": 1.352,
    "SM_Controls": 1.0, "SM_Rocket": 1.0,
}

report = {"meshes": []}


def measure(name):
    path = ROOT + name + "." + name
    mesh = unreal.load_asset(path)
    if mesh is None:
        return {"name": name, "error": "did not load: " + path}

    b = mesh.get_bounds()
    o = b.origin
    e = b.box_extent
    s = SCALED.get(name, 1.0)
    px, py, pz = PLACED.get(name, (0.0, 0.0, 0.0))

    # WHERE THE GEOMETRY ACTUALLY SITS in the spaceport's local space: the placement, plus
    # the pivot-to-geometry offset, scaled the way the part is scaled. Rotation is ignored
    # here on purpose - the interior parts share a yaw, so this is the shape of the answer,
    # and the exact spot gets confirmed by standing in it.
    return {
        "name": name,
        "bounds_origin": [o.x, o.y, o.z],
        "box_extent": [e.x, e.y, e.z],
        "placed_at": [px, py, pz],
        "scale": s,
        "geometry_centre_local": [px + o.x * s, py + o.y * s, pz + o.z * s],
        "geometry_floor_z_local": pz + (o.z - e.z) * s,
        "size_cm": [e.x * 2.0 * s, e.y * 2.0 * s, e.z * 2.0 * s],
    }


try:
    for n in NAMES:
        report["meshes"].append(measure(n))

    # The one number boarding needs: the centre of the walls, and the floor under it.
    walls = next((m for m in report["meshes"] if m["name"] == "SM_Walls" and "error" not in m), None)
    if walls:
        report["suggested_BoardingOffset"] = [
            round(walls["geometry_centre_local"][0], 1),
            round(walls["geometry_centre_local"][1], 1),
            round(walls["geometry_floor_z_local"], 1),
        ]
except Exception:
    report["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(report, fh, indent=2)

unreal.log("###INTERIOR### %s" % json.dumps(report))
