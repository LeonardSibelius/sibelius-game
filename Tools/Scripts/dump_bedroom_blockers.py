"""Dump HiddenDoors, HatchLock, Elise, and anything sitting in the bedroom hall."""
import json
import math
import unreal

MAP = "/Game/L_Office_v02"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)


def lab(a):
    try:
        return a.get_actor_label()
    except Exception:
        return a.get_name()


def loc_of(a):
    p = a.get_actor_location()
    return [round(p.x, 1), round(p.y, 1), round(p.z, 1)]


def prop(a, name, default=None):
    try:
        return a.get_editor_property(name)
    except Exception:
        return default


doors = []
nearby = []
hatch = None
elise = None
compile_grant = None

# Bedroom hall doorway from the screenshot / prior placements
HALL = unreal.Vector(-2112.0, 9751.0, 400.0)

for a in eas.get_all_level_actors():
    n = lab(a)
    cls = a.get_class().get_name()
    p = a.get_actor_location()
    if cls in ("HiddenDoor", "SauceDoor"):
        box = None
        try:
            box = a.get_editor_property("blocking_box")
        except Exception:
            pass
        extent = None
        if box:
            try:
                e = box.get_unscaled_box_extent()
                extent = [round(e.x, 1), round(e.y, 1), round(e.z, 1)]
            except Exception:
                pass
        doors.append({
            "label": n,
            "cls": cls,
            "loc": loc_of(a),
            "rot": [round(a.get_actor_rotation().pitch, 1),
                    round(a.get_actor_rotation().yaw, 1),
                    round(a.get_actor_rotation().roll, 1)],
            "scale": [round(a.get_actor_scale3d().x, 3),
                      round(a.get_actor_scale3d().y, 3),
                      round(a.get_actor_scale3d().z, 3)],
            "travel": str(prop(a, "travel_target_level")),
            "arrival": str(prop(a, "arrival_tag")),
            "prompt": str(prop(a, "travel_prompt_text")),
            "box_extent": extent,
        })
    if n == "HatchLock" or cls == "HatchLock":
        hatch = {"label": n, "loc": loc_of(a), "cls": cls}
    if n == "BP_MHC_Elise":
        elise = {"label": n, "loc": loc_of(a), "cls": cls}
    if n == "PowerGrant_Compile":
        compile_grant = {"label": n, "loc": loc_of(a)}
        meshes = []
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            meshes.append({
                "name": c.get_name(),
                "visible": bool(c.is_visible()),
            })
        compile_grant["meshes"] = meshes

    d = p - HALL
    dist = math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z)
    if dist < 400.0 and cls not in ("WorldSettings", "Brush", "LevelBounds"):
        nearby.append({
            "label": n,
            "cls": cls,
            "loc": loc_of(a),
            "dist": round(dist, 1),
        })

nearby.sort(key=lambda x: x["dist"])
payload = {
    "doors": doors,
    "hatch": hatch,
    "elise": elise,
    "compile_grant": compile_grant,
    "near_hidden_door2": nearby[:40],
}
text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "dump_bedroom_blockers.json"
with open(out, "w", encoding="utf-8") as f:
    f.write(text)
print(text)
unreal.log("[dump_bedroom_blockers] wrote " + out)
