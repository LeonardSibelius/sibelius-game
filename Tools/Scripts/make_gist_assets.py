"""THE GIST's one asset: M_GistClay, the matte toy material (docs/GIST_GENERATOR_PLAN.md).
Run in the editor with PIE stopped. Creates the material only if it is missing - never rebuild a
material in place that a loaded C++ class holds (AGistSite names it in its constructor).

Colour comes from per-instance custom data 0-2, so every part of every gist shares one material
and one draw per shape. A faint emissive keeps the toys readable in the keep's darker corners.
"""
import unreal

FOLDER = "/Game/Gist"
NAME = "M_GistClay"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def const(mat, value, x, y):
    n = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", value)
    return n


def clay():
    path = FOLDER + "/" + NAME
    if eal.does_asset_exist(path):
        return "exists"
    mat = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    channels = []
    for i in range(3):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -600, i * 100)
        n.set_editor_property("data_index", i)
        channels.append(n)
    rg = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -400, 0)
    rgb = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -200, 0)
    mel.connect_material_expressions(channels[0], "", rg, "A")
    mel.connect_material_expressions(channels[1], "", rg, "B")
    mel.connect_material_expressions(rg, "", rgb, "A")
    mel.connect_material_expressions(channels[2], "", rgb, "B")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 0, 200)
    mel.connect_material_expressions(rgb, "", glow, "A")
    mel.connect_material_expressions(const(mat, 0.03, -200, 250), "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(const(mat, 0.0, -200, 350), "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(const(mat, 0.65, -200, 450), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(path)
    return "created"


def picture():
    """M_GistPicture: the easel's canvas (THE GIST, illustrated - docs/GIST_PICTURES_PLAN.md).
    Texture parameter "Picture" is the painting the server sent; scalar "Reveal" fades from a blank
    cream canvas (0, the artist is painting) to the painting (1); scalar "Brightness", which
    AGistSite sets (CanvasBrightness), scales the result.

    UNLIT, the painting's own colours as emissive times Brightness. Lit with a little glow added (the
    clay's recipe), the first painting in the keep's bright hall came out washed nearly white, and
    unlit at full strength too; 0.15 matched the picture (2026-11-01). The 2026-11-01 asset was
    turned into this in place, not recreated: never delete its expressions in a running editor -
    AGistSite's constructor holds the material, and delete_all_material_expressions crashed the
    editor on "!IsRooted()"."""
    path = FOLDER + "/M_GistPicture"
    if eal.does_asset_exist(path):
        return "exists"
    mat = tools.create_asset("M_GistPicture", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property("parameter_name", "Picture")
    blank = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if blank:
        tex.set_editor_property("texture", blank)
    cream = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -700, 250)
    cream.set_editor_property("constant", unreal.LinearColor(0.82, 0.76, 0.62, 1.0))
    reveal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 400)
    reveal.set_editor_property("parameter_name", "Reveal")
    reveal.set_editor_property("default_value", 1.0)
    mix = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -400, 100)
    mel.connect_material_expressions(cream, "", mix, "A")
    mel.connect_material_expressions(tex, "RGB", mix, "B")
    mel.connect_material_expressions(reveal, "", mix, "Alpha")
    bright = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 350)
    bright.set_editor_property("parameter_name", "Brightness")
    bright.set_editor_property("default_value", 0.15)
    shown = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 150)
    mel.connect_material_expressions(mix, "", shown, "A")
    mel.connect_material_expressions(bright, "", shown, "B")
    mel.connect_material_property(shown, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    return "created"


print("THE GIST: M_GistClay " + clay() + ", M_GistPicture " + picture() + ".")
