"""Dump BuildSite ghost/final meshes and HatchLock."""
import json
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
    return [round(p.x), round(p.y), round(p.z)]


def mesh_name(comp):
    if not comp:
        return None
    try:
        m = comp.get_editor_property("static_mesh")
        return m.get_name() if m else None
    except Exception:
        return None


out = {"sites": [], "hatch": None}
for a in eas.get_all_level_actors():
    cls = a.get_class().get_name()
    if cls == "BuildSite":
        ghost = a.get_editor_property("ghost_mesh")
        final = a.get_editor_property("final_mesh")
        out["sites"].append({
            "label": lab(a),
            "loc": loc_of(a),
            "output": str(a.get_editor_property("output")),
            "cost": a.get_editor_property("cost"),
            "built": None,
            "ghost": mesh_name(ghost),
            "final": mesh_name(final),
            "ghost_as_orb": str(a.get_editor_property("ghost_as_orb")),
            "radius": a.get_editor_property("interact_radius"),
        })
    if cls == "HatchLock":
        out["hatch"] = {"label": lab(a), "loc": loc_of(a)}

text = json.dumps(out, indent=2)
path = unreal.Paths.project_saved_dir() + "dump_build_site_meshes.json"
with open(path, "w", encoding="utf-8") as f:
    f.write(text)
print(text)
