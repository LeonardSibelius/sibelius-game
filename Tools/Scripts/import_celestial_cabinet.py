# import_celestial_cabinet.py - bring the Blender cabinet in and stand it at the cathedral apse.
#
# Walt, 2026-10-16: "ok, build the custom cabinet in blender". build_cabinet_blender.py made
# SourceArt/SlotCabinet/SM_CelestialCabinet.fbx (one mesh, nine material slots) and
# make_cabinet_art.mjs its three painted panels. This imports them into /Game/SlotFactory/Cabinet,
# makes the materials the slots ask for, and - when L_Cathedral is the open level - puts the mesh
# on the ASlotCabinet in place of the stretched engine cube, standing on the floor, and removes
# the cube-era sign (the SlotCab_Marquee* actors build_cabinet_marquee.py made; the cabinet now
# carries its own marquee). The level is NOT saved: Walt looks, then saves it himself.
#
# Materials, made only when missing (never rebuilt in place - see make_genesis_assets.py):
#   lit:   M_CabLacquer (midnight body), M_CabGlass (the deck), M_CabBadgeBrass (Walt's plate),
#          M_CabBrass (bezels and trim) unless M_SlotGold is already a lit metal to reuse
#   unlit: M_CabScreenLit, M_CabMarqueeLit (the spec: the brightest thing in the apse),
#          M_CabPipe, M_CabButtonLit, M_CabSpinLit
#
# *** RUN FROM THE OPEN EDITOR, PIE stopped (L_Cathedral open to place it; any level to import) ***

import os

import unreal

SRC = "C:/Users/wpark/projects/sibelius-game/SourceArt/SlotCabinet"
FOLDER = "/Game/SlotFactory/Cabinet"
MESH = "SM_CelestialCabinet"
FLOOR_Z = 0.0                       # the apse floor (build_cabinet_marquee.py measured it)
TAG = "###CABINET###"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
notes = []


def say(msg):
    notes.append(msg)
    unreal.log("%s %s" % (TAG, msg))


def srgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def import_texture(name):
    path = "%s/%s" % (FOLDER, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", "%s/%s.png" % (SRC, name))
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    say("texture %s %s" % (name, "imported" if tex else "FAILED"))
    return tex


def import_mesh():
    path = "%s/%s" % (FOLDER, MESH)
    if eal.does_asset_exist(path):
        say("mesh %s already there - left as it is" % MESH)
        return unreal.load_asset(path)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    smd = ui.get_editor_property("static_mesh_import_data")
    smd.set_editor_property("combine_meshes", True)
    smd.set_editor_property("auto_generate_collision", False)
    smd.set_editor_property("generate_lightmap_u_vs", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", "%s/%s.fbx" % (SRC, MESH))
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", MESH)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if not mesh:
        raise RuntimeError("the cabinet mesh did not import")
    # One box round the whole machine: the interactor's focus trace has to land on it (SC9).
    sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if sme.get_simple_collision_count(mesh) == 0:
        sme.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    eal.save_asset(path)
    say("mesh %s imported (%d material slots)" % (MESH, len(mesh.get_editor_property("static_materials"))))
    return mesh


def new_material(name):
    path = "%s/%s" % (FOLDER, name)
    if eal.does_asset_exist(path):
        return None, unreal.load_asset(path)
    return path, tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def const3(mat, rgb, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    c.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return c


def scalar(mat, v, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v)
    return c


def finish(path, mat, what):
    if path:
        mel.recompile_material(mat)
        eal.save_asset(path)
        say("%s made (%s)" % (path.split("/")[-1], what))
    return mat


def lit(name, hexcol, metallic, roughness, texture=None):
    path, mat = new_material(name)
    if path:
        if texture:
            t = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, 0)
            t.set_editor_property("texture", texture)
            mel.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            mel.connect_material_property(const3(mat, srgb(hexcol), -420, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(scalar(mat, metallic, -420, 160), "", unreal.MaterialProperty.MP_METALLIC)
        mel.connect_material_property(scalar(mat, roughness, -420, 240), "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(path, mat, "lit")


def unlit(name, strength, hexcol=None, texture=None):
    # Unlit: a backlit panel must not take the apse's light (build_cabinet_marquee.py, note 1).
    path, mat = new_material(name)
    if path:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        if texture:
            src = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 0)
            src.set_editor_property("texture", texture)
            out = "RGB"
        else:
            src, out = const3(mat, srgb(hexcol), -600, 0), ""
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 0)
        mel.connect_material_expressions(src, out, mul, "A")
        mel.connect_material_expressions(scalar(mat, strength, -600, 200), "", mul, "B")
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(path, mat, "unlit x %.1f" % strength)


def brass():
    gold = unreal.load_asset("/Game/SlotFactory/M_SlotGold")
    if gold and isinstance(gold, unreal.Material) and \
            gold.get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_DEFAULT_LIT:
        say("brass = M_SlotGold (lit)")
        return gold
    return lit("M_CabBrass", "#D4A836", 1.0, 0.35)


def materials():
    screen, marquee, badge = (import_texture(n) for n in ("T_CabScreen", "T_Marquee_CelestialFortune", "T_CabBadge"))
    return {
        "M_CabBody": lit("M_CabLacquer", "#12142C", 0.55, 0.32),
        "M_CabBrass": brass(),
        "M_CabBlack": lit("M_CabGlass", "#050508", 0.0, 0.12),
        "M_CabGlow": unlit("M_CabPipe", 4.0, hexcol="#C7EEFF"),
        "M_CabButton": unlit("M_CabButtonLit", 2.5, hexcol="#FFD24D"),
        "M_CabSpin": unlit("M_CabSpinLit", 3.0, hexcol="#E8320E"),
        "M_CabScreen": unlit("M_CabScreenLit", 1.6, texture=screen),
        "M_CabMarquee": unlit("M_CabMarqueeLit", 6.0, texture=marquee),
        "M_CabBadge": lit("M_CabBadgeBrass", None, 1.0, 0.35, texture=badge),
    }


def assign(mesh, mats):
    for i, slot in enumerate(mesh.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        if name in mats and mats[name]:
            mesh.set_material(i, mats[name])
        else:
            say("slot %d '%s' has no material to give it" % (i, name))
    eal.save_asset("%s/%s" % (FOLDER, MESH))


def place(mesh):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_name() != "L_Cathedral":
        say("open level is %s, not L_Cathedral - imported only, nothing placed"
            % (world.get_name() if world else "none"))
        return
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cabinet = None
    for a in list(eas.get_all_level_actors()):
        if a.get_class().get_name() == "SlotCabinet":
            cabinet = a
    if not cabinet:
        say("no ASlotCabinet in L_Cathedral - nothing placed")
        return
    for a in list(eas.get_all_level_actors()):
        if a.get_actor_label().startswith("SlotCab_Marquee"):
            say("removed %s (the cube's sign; the cabinet has its own marquee)" % a.get_actor_label())
            eas.destroy_actor(a)
    comp = cabinet.get_editor_property("cabinet_mesh")
    comp.set_static_mesh(mesh)
    comp.set_editor_property("override_materials", [])
    comp.set_editor_property("relative_scale3d", unreal.Vector(1, 1, 1))
    comp.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
    # Built with its front along +X. The actor faces down the nave at yaw 180, so local +X is
    # world -X, toward the entrance. If the import turned it round, turn the mesh back.
    box = mesh.get_bounding_box()
    front_plus_x = box.max.x > -box.min.x
    comp.set_editor_property("relative_rotation", unreal.Rotator(roll=0, pitch=0, yaw=0 if front_plus_x else 180))
    loc = cabinet.get_actor_location()
    cabinet.set_actor_location(unreal.Vector(loc.x, loc.y, FLOOR_Z), False, True)
    say("placed on %s at (%.0f, %.0f, %.0f), yaw %.0f; mesh bounds x %.0f..%.0f, z %.0f..%.0f%s"
        % (cabinet.get_actor_label(), loc.x, loc.y, FLOOR_Z, cabinet.get_actor_rotation().yaw,
           box.min.x, box.max.x, box.min.z, box.max.z, "" if front_plus_x else " (turned round)"))
    say("L_Cathedral NOT saved - Walt saves it (Ctrl+S) once it looks right")


try:
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("REFUSING: stop Play first - the editor's asset functions do not work during PIE.")
    mesh = import_mesh()
    assign(mesh, materials())
    place(mesh)
except Exception as ex:
    say("FAILED: %s" % ex)
unreal.log("%s DONE\n%s" % (TAG, "\n".join(notes)))
