"""probe_office_floor.py -- read-only. Find clear floor in the office for a second
machine, by testing candidate footprints against every actor bound in the level.

Guessing at level space is how the first machine's bins ended up on bathroom tile
through a wall (see build_legacy_machine.py). Measure first.

    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script=".../probe_office_floor.py"
"""
import json
import unreal

MAP = "/Game/L_Office_v02"

# Machine 1 stands here, for reference and to avoid.
M1 = unreal.Vector(-1900.0, 9650.0, 90.0)

# What a 4-station machine needs, in cm, as a half-extent box around its centre.
NEED_X = 130.0     # player side (-X) plus the bed
NEED_Y = 230.0     # 4 stations at 75 plus bins and the readouts
NEED_Z = 110.0

FLOOR_Z = 90.0     # crate bases on machine 1 sit here; treat it as the standing surface

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)

# Collect solid world bounds once. Skip the things that are not obstacles.
SKIP = ("Light", "PostProcess", "PlayerStart", "Fog", "Volume", "Reflection",
        "Camera", "Note", "Sky", "Decal", "LegacyPart_", "LegacyMachine")
boxes = []
for a in eas.get_all_level_actors():
    try:
        label = a.get_actor_label()
        cls = a.get_class().get_name()
    except Exception:
        continue
    if any(s.lower() in label.lower() or s.lower() in cls.lower() for s in SKIP):
        continue
    try:
        origin, extent = a.get_actor_bounds(only_colliding_components=True)
    except Exception:
        continue
    if extent.x <= 1.0 and extent.y <= 1.0:
        continue
    boxes.append((origin, extent, label))


def blockers_at(cx, cy):
    """Everything a NEED_X/Y/Z box centred here would intersect."""
    lo_x, hi_x = cx - NEED_X, cx + NEED_X
    lo_y, hi_y = cy - NEED_Y, cy + NEED_Y
    lo_z, hi_z = FLOOR_Z + 5.0, FLOOR_Z + NEED_Z
    hits = []
    for origin, extent, label in boxes:
        if (origin.x - extent.x < hi_x and origin.x + extent.x > lo_x and
                origin.y - extent.y < hi_y and origin.y + extent.y > lo_y and
                origin.z - extent.z < hi_z and origin.z + extent.z > lo_z):
            hits.append(label)
    return hits


results = []
# Sweep the office floor around machine 1 on a 50cm grid.
x = M1.x - 700.0
while x <= M1.x + 700.0:
    y = M1.y - 1400.0
    while y <= M1.y + 1400.0:
        hits = blockers_at(x, y)
        results.append({"x": round(x, 0), "y": round(y, 0), "n": len(hits),
                        "blockers": sorted(set(hits))[:6],
                        "dist": round(((x - M1.x) ** 2 + (y - M1.y) ** 2) ** 0.5, 0)})
        y += 50.0
    x += 50.0

results.sort(key=lambda r: (r["n"], r["dist"]))
out = {"candidates_found": sum(1 for r in results if r["n"] == 0),
       "need_half_extent": [NEED_X, NEED_Y, NEED_Z],
       "obstacles_considered": len(boxes),
       "nearest_clear": results[:25]}
text = json.dumps(out, indent=2)
p = unreal.Paths.project_saved_dir() + "probe_office_floor.json"
try:
    with open(p, "w") as f:
        f.write(text)
except Exception as e:
    unreal.log_error("write failed: %s" % e)
unreal.log(text)
