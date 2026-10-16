# make_genesis_assets.py - the AI city's materials, and its empty level.
#
# Walt, 2026-10-15: "Let's do Seed 1" - upstairs, a grain of light on a pedestal (AUniverseSeed);
# with all six powers it takes Leonard to the AI city (L_Genesis), a city that is also a mind,
# wrapped round a star, where the AIs grow universes to amuse themselves (AAICity). Everything
# there is built by code from the engine's shapes and these two materials - no purchased asset,
# so it can be rendered and judged by eye, like the basketball court.
#
#   /Game/Genesis/M_GenesisGlow   unlit light: Color x Intensity. The star, the city's lights,
#                                 the grain, the little galaxies.
#   /Game/Genesis/M_GenesisGlass  unlit, translucent, brightest at the rim (Fresnel): the glass
#                                 of the universes in the gallery, the grain's halo.
#   /Game/Maps/L_Genesis          an EMPTY level (the WorldFactory's), made without opening it -
#                                 scripts here never load levels; Walt opens it with File > Open
#                                 Level, and place_genesis.py fills it.
#
# *** RUN FROM THE OPEN EDITOR, any level, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_genesis_assets.py"
# Safe to re-run: it makes only what is missing - a material or the level that exists is left
# alone (rebuilding a material in place crashed the editor once the C++ held it; see
# fresh_material).
#
#   /Game/Genesis/M_GenesisStar   unlit: hotter at the middle, darker and redder at the limb,
#                                 mottled. The star (AAICity::Paint loads it by path; cooked by
#                                 DefaultGame.ini's /Game/Genesis line).

import unreal

TAG = "###GENESISASSETS###"
FOLDER = "/Game/Genesis"
LEVEL = "/Game/Maps/L_Genesis"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def say(text):
    print("%s %s" % (TAG, text))


class Exists(Exception):
    pass


def fresh_material(name):
    # NEVER REBUILT IN PLACE (2026-10-15): once AUniverseSeed and AAICity are built, their
    # constructors hold these materials, and deleting the expressions of one the editor has
    # loaded that way CRASHED the editor (Assertion failed: !IsRooted(), in the material
    # editor). So an existing material is left alone; to change one, close the editor, delete
    # the .uasset, reopen, and run this again.
    path = "%s/%s" % (FOLDER, name)
    if eal.does_asset_exist(path):
        raise Exists(path)
    mat = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    return path, mat


def param_vector(mat, name, value, x, y):
    p = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


def param_scalar(mat, name, value, x, y):
    p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


def finish(path, mat):
    mel.recompile_material(mat)
    eal.save_asset(path)
    say("material %s: vectors %s, scalars %s" % (path, list(mel.get_vector_parameter_names(mat)),
                                              list(mel.get_scalar_parameter_names(mat))))


def build_glow():
    path, mat = fresh_material("M_GenesisGlow")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = param_vector(mat, "Color", unreal.LinearColor(1.0, 0.85, 0.55, 1.0), -600, -100)
    power = param_scalar(mat, "Intensity", 10.0, -600, 100)
    lit = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 0)
    mel.connect_material_expressions(color, "", lit, "A")
    mel.connect_material_expressions(power, "", lit, "B")
    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(path, mat)


def build_glass():
    path, mat = fresh_material("M_GenesisGlass")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    color = param_vector(mat, "Color", unreal.LinearColor(0.55, 0.75, 1.0, 1.0), -900, -250)
    power = param_scalar(mat, "Intensity", 2.0, -900, -50)
    exponent = param_scalar(mat, "Exponent", 3.0, -900, 150)
    rim_alpha = param_scalar(mat, "RimOpacity", 0.6, -900, 300)
    base_alpha = param_scalar(mat, "BaseOpacity", 0.04, -900, 450)
    fresnel = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -600, 150)
    mel.connect_material_expressions(exponent, "", fresnel, "ExponentIn")
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, -150)
    mel.connect_material_expressions(color, "", tint, "A")
    mel.connect_material_expressions(power, "", tint, "B")
    rim = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(tint, "", rim, "A")
    mel.connect_material_expressions(fresnel, "", rim, "B")
    mel.connect_material_property(rim, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    alpha = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 350)
    mel.connect_material_expressions(fresnel, "", alpha, "A")
    mel.connect_material_expressions(rim_alpha, "", alpha, "B")
    total = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -300, 350)
    mel.connect_material_expressions(alpha, "", total, "A")
    mel.connect_material_expressions(base_alpha, "", total, "B")
    mel.connect_material_property(total, "", unreal.MaterialProperty.MP_OPACITY)
    finish(path, mat)


def build_star():
    # The star: hotter at the middle, darker and redder at the limb (as a real one is, and what
    # makes a disc read as a ball), and mottled - procedural noise over its surface. The first
    # render (2026-10-15) was a flat white disc, a lamp.
    path, mat = fresh_material("M_GenesisStar")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    core = param_vector(mat, "Core", unreal.LinearColor(1.0, 0.82, 0.5, 1.0), -1100, -300)
    limb = param_vector(mat, "Limb", unreal.LinearColor(1.0, 0.35, 0.08, 1.0), -1100, -100)
    power = param_scalar(mat, "Intensity", 3.0, -1100, 100)
    exponent = param_scalar(mat, "LimbExponent", 1.6, -1100, 250)
    fresnel = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -800, 200)
    mel.connect_material_expressions(exponent, "", fresnel, "ExponentIn")
    colour = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -800, -200)
    mel.connect_material_expressions(core, "", colour, "A")
    mel.connect_material_expressions(limb, "", colour, "B")
    mel.connect_material_expressions(fresnel, "", colour, "Alpha")
    # Dimmer toward the limb, too.
    dim = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -600, 200)
    mel.connect_material_expressions(fresnel, "", dim, "")
    shade = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 200)
    shade.set_editor_property("const_b", 0.75)
    mel.connect_material_expressions(dim, "", shade, "A")
    floor = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -300, 200)
    floor.set_editor_property("const_b", 0.25)
    mel.connect_material_expressions(shade, "", floor, "A")
    # Granulation: cells a few metres across on a 160 m star.
    noise = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -800, 450)
    noise.set_editor_property("scale", 0.004)
    noise.set_editor_property("levels", 4)
    noise.set_editor_property("output_min", 0.7)
    noise.set_editor_property("output_max", 1.15)
    lit = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, -100)
    mel.connect_material_expressions(colour, "", lit, "A")
    mel.connect_material_expressions(power, "", lit, "B")
    lit2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -50)
    mel.connect_material_expressions(lit, "", lit2, "A")
    mel.connect_material_expressions(floor, "", lit2, "B")
    lit3 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 50)
    mel.connect_material_expressions(lit2, "", lit3, "A")
    mel.connect_material_expressions(noise, "", lit3, "B")
    mel.connect_material_property(lit3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(path, mat)


def make_level():
    if eal.does_asset_exist(LEVEL):
        say("level %s already exists - left as it is" % LEVEL)
        return
    world = tools.create_asset("L_Genesis", "/Game/Maps", unreal.World, unreal.WorldFactory())
    eal.save_asset(LEVEL)
    say("level %s made (empty): %s" % (LEVEL, world.get_path_name() if world else "FAILED"))


try:
    for build in (build_glow, build_glass, build_star):
        try:
            build()
        except Exists as there:
            say("material %s already exists - left as it is" % there)
    make_level()
    say("DONE")
except Exception as error:
    say("FAILED: %s" % error)
