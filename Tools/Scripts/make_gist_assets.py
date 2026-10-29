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


print("THE GIST: M_GistClay " + clay() + ".")
