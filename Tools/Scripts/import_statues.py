# import_statues.py - the Archaic Language Temple's five statues, into /Game/Statues.
#
# Walt, 2026-10-19: "yes, convert them in blender and import". build_statues_blender.py made one
# FBX per statue from SMK's public-domain scans (SourceArt/Statues/SOURCES.md), each at its cast's
# real height, standing on its base with the origin at its feet, facing -Y. This imports them as
# Nanite meshes (200,000-400,000 triangles each; the Olympus temple is a Nanite level) and gives
# them M_StatueMarble, which is made here only when missing - never rebuilt in place (see
# make_genesis_assets.py). The scans have no UVs, so the marble is procedural: two warm whites
# mixed by a noise on world position, so neighbouring statues do not repeat each other's grain.
#
# One box of simple collision each, so Leonard walks round a statue, not through it.
#
# Imports and saves assets only. No level is opened, changed or saved.
#
# *** RUN FROM THE OPEN EDITOR, PIE stopped ***

import unreal

SRC = "C:/Users/wpark/projects/sibelius-game/SourceArt/Statues"
FOLDER = "/Game/Statues"
MARBLE = "M_StatueMarble"
TAG = "###STATUES###"

STATUES = [   # folder, asset - the five old languages
    ("FORTRAN_Discobolus", "SM_Statue_FORTRAN_Discobolus"),
    ("COBOL_Doryphoros", "SM_Statue_COBOL_Doryphoros"),
    ("BASIC_Kore", "SM_Statue_BASIC_Kore"),
    ("Java_Zeus", "SM_Statue_Java_Zeus"),
    ("Python_Laocoon", "SM_Statue_Python_Laocoon"),
]

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def say(msg):
    unreal.log("%s %s" % (TAG, msg))
    print("%s %s" % (TAG, msg))


def srgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def marble():
    path = "%s/%s" % (FOLDER, MARBLE)
    if eal.does_asset_exist(path):
        say("%s already there - left as it is" % MARBLE)
        return unreal.load_asset(path)
    mat = tools.create_asset(MARBLE, FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    def const3(hexcol, x, y):
        rgb = srgb(hexcol)
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
        c.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return c

    def scalar(v, x, y):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
        c.set_editor_property("r", v)
        return c

    pos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 0)
    noise = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -650, 0)
    noise.set_editor_property("scale", 0.012)      # grain about a hand across, in cm
    noise.set_editor_property("levels", 4)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    mel.connect_material_expressions(pos, "", noise, "Position")

    warm = const3("#ece6db", -650, -220)     # old Pentelic marble
    cool = const3("#d9d6cf", -650, -120)
    mix = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, -160)
    mel.connect_material_expressions(warm, "", mix, "A")
    mel.connect_material_expressions(cool, "", mix, "B")
    mel.connect_material_expressions(noise, "", mix, "Alpha")
    mel.connect_material_property(mix, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, 80)
    mel.connect_material_expressions(scalar(0.32, -650, 140), "", rough, "A")
    mel.connect_material_expressions(scalar(0.48, -650, 220), "", rough, "B")
    mel.connect_material_expressions(noise, "", rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_asset(path)
    say("%s made (procedural marble, no UVs needed)" % MARBLE)
    return mat


def import_statue(folder, name, mat):
    path = "%s/%s" % (FOLDER, name)
    if eal.does_asset_exist(path):
        say("%s already there - left as it is" % name)
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
    smd.set_editor_property("generate_lightmap_u_vs", False)   # Lumen; and no UVs to lay them from
    try:
        smd.set_editor_property("build_nanite", True)
    except Exception as e:
        say("build_nanite not settable at import (%s) - set after" % e)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", "%s/%s/%s.fbx" % (SRC, folder, name))
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if not mesh:
        say("%s FAILED to import" % name)
        return None

    nanite = mesh.get_editor_property("nanite_settings")
    if not nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", True)
        sme.set_nanite_settings(mesh, nanite, apply_changes=True)
    mesh.set_material(0, mat)
    if sme.get_simple_collision_count(mesh) == 0:
        sme.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
    eal.save_asset(path)

    box = mesh.get_bounding_box()
    size = box.max - box.min
    try:
        verts = sme.get_number_verts(mesh, 0)
    except Exception:
        verts = -1
    say("%s imported: %.0f x %.0f x %.0f cm, Nanite %s, %d verts" % (
        name, size.x, size.y, size.z, mesh.get_editor_property("nanite_settings").get_editor_property("enabled"), verts))
    return mesh


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    say("PIE is running - stop it first; nothing done")
else:
    if not eal.does_directory_exist(FOLDER):
        eal.make_directory(FOLDER)
    mat = marble()
    for folder, name in STATUES:
        import_statue(folder, name, mat)
    say("DONE")
