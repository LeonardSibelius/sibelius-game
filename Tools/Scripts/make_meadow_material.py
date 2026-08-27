# make_meadow_material.py — ground for the meadow.
#
# *** RUN FROM THE OPEN EDITOR with L_Meadow loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_meadow_material.py"
#
# Safe to re-run: deletes and rebuilds the material, then re-assigns it and saves.
#
# ===========================================================================
# FOUR THINGS WENT WRONG BEFORE THIS WORKED. All four produced the same symptom -
# ground that was not grass - for entirely different reasons, which is why it took five
# passes. Written down so the next landscape in this project costs one.
#
# 1. M_ForestGround IS A CORPSE. It samples an 8K albedo from /Game/PN_GrassLibrary,
#    and that pack was deleted in the v0.7.2 size diet. A landscape wearing a material
#    whose texture is missing renders as a grey checkerboard. This is also why
#    Content/Forest is 1 MB - not an empty folder, orphaned materials pointing at
#    content that no longer ships.
#
# 2. RE-INSTALLING THE PACK DID NOT REPAIR IT. Deleting the texture nulled the
#    material's reference to it; putting the asset back does not re-solder the wire.
#
# 3. T_Grass_A IS A FOLIAGE CARD, NOT A GROUND TILE. It is the texture on a grass
#    BLADE billboard. Tiled across a landscape it is flat green mush. The pack's
#    LandscapeTextures/ground_*_albedo are the tiles authored for this job.
#
# 4. TEXTURECOORDINATE TILES PER COMPONENT ON A LANDSCAPE, not across it. This one has
#    64 components, so a "tiling" of 150 meant roughly 9,600 repeats across a kilometre
#    - about 10 cm per tile, far below a pixel. The GPU averaged it into a single flat
#    colour. That is what fluorescent pea soup is: a grass texture sampled so small it
#    becomes its own average.
#
# And the reason baseGround_III could not simply be picked instead: the Landscape
# Material dropdown lists only materials with bUsedWithLandscape set, and that one is
# built to be a LAYER inside MA_LayerGround, never worn directly.
#
# ===========================================================================
# SO: world-position UVs, the pack's ground tiles, and the usage flag set.
#
# World position removes the ambiguity behind (4) completely. XY in centimetres divided
# by a tile size in centimetres is one repeat every TILE_METRES no matter how the
# landscape is componentised, scaled or resized later. The knob below is a real
# measurement rather than a multiplier whose meaning depends on geometry.
#
# TILE_METRES is the one number to argue with. At 4 m there is detail underfoot and a
# visible grid from a hilltop; no single value solves both. Killing repetition properly
# means blending two scales or a macro variation mask - a real material job, and not
# this script's business.

import unreal

MAT_DIR = "/Game/Environments/Materials"
MAT_NAME = "M_MeadowGround"
MAT_PATH = MAT_DIR + "/" + MAT_NAME

ALBEDO = "/Game/PN_GrassLibrary/Textures/LandscapeTextures/ground_III_albedo"
NORMAL = "/Game/PN_GrassLibrary/Textures/LandscapeTextures/ground_III_normal"

TILE_METRES = 4.0
ROUGHNESS = 0.92      # grass is not shiny; a wet-looking meadow reads as plastic

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
notes = []

# ---------------------------------------------------------------- the material
if eal.does_asset_exist(MAT_PATH):
    eal.delete_asset(MAT_PATH)

mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
if not mat:
    unreal.log_error("[meadow] could not create %s" % MAT_PATH)
    raise SystemExit(1)

# Without this the material is not offered to a landscape at all - see the header.
try:
    mat.set_editor_property("used_with_landscape", True)
    notes.append("used_with_landscape set")
except Exception as e:
    notes.append("could NOT set used_with_landscape: %s" % e)

albedo_tex = eal.load_asset(ALBEDO)
normal_tex = eal.load_asset(NORMAL)
if not albedo_tex:
    unreal.log_error("[meadow] missing %s - nothing to stand on" % ALBEDO)
    raise SystemExit(1)

# ---------------------------------------------------------------- UVs from world space
wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 0)

mask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1000, 0)
mask.set_editor_property("r", True)
mask.set_editor_property("g", True)
mask.set_editor_property("b", False)
mask.set_editor_property("a", False)
mel.connect_material_expressions(wp, "", mask, "")

# One UV chain feeds both samplers, so albedo and normal cannot drift out of register.
uv = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, -800, 0)
uv.set_editor_property("const_b", TILE_METRES * 100.0)   # metres -> unreal units
mel.connect_material_expressions(mask, "", uv, "A")

# ---------------------------------------------------------------- surface
base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -150)
base.set_editor_property("texture", albedo_tex)
mel.connect_material_expressions(uv, "", base, "UVs")
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
notes.append("albedo %s, one tile every %.1f m" % (ALBEDO.split("/")[-1], TILE_METRES))

if normal_tex:
    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 150)
    nrm.set_editor_property("texture", normal_tex)
    # Sampled as colour a normal map comes out flat and the lighting is a quiet lie.
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(uv, "", nrm, "UVs")
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    notes.append("normal map wired")
else:
    notes.append("no normal map - flat lighting")

rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 400)
rough.set_editor_property("r", ROUGHNESS)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

mel.recompile_material(mat)
eal.save_asset(MAT_PATH)
notes.append("saved %s" % MAT_PATH)

# ---------------------------------------------------------------- put it on the ground
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
landscapes = [a for a in eas.get_all_level_actors() if isinstance(a, unreal.Landscape)]

if not landscapes:
    notes.append("NO LANDSCAPE IN THIS LEVEL - built but not assigned. Open L_Meadow.")
else:
    for ls in landscapes:
        ls.set_editor_property("landscape_material", mat)
        notes.append("assigned to %s" % ls.get_actor_label())
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    notes.append("level saved")

for n in notes:
    unreal.log("[meadow] " + n)
print("\n".join(notes))
