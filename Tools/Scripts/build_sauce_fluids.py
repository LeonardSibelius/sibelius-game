# build_sauce_fluids.py — v0.9.7.1 experiment 1: sauce is a fluid.
#
# Idempotent. Does three things:
#   1. Creates /Game/Sauce/M_SauceSurface — unlit translucent sauce-green with
#      Color / Glow / Opacity parameters (the temple meniscus + fallback stream).
#   2. Duplicates Niagara Fluids plugin templates into /Game/Sauce so Walt can
#      retint without touching Engine. C++ already hard-refs the plugin originals
#      (they cook either way); BeginPlay prefers these copies when they exist.
#   3. Logs each copy's path so a missing plugin shows up here, not in PIE.
#
# ORDER: NiagaraFluids enabled in the .uproject (already committed) → reopen the
# editor if it was open during the enable → run me.
#
# RUN NATIVELY (editor Cmd box, dropdown = Cmd):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_sauce_fluids.py"
#
# HEADLESS (editor CLOSED):
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript
#     -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_sauce_fluids.py"
#     -unattended -nopause -nosplash -stdout

import unreal

TAG = "###SAUCEFLUID###"
PKG = "/Game/Sauce"

SOURCES = {
    "NS_SauceSimmer": "/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_ColoredSmoke",
    "NS_SaucePour":   "/NiagaraFluids/Templates/Liquid/2D/Systems/Grid2D_FLIP_Hose",
    "NS_SaucePool":   "/NiagaraFluids/Templates/Liquid/2D/Systems/ShallowWater/Grid2D_SW_Pool",
}


def ensure_folder():
    if not unreal.EditorAssetLibrary.does_directory_exist(PKG):
        unreal.EditorAssetLibrary.make_directory(PKG)


