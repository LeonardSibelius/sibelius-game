# greystone_render_test.py — does Greystone render AT ALL in this project?
#
# *** RUN FROM THE OPEN EDITOR with L_Meadow loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/greystone_render_test.py"
#
# ===========================================================================
# WHY THIS EXISTS.
#
# Battle form reports every property correct - in form, BattleCamera active, view 386 cm
# behind the pawn, mesh=Greystone, 2 m bounds at the pawn, 17 materials none null,
# mainPass=1, ownerNoSee=0, visible=1 - and a captured frame shows empty meadow. Walt
# sees a Greystone-shaped shadow holding a greatsword and no Greystone.
#
# Properties and pixels disagree, so the question is no longer "which property" but
# "does this mesh paint anything, anywhere". That splits the whole problem in two and
# nothing measured so far can answer it.
#
# THE PREVIOUS ATTEMPT AT THIS TEST WAS WORTHLESS, and knowing why matters. It spawned a
# SkeletalMeshActor through the MCP bridge and set the mesh and a scale of 20 through
# unreal_set_property, which reported success both times. Reading the actor back showed
# scale 1,1,1 - the writes never landed. A 44-metre Greystone that "did not render" was
# an empty actor all along. Any tool that reports success without being read back is a
# guess wearing a lab coat.
#
# So this one sets through Python, READS EVERY VALUE BACK, and prints what it found.

import json
import traceback

import unreal

MESH = "/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/greystone_render_test.json"
LABEL = "GreystoneRenderTest"

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # Clear any earlier attempt so this never measures a stale actor.
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == LABEL:
            eas.destroy_actor(a)

    mesh = unreal.EditorAssetLibrary.load_asset(MESH)
    r["mesh_loaded"] = mesh is not None
    if not mesh:
        raise Exception("could not load %s" % MESH)

    r["mesh_class"] = str(type(mesh).__name__)
    try:
        r["mesh_skeleton"] = str(mesh.get_editor_property("skeleton").get_name())
    except Exception as e:
        r["mesh_skeleton"] = "unreadable: %s" % e

    # Put him beside the PlayerStart so the editor camera is already looking at him,
    # and big enough that "too small to see" cannot be the answer.
    start = next((a for a in eas.get_all_level_actors()
                  if isinstance(a, unreal.PlayerStart)), None)
    where = start.get_actor_location() if start else unreal.Vector(0, 0, 300)
    where = unreal.Vector(where.x + 300.0, where.y, where.z)

    actor = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, where)
    actor.set_actor_label(LABEL)
    comp = actor.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(mesh)
    actor.set_actor_scale3d(unreal.Vector(5.0, 5.0, 5.0))

    # ---- READ EVERYTHING BACK. The whole point of this script. -------------
    got = comp.get_skeletal_mesh_asset()
    r["assigned_mesh"] = got.get_name() if got else None
    r["assign_worked"] = got is not None
    s = actor.get_actor_scale3d()
    r["scale_readback"] = [s.x, s.y, s.z]
    r["scale_worked"] = abs(s.x - 5.0) < 0.01
    l = actor.get_actor_location()
    r["location"] = [round(l.x, 1), round(l.y, 1), round(l.z, 1)]

    mats = comp.get_editor_property("override_materials")
    r["override_material_count"] = len(mats) if mats else 0
    try:
        r["num_material_slots"] = len(mesh.materials)
        r["material_names"] = [str(m.material_interface.get_name())
                               if m.material_interface else "NULL"
                               for m in mesh.materials][:6]
    except Exception as e:
        r["material_names"] = "unreadable: %s" % e

    r["visible"] = bool(comp.is_visible())
    r["hidden_in_game"] = bool(comp.get_editor_property("hidden_in_game"))

    b = actor.get_actor_bounds(False)
    r["bounds_origin"] = [round(b[0].x, 1), round(b[0].y, 1), round(b[0].z, 1)]
    r["bounds_extent"] = [round(b[1].x, 1), round(b[1].y, 1), round(b[1].z, 1)]
    r["bounds_are_real"] = bool(b[1].z > 1.0)

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###GREYSTONE### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
