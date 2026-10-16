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
#                                 mottled. The star's first skin; M_GenesisSun replaced it.
#   /Game/Genesis/T_Sun           Celestial Fortune's own sun (make_sun_texture.mjs renders it
#                                 from SourceArt/Genesis/sun.svg, the slot game's original art).
#   /Game/Genesis/M_GenesisSunDisc  unlit, translucent: T_Sun, alive - a slow turn, a heat
#                                 shimmer, a breathing glow, the surface brightening and dimming
#                                 in patches. The star's skin until the layered one is made.
#   /Game/Genesis/T_SunCorona, T_SunLoopNE/SE/SW/N, T_SunDisc, T_SunSpots, T_SunFlare
#                                 the same sun in layers (make_sun_texture.mjs).
#   /Game/Genesis/M_GenesisSunLive  unlit, translucent: the layers stacked back, each moving on
#                                 its own - the corona breathing, every loop rising and falling
#                                 and flickering, the disc shimmering, the spots drifting
#                                 across, the flare bursting. Worn by the star (AAICity::Paint
#                                 loads it by path; cooked by DefaultGame.ini's /Game/Genesis).

import os
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


def import_sun_texture():
    # Celestial Fortune's sun, rendered by make_sun_texture.mjs. Imported only if missing, like
    # the materials; to refresh it, delete /Game/Genesis/T_Sun with the editor closed.
    path = "%s/T_Sun" % FOLDER
    if eal.does_asset_exist(path):
        say("texture %s already exists - left as it is" % path)
        return eal.load_asset(path)
    png = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Genesis", "T_Sun.png")
    if not os.path.exists(png):
        raise RuntimeError("no %s - run make_sun_texture.mjs first" % png)
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = FOLDER
    task.destination_name = "T_Sun"
    task.replace_existing = False
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    tex = eal.load_asset(path)
    if not tex:
        raise RuntimeError("T_Sun did not import")
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    eal.save_asset(path)
    say("texture %s %dx%d" % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
    return tex


def build_sun_disc():
    # THE SUN OF CELESTIAL FORTUNE, ALIVE (Walt, 2026-10-15: "go ahead with the small version,
    # I am really excited to see this"). The slot's sun on a flat disc facing the gallery, and
    # everything that moves it done here, on the one picture:
    #   a slow turn          - TexCoord through a Rotator, Time x TurnSpeed
    #   a heat shimmer       - two drifting noises nudge where the picture is read (Boil)
    #   a breathing glow     - brightness up and down on a slow sine (Breathe)
    #   the surface boiling  - patches brighten and dim, a slow third noise (Flicker)
    tex = import_sun_texture()
    path, mat = fresh_material("M_GenesisSunDisc")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    power = param_scalar(mat, "Intensity", 1.6, -1500, -500)
    turn = param_scalar(mat, "TurnSpeed", 0.004, -1500, -350)
    boil = param_scalar(mat, "Boil", 0.0035, -1500, -200)
    breathe = param_scalar(mat, "Breathe", 0.08, -1500, -50)
    flicker = param_scalar(mat, "Flicker", 0.12, -1500, 100)

    time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1500, 300)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 450)

    # The turn.
    spin_time = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1250, -350)
    mel.connect_material_expressions(time, "", spin_time, "A")
    mel.connect_material_expressions(turn, "", spin_time, "B")
    rot = mel.create_material_expression(mat, unreal.MaterialExpressionRotator, -1050, -300)
    rot.set_editor_property("center_x", 0.5)
    rot.set_editor_property("center_y", 0.5)
    rot.set_editor_property("speed", 1.0)
    mel.connect_material_expressions(uv, "", rot, "Coordinate")
    mel.connect_material_expressions(spin_time, "", rot, "Time")

    # The shimmer: slow 3D noises over (the picture, time), a nudge to where it is read.
    def drifting_noise(scale_uv, scale_t, offset, lo, hi, x, y):
        k = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
        k.set_editor_property("const_b", scale_uv)
        mel.connect_material_expressions(uv, "", k, "A")
        shift = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, x + 150, y)
        shift.set_editor_property("const_b", offset)
        mel.connect_material_expressions(k, "", shift, "A")
        t = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y + 120)
        t.set_editor_property("const_b", scale_t)
        mel.connect_material_expressions(time, "", t, "A")
        pos = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x + 300, y + 60)
        mel.connect_material_expressions(shift, "", pos, "A")
        mel.connect_material_expressions(t, "", pos, "B")
        n = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, x + 450, y + 60)
        n.set_editor_property("scale", 1.0)
        n.set_editor_property("levels", 2)
        n.set_editor_property("output_min", lo)
        n.set_editor_property("output_max", hi)
        n.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
        mel.connect_material_expressions(pos, "", n, "Position")
        return n

    n1 = drifting_noise(28.0, 0.35, 0.0, -1.0, 1.0, -1300, 550)
    n2 = drifting_noise(28.0, 0.35, 7.3, -1.0, 1.0, -1300, 800)
    nudge = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -650, 650)
    mel.connect_material_expressions(n1, "", nudge, "A")
    mel.connect_material_expressions(n2, "", nudge, "B")
    nudge_k = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, 650)
    mel.connect_material_expressions(nudge, "", nudge_k, "A")
    mel.connect_material_expressions(boil, "", nudge_k, "B")
    read_at = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -850, -150)
    mel.connect_material_expressions(rot, "", read_at, "A")
    mel.connect_material_expressions(nudge_k, "", read_at, "B")

    sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -650, -150)
    sample.set_editor_property("parameter_name", "Sun")
    sample.set_editor_property("texture", tex)
    mel.connect_material_expressions(read_at, "", sample, "UVs")

    # The breath: 1 + Breathe x sin(0.8 t). The Sine node's period is in time units.
    sine = mel.create_material_expression(mat, unreal.MaterialExpressionSine, -1050, 50)
    sine.set_editor_property("period", 7.85)
    mel.connect_material_expressions(time, "", sine, "")
    breath = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -850, 50)
    mel.connect_material_expressions(sine, "", breath, "A")
    mel.connect_material_expressions(breathe, "", breath, "B")
    breath1 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -700, 50)
    breath1.set_editor_property("const_b", 1.0)
    mel.connect_material_expressions(breath, "", breath1, "A")

    # The boiling surface: a slow, broad noise brightening and dimming it in patches.
    n3 = drifting_noise(4.0, 0.12, 3.1, -1.0, 1.0, -1300, 1050)
    patch = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 1050)
    mel.connect_material_expressions(n3, "", patch, "A")
    mel.connect_material_expressions(flicker, "", patch, "B")
    patch1 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -500, 1050)
    patch1.set_editor_property("const_b", 1.0)
    mel.connect_material_expressions(patch, "", patch1, "A")

    lit = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -100)
    mel.connect_material_expressions(sample, "RGB", lit, "A")
    mel.connect_material_expressions(power, "", lit, "B")
    lit2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 0)
    mel.connect_material_expressions(lit, "", lit2, "A")
    mel.connect_material_expressions(breath1, "", lit2, "B")
    lit3 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 100)
    mel.connect_material_expressions(lit2, "", lit3, "A")
    mel.connect_material_expressions(patch1, "", lit3, "B")
    mel.connect_material_property(lit3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY)
    finish(path, mat)