def ensure_surface_material():
    dest = PKG + "/M_SauceSurface"
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(dest):
        eal.delete_asset(dest)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    mat = asset_tools.create_asset("M_SauceSurface", PKG, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error("%s could not create M_SauceSurface" % TAG)
        return None

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)

    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, -40)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.07, 0.62, 0.18, 1.0))

    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 220)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 1.8)

    opacity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 420)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.78)

    wave_h = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 620)
    wave_h.set_editor_property("parameter_name", "WaveHeight")
    wave_h.set_editor_property("default_value", 3.2)

    t = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -900, 820)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 980)
    mask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -620, 980)
    mask.set_editor_property("r", True)
    mask.set_editor_property("g", True)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", False)
    mel.connect_material_expressions(wp, "", mask, "")

    scale = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -620, 820)
    scale.set_editor_property("r", 0.07)
    wp_s = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -440, 900)
    mel.connect_material_expressions(mask, "", wp_s, "A")
    mel.connect_material_expressions(scale, "", wp_s, "B")

    t3 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -440, 720)
    k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -620, 720)
    k.set_editor_property("r", 3.0)
    mel.connect_material_expressions(t, "", t3, "A")
    mel.connect_material_expressions(k, "", t3, "B")

    add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -260, 800)
    mel.connect_material_expressions(t3, "", add, "A")
    mel.connect_material_expressions(wp_s, "", add, "B")

    sine = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -80, 800)
    mel.connect_material_expressions(add, "", sine, "")

    z = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 80, 800)
    mel.connect_material_expressions(sine, "", z, "A")
    mel.connect_material_expressions(wave_h, "", z, "B")

    zero = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 80, 640)
    zero.set_editor_property("r", 0.0)
    append = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, 280, 720)
    mel.connect_material_expressions(zero, "", append, "A")
    mel.connect_material_expressions(zero, "", append, "B")
    append2 = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, 480, 720)
    mel.connect_material_expressions(append, "", append2, "A")
    mel.connect_material_expressions(z, "", append2, "B")
    mel.connect_material_property(append2, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -440, 80)
    fres.set_editor_property("exponent", 4.0)
    rim = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 40)
    rim_k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -440, -40)
    rim_k.set_editor_property("r", 0.45)
    mel.connect_material_expressions(fres, "", rim, "A")
    mel.connect_material_expressions(rim_k, "", rim, "B")

    body = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 220)
    mel.connect_material_expressions(color, "", body, "A")
    mel.connect_material_expressions(glow, "", body, "B")

    flicker = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -80, 400)
    t6 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -260, 400)
    k6 = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -440, 400)
    k6.set_editor_property("r", 6.0)
    mel.connect_material_expressions(t, "", t6, "A")
    mel.connect_material_expressions(k6, "", t6, "B")
    mel.connect_material_expressions(t6, "", flicker, "")
    flick_01 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 80, 400)
    half = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -80, 520)
    half.set_editor_property("r", 0.12)
    mel.connect_material_expressions(flicker, "", flick_01, "A")
    mel.connect_material_expressions(half, "", flick_01, "B")
    one = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 80, 520)
    one.set_editor_property("r", 1.0)
    flick_add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, 260, 400)
    mel.connect_material_expressions(one, "", flick_add, "A")
    mel.connect_material_expressions(flick_01, "", flick_add, "B")

    body2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 260, 200)
    mel.connect_material_expressions(body, "", body2, "A")
    mel.connect_material_expressions(flick_add, "", body2, "B")

    emiss = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, 460, 80)
    mel.connect_material_expressions(body2, "", emiss, "A")
    mel.connect_material_expressions(rim, "", emiss, "B")
    mel.connect_material_property(emiss, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(mat)
    eal.save_asset(dest)
    unreal.log("%s M_SauceSurface recreated (waves + fresnel, not a hockey puck)" % TAG)
    return mat


def ensure_bubble_material():
    dest = PKG + "/M_SauceBubble"
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(dest):
        eal.delete_asset(dest)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    mat = asset_tools.create_asset("M_SauceBubble", PKG, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error("%s could not create M_SauceBubble" % TAG)
        return None
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)

    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.55, 1.0, 0.65, 1.0))
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 220)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 3.2)
    opacity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 420)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.28)

    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -400, 80)
    fres.set_editor_property("exponent", 5.0)

    rim = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -180, 40)
    mel.connect_material_expressions(color, "", rim, "A")
    mel.connect_material_expressions(glow, "", rim, "B")
    rim2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 40, 40)
    mel.connect_material_expressions(rim, "", rim2, "A")
    mel.connect_material_expressions(fres, "", rim2, "B")
    mel.connect_material_property(rim2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    op = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 40, 280)
    mel.connect_material_expressions(opacity, "", op, "A")
    addf = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -180, 280)
    k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 280)
    k.set_editor_property("r", 0.35)
    mel.connect_material_expressions(fres, "", addf, "A")
    mel.connect_material_expressions(k, "", addf, "B")
    mel.connect_material_expressions(addf, "", op, "B")
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(mat)
    eal.save_asset(dest)
    unreal.log("%s M_SauceBubble created (fresnel rim, see-through centre)" % TAG)
    return mat


def ensure_duplicate(dest_name, src_path):
    eal = unreal.EditorAssetLibrary
    dest = PKG + "/" + dest_name
    if eal.does_asset_exist(dest):
        unreal.log("%s %s already exists" % (TAG, dest))
        return eal.load_asset(dest)
    if not eal.does_asset_exist(src_path):
        unreal.log_error("%s missing source %s — is NiagaraFluids enabled?" % (TAG, src_path))
        return None
    copied = eal.duplicate_asset(src_path, dest)
    if copied:
        eal.save_asset(dest)
        unreal.log("%s duplicated %s -> %s" % (TAG, src_path, dest))
    else:
        unreal.log_error("%s duplicate FAILED %s -> %s" % (TAG, src_path, dest))
    return copied


def main():
    ensure_folder()
    ensure_surface_material()
    ensure_bubble_material()
    ok = 0
    for name, src in SOURCES.items():
        if ensure_duplicate(name, src):
            ok += 1
    unreal.log("%s done: %d/%d Niagara copies. Ctrl+S if the editor is open." % (TAG, ok, len(SOURCES)))
    if ok < len(SOURCES):
        unreal.log_warning("%s C++ still hard-refs the plugin templates, so PIE works; the copies are for retinting." % TAG)


main()
