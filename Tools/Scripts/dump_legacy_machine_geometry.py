"""dump_legacy_machine_geometry.py -- read-only. Where everything on the legacy machine
ACTUALLY is, in world centimetres.

Written because the label offsets on this machine have now been wrong three times, every
time from reasoning about mesh bounds instead of asking. The plates merged into one black
strip across the row and hid the travelling workpiece; before moving anything a fourth
time, measure.

    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script=".../dump_legacy_machine_geometry.py"
"""
import json
import unreal

MAP = "/Game/L_Office_v02"
out = {"parts": [], "machine": {}}

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def comp_report(c):
    loc = c.get_world_location()
    r = {"name": c.get_name(), "world": vec(loc), "rel": vec(c.get_editor_property("relative_location"))}
    try:
        s = c.get_editor_property("relative_scale3d")
        r["scale"] = vec(s)
    except Exception:
        pass
    try:
        mn, mx = c.get_editor_property("static_mesh").get_bounding_box().min, \
                 c.get_editor_property("static_mesh").get_bounding_box().max
        r["mesh_local_z"] = [round(mn.z, 1), round(mx.z, 1)]
        # world extent along Y (width) and Z (height) after scale
        r["world_size_yz"] = [round((mx.y - mn.y) * s.y, 1), round((mx.z - mn.z) * s.z, 1)]
    except Exception:
        pass
    try:
        r["text_world_size"] = round(c.get_editor_property("world_size"), 2)
    except Exception:
        pass
    return r


for a in eas.get_all_level_actors():
    try:
        label = a.get_actor_label()
    except Exception:
        continue

    if label.startswith("LegacyPart_"):
        entry = {"label": label, "actor_world": vec(a.get_actor_location()),
                 "actor_scale": vec(a.get_actor_scale3d()), "components": []}
        for prop in ("is_faulty", "fault_chance", "armed_by_grant", "fault_seed"):
            try:
                entry[prop] = str(a.get_editor_property(prop))
            except Exception as e:
                entry[prop] = "UNREADABLE: %s" % e
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            entry["components"].append(comp_report(c))
        for c in a.get_components_by_class(unreal.TextRenderComponent):
            entry["components"].append(comp_report(c))
        # the crate's own world-space vertical span, which is what the workpiece
        # has to clear
        root = a.get_editor_property("mesh")
        if root:
            sm = root.get_editor_property("static_mesh")
            if sm:
                b = sm.get_bounding_box()
                s = a.get_actor_scale3d().z
                z = a.get_actor_location().z
                entry["crate_world_z"] = [round(z + b.min.z * s, 1), round(z + b.max.z * s, 1)]
        out["parts"].append(entry)

    elif label == "LegacyMachine":
        out["machine"]["actor_world"] = vec(a.get_actor_location())
        out["machine"]["components"] = []
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            out["machine"]["components"].append(comp_report(c))
        for c in a.get_components_by_class(unreal.TextRenderComponent):
            out["machine"]["components"].append(comp_report(c))

text = json.dumps(out, indent=2)
p = unreal.Paths.project_saved_dir() + "legacy_machine_geometry.json"
try:
    with open(p, "w") as f:
        f.write(text)
except Exception as e:
    unreal.log_error("write failed: %s" % e)
unreal.log(text)
