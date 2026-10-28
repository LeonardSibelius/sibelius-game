# make_lantern_assets.py - the orb material for Lantern Burst (ALanternBurst, in Kaia's jungle).
#
# Walt, 2026-10-27: "make a new game that is addictive and attractive". Every orb, spark, guide
# dot and ember of the lantern is an instance of the engine's sphere in ONE material, coloured and
# lit per instance by its custom data (ALanternBurst writes four floats an instance):
#
#   /Game/LanternBurst/M_LanternOrb   lit glass: base colour = custom data 0-2, darkened, so the
#                                     sun still puts a highlight on it; glossy (roughness 0.12);
#                                     emissive = colour x glow (custom data 3) x (a soft core plus
#                                     a bright Fresnel rim) - a lit glass ball, not a flat disc.
#                                     used_with_instanced_static_meshes is set and SAVED (the
#                                     2026-10-18 lesson: unset, a cooked build draws it grey).
#
# Built from engine nodes only - no purchased asset - so it can be rendered and judged by eye.
#
# *** RUN FROM THE OPEN EDITOR, any level, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_lantern_assets.py"
# Safe to re-run: it makes only what is missing (make_genesis_assets.py's rule - rebuilding a
# material in place that a loaded C++ class holds crashed the editor). To change it, close the
# editor, delete the .uasset, reopen, run this again.

import unreal

TAG = "###LANTERNASSETS###"
FOLDER = "/Game/LanternBurst"
NAME = "M_LanternOrb"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def say(text):
    print("%s %s" % (TAG, text))


def node(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def constant(mat, value, x, y):
    c = node(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", value)
    return c


def binary(mat, cls, a, b, x, y):
    n = node(mat, cls, x, y)
    mel.connect_material_expressions(a, "", n, "A")
    mel.connect_material_expressions(b, "", n, "B")
    return n


def build_orb():
    path = "%s/%s" % (FOLDER, NAME)
    if eal.does_asset_exist(path):
        say("%s exists - left alone." % path)
        return
    mat = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    # The instance's colour (custom data 0-2) and glow (3).
    channels = []
    for i in range(4):
        cd = node(mat, unreal.MaterialExpressionPerInstanceCustomData, -1400, -300 + 160 * i)
        cd.set_editor_property("data_index", i)
        channels.append(cd)
    rg = node(mat, unreal.MaterialExpressionAppendVector, -1150, -250)
    mel.connect_material_expressions(channels[0], "", rg, "A")
    mel.connect_material_expressions(channels[1], "", rg, "B")
    rgb = node(mat, unreal.MaterialExpressionAppendVector, -950, -200)
    mel.connect_material_expressions(rg, "", rgb, "A")
    mel.connect_material_expressions(channels[2], "", rgb, "B")
    glow = channels[3]

    # Base colour: the colour, darkened, so the light on it reads as glass and not paint.
    base = binary(mat, unreal.MaterialExpressionMultiply, rgb, constant(mat, 0.22, -950, -60), -650, -260)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(constant(mat, 0.12, -650, -120), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(constant(mat, 0.9, -650, -40), "", unreal.MaterialProperty.MP_SPECULAR)

    # Emissive: colour x glow x (0.55 + 2.2 x Fresnel) - lit through, brightest at the rim.
    fresnel = node(mat, unreal.MaterialExpressionFresnel, -1150, 200)
    fresnel.set_editor_property("exponent", 2.5)
    fresnel.set_editor_property("base_reflect_fraction", 0.0)
    rim = binary(mat, unreal.MaterialExpressionMultiply, fresnel, constant(mat, 2.2, -1150, 330), -900, 220)
    shell = binary(mat, unreal.MaterialExpressionAdd, rim, constant(mat, 0.55, -900, 340), -700, 240)
    lit = binary(mat, unreal.MaterialExpressionMultiply, rgb, glow, -700, 60)
    emissive = binary(mat, unreal.MaterialExpressionMultiply, lit, shell, -450, 120)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_asset(path)
    say("made %s (instanced: %s)." % (path, mat.get_editor_property("used_with_instanced_static_meshes")))


build_orb()
