# make_go_assets.py — the Go table's assets: /Game/Go/T_GoWood and /Game/Go/M_GoSurface, and the
# living room's GO door sign, /Game/Go/T_Sign_Go.
#
# *** RUN FROM THE OPEN EDITOR, any level, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_go_assets.py"
#
# Draw the grain first:  python Tools/Scripts/make_go_wood.py   (SourceArt/Go/T_GoWood.png)
# and the sign:          python Tools/Scripts/make_go_sign.py   (SourceArt/Go/T_Sign_Go.png)
# Safe to re-run: re-imports the picture over the texture and rebuilds the material in place.
#
# M_GoSurface is the one material on the board, the lines and the stones (AGoTable makes a
# dynamic instance of it for each): BaseColor = Color x lerp(1, Pattern, PatternAmount), and
# Roughness. The wood uses the grain (PatternAmount 1); lines and stones are plain colour
# (PatternAmount 0). The parameter names are the ones AGoTable sets - and Color is also the
# engine's BasicShapeMaterial's, which is what the table falls back to without this.

import os
import unreal

TAG = "###GOASSETS###"
FOLDER = "/Game/Go"
TEX_NAME = "T_GoWood"
MAT_NAME = "M_GoSurface"
SIGN_NAME = "T_Sign_Go"
ART = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Go")
PNG = os.path.join(ART, "T_GoWood.png")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def say(text):
    print("%s %s" % (TAG, text))


def import_texture(png=PNG, name=TEX_NAME):
    if not os.path.exists(png):
        raise RuntimeError("no %s - run its make_go_*.py first" % png)
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = FOLDER
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    tex = eal.load_asset("%s/%s" % (FOLDER, name))
    if not tex:
        raise RuntimeError("%s did not import" % name)
    tex.set_editor_property("srgb", True)
    if name == SIGN_NAME:
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)   # as T_Sign_Chess
    eal.save_asset(tex.get_path_name())
    say("texture %s %dx%d" % (tex.get_path_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
    return tex


def build_material(tex):
    path = "%s/%s" % (FOLDER, MAT_NAME)
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(MAT_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    pattern = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, -100)
    pattern.set_editor_property("parameter_name", "Pattern")
    pattern.set_editor_property("texture", tex)

    amount = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 150)
    amount.set_editor_property("parameter_name", "PatternAmount")
    amount.set_editor_property("default_value", 1.0)

    blend = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -600, 0)
    blend.set_editor_property("const_a", 1.0)
    mel.connect_material_expressions(pattern, "RGB", blend, "B")
    mel.connect_material_expressions(amount, "", blend, "Alpha")

    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, -250)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.78, 0.55, 0.27, 1.0))

    tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(color, "", tint, "A")
    mel.connect_material_expressions(blend, "", tint, "B")
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -300, 200)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.5)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_asset(path)
    say("material %s: vectors %s, scalars %s, textures %s" % (
        path, list(mel.get_vector_parameter_names(mat)), list(mel.get_scalar_parameter_names(mat)),
        list(mel.get_texture_parameter_names(mat))))
    return mat


def check_fallback():
    basic = eal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
    say("fallback BasicShapeMaterial vectors: %s" % list(mel.get_vector_parameter_names(basic)))


try:
    texture = import_texture()
    build_material(texture)
    import_texture(os.path.join(ART, "T_Sign_Go.png"), SIGN_NAME)
    check_fallback()
    say("DONE")
except Exception as error:
    say("FAILED: %s" % error)