def import_sun_layer(name, max_size):
    # One of make_sun_texture.mjs's layers. Imported only if missing; the small ones capped at
    # max_size, since a loop or a spot does not need the disc's 2048.
    path = "%s/%s" % (FOLDER, name)
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    png = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Genesis", name + ".png")
    if not os.path.exists(png):
        raise RuntimeError("no %s - run make_sun_texture.mjs first" % png)
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = FOLDER
    task.destination_name = name
    task.replace_existing = False
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    tex = eal.load_asset(path)
    if not tex:
        raise RuntimeError("%s did not import" % name)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    if max_size < 2048:
        tex.set_editor_property("max_texture_size", max_size)
    eal.save_asset(path)
    say("texture %s" % path)
    return tex


def build_sun_live():
    # THE SUN, EACH PART MOVING ON ITS OWN (Walt, 2026-10-15: "let's do the medium please").
    # make_sun_texture.mjs's layers stacked back, back to front, each with its own motion:
    #   the corona     breathes - a little larger and brighter, and back (CoronaBreathe)
    #   four loops     each rises and falls from its own foot at its own pace (LoopRise), and
    #                  flickers (LoopFlicker)
    #   the disc       shimmers (Boil), breathes (Breathe), boils in patches (Flicker)
    #   the sunspots   drift across the face, off one edge and back round the other (SpotDrift,
    #                  in widths of the picture a second), kept to the disc
    #   the flare      bursts every few seconds (FlareBurst)
    # Stacked with the "over" operator on premultiplied colour; the result's colour is divided
    # back out of its coverage for the translucent blend, which multiplies by opacity itself.
    layers = {
        "T_SunCorona": import_sun_layer("T_SunCorona", 1024),
        "T_SunDisc": import_sun_layer("T_SunDisc", 2048),
        "T_SunSpots": import_sun_layer("T_SunSpots", 1024),
        "T_SunFlare": import_sun_layer("T_SunFlare", 1024),
    }
    # The loops, with the middle of their two feet on the limb (the SVG's own coordinates / 200),
    # how fast each rises and falls (radians a second), and where in that it starts.
    loops = [("T_SunLoopNE", 145, 61, 0.35, 0.0), ("T_SunLoopSE", 150, 131, 0.28, 1.7),
             ("T_SunLoopSW", 50, 131, 0.42, 3.1), ("T_SunLoopN", 106, 44, 0.31, 4.4)]
    for name, _, _, _, _ in loops:
        layers[name] = import_sun_layer(name, 1024)

    path, mat = fresh_material("M_GenesisSunLive")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    cursor = [0]

    def node(cls):
        cursor[0] += 1
        return mel.create_material_expression(mat, cls, -2400 + (cursor[0] % 12) * 180, -1200 + (cursor[0] // 12) * 140)

    def link(src, dst, inp):
        # src: (expression, output name) or a number (a Constant).
        if isinstance(src, (int, float)):
            k = node(unreal.MaterialExpressionConstant)
            k.set_editor_property("r", float(src))
            src = (k, "")
        mel.connect_material_expressions(src[0], src[1], dst, inp)

    def op(cls, a, b):
        n = node(cls)
        link(a, n, "A")
        if isinstance(b, (int, float)):
            n.set_editor_property("const_b", float(b))
        else:
            link(b, n, "B")
        return (n, "")

    def mul(a, b): return op(unreal.MaterialExpressionMultiply, a, b)
    def add(a, b): return op(unreal.MaterialExpressionAdd, a, b)
    def sub(a, b): return op(unreal.MaterialExpressionSubtract, a, b)
    def div(a, b): return op(unreal.MaterialExpressionDivide, a, b)

    def one(cls, a):
        n = node(cls)
        link(a, n, "")
        return (n, "")

    def vec2(u, v):
        n = node(unreal.MaterialExpressionConstant2Vector)
        n.set_editor_property("r", u)
        n.set_editor_property("g", v)
        return (n, "")

    def append(a, b):
        n = node(unreal.MaterialExpressionAppendVector)
        link(a, n, "A")
        link(b, n, "B")
        return (n, "")

    def param(name, value):
        n = node(unreal.MaterialExpressionScalarParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", value)
        return (n, "")

    def mask(a, channel):
        n = node(unreal.MaterialExpressionComponentMask)
        for c in "rgba":
            n.set_editor_property(c, c == channel)
        link(a, n, "")
        return (n, "")

    time = (node(unreal.MaterialExpressionTime), "")
    uv = (node(unreal.MaterialExpressionTextureCoordinate), "")

    def wave(omega, phase):
        # sin(omega t + phase): the Sine node's period is in time units.
        s = node(unreal.MaterialExpressionSine)
        s.set_editor_property("period", 6.283185307 / omega)
        link(add(time, phase / omega), s, "")
        return (s, "")

    def scale_about(pu, pv, s):
        # Read the picture as if it were s times larger about (pu, pv).
        p = vec2(pu, pv)
        return add(div(sub(uv, p), s), p)

    def sample(name, at):
        n = node(unreal.MaterialExpressionTextureSampleParameter2D)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("texture", layers[name])
        link(at, n, "UVs")
        return n

    def noise(at3, lo, hi):
        n = node(unreal.MaterialExpressionNoise)
        n.set_editor_property("scale", 1.0)
        n.set_editor_property("levels", 2)
        n.set_editor_property("output_min", lo)
        n.set_editor_property("output_max", hi)
        n.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
        link(at3, n, "Position")
        return (n, "")

    power = param("Intensity", 1.6)
    corona_breathe = param("CoronaBreathe", 0.03)
    loop_rise = param("LoopRise", 0.12)
    loop_flicker = param("LoopFlicker", 0.25)
    boil = param("Boil", 0.0035)
    breathe = param("Breathe", 0.08)
    flicker = param("Flicker", 0.12)
    drift = param("SpotDrift", 0.004)
    burst = param("FlareBurst", 0.8)

    # The stack so far: premultiplied colour P and coverage A.
    stack = {"P": None, "A": None}

    def over(rgb, a):
        if stack["P"] is None:
            stack["P"], stack["A"] = mul(rgb, a), a
            return
        inv = one(unreal.MaterialExpressionOneMinus, a)
        stack["P"] = add(mul(rgb, a), mul(stack["P"], inv))
        stack["A"] = add(a, mul(stack["A"], inv))

    # The corona, breathing.
    corona = sample("T_SunCorona", scale_about(0.5, 0.5, add(mul(wave(0.5, 0.0), corona_breathe), 1.0)))
    over(mul((corona, "RGB"), add(mul(wave(0.5, 1.0), 0.15), 1.0)), (corona, "A"))

    # The loops, each rising and falling from its feet, and flickering.
    for name, pu, pv, omega, phase in loops:
        lift = add(mul(wave(omega, phase), loop_rise), 1.0)
        loop = sample(name, scale_about(pu / 200.0, pv / 200.0, lift))
        flick = add(mul(add(mul(wave(2.3, phase * 2.0), 0.6), mul(wave(5.1, phase * 3.0), 0.4)), loop_flicker), 1.0)
        over(mul((loop, "RGB"), flick), (loop, "A"))

    # The disc: shimmering, breathing, boiling in patches.
    def drifting(scale_uv, scale_t, offset):
        return noise(append(add(mul(uv, scale_uv), offset), mul(time, scale_t)), -1.0, 1.0)

    nudge = mul(append(drifting(28.0, 0.35, 0.0), drifting(28.0, 0.35, 7.3)), boil)
    disc_at = add(uv, nudge)
    disc = sample("T_SunDisc", disc_at)
    glow = mul(add(mul(wave(0.8, 0.0), breathe), 1.0), add(mul(drifting(4.0, 0.12, 3.1), flicker), 1.0))
    over(mul((disc, "RGB"), glow), (disc, "A"))

    # The sunspots, drifting across: u wraps round the disc's width (0.165 .. 0.835), and they
    # are kept to the disc by its own coverage.
    u = mask(uv, "r")
    v = mask(uv, "g")
    across = add(mul(one(unreal.MaterialExpressionFrac, div(add(sub(u, 0.165), mul(time, drift)), 0.67)), 0.67), 0.165)
    spots = sample("T_SunSpots", add(append(across, v), nudge))
    over((spots, "RGB"), mul((spots, "A"), (disc, "A")))

    # The flare, in bursts.
    pulse_base = add(mul(wave(1.1, 0.0), 0.5), 0.5)
    pulse_node = node(unreal.MaterialExpressionPower)
    link(pulse_base, pulse_node, "Base")
    pulse_node.set_editor_property("const_exponent", 6.0)
    pulse = (pulse_node, "")
    flare = sample("T_SunFlare", scale_about(0.65, 0.29, add(mul(pulse, 0.12), 1.0)))
    over(mul((flare, "RGB"), add(mul(mul(pulse, burst), 3.0), 1.0)), (flare, "A"))

    covered = node(unreal.MaterialExpressionMax)
    link(stack["A"], covered, "A")
    covered.set_editor_property("const_b", 0.0005)
    colour = mul(div(stack["P"], (covered, "")), power)
    mel.connect_material_property(colour[0], colour[1], unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(stack["A"][0], stack["A"][1], unreal.MaterialProperty.MP_OPACITY)
    finish(path, mat)


def make_level():
    if eal.does_asset_exist(LEVEL):
        say("level %s already exists - left as it is" % LEVEL)
        return
    world = tools.create_asset("L_Genesis", "/Game/Maps", unreal.World, unreal.WorldFactory())
    eal.save_asset(LEVEL)
    say("level %s made (empty): %s" % (LEVEL, world.get_path_name() if world else "FAILED"))


try:
    # NOT DURING PLAY (2026-10-15): with PIE running, the editor's asset functions refuse
    # ("The Editor is currently in a play mode.") and does_asset_exist answers False - so an
    # existing material looked missing, create_asset tried to make it over the one in use, and
    # the editor put up "Overwrite Existing Object" / "is in use" dialogs. Nothing was harmed,
    # but this script must not run then.
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("REFUSING: stop Play first - the editor's asset functions do not work during PIE.")
    for build in (build_glow, build_glass, build_star, build_sun_disc, build_sun_live):
        try:
            build()
        except Exists as there:
            say("material %s already exists - left as it is" % there)
    make_level()
    say("DONE")
except Exception as error:
    say("FAILED: %s" % error)
