# make_meadow_material.py — ground for the meadow, from textures that still exist.
#
# *** RUN FROM THE OPEN EDITOR with L_Meadow loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_meadow_material.py"
#
# Safe to re-run: it deletes and rebuilds the material, then re-assigns it.
#
# ---------------------------------------------------------------------------
# WHY NOT M_ForestGround, WHICH IS RIGHT THERE.
#
# Because it is a corpse. It samples
# /Game/PN_GrassLibrary/Textures/grassTextures/MasterTextures/springGrass_Albedo_8k_I
# and that pack was deleted in the v0.7.2 size diet - the one that halved the download.
# The material survived the cull, its texture did not, and a landscape wearing a material
# whose texture is missing renders as the grey checkerboard Walt saw.
#
# That is also why Content/Forest is 1 MB: it is not an empty folder, it is orphaned
# materials pointing at content that no longer ships. Worth knowing before anything else
# in there gets reached for.
#
# So this builds M_MeadowGround from T_Grass_A and T_Grass_A_nm, which live in
# HouseFurniture and are still very much present - HouseFurniture is the second largest
# folder in the cook at 1.6 GB, so it is not going anywhere.
#
# ---------------------------------------------------------------------------
# TILING IS THE ONE NUMBER TO ARGUE WITH. The landscape is 1008 metres across and the
# texture is a couple of metres of grass. At TILING = 150 each repeat covers about 6.7m,
# which reads as ground underfoot and as obvious stripes from the hilltops. That is the
# usual trade and there is no value that solves it - fixing repetition properly means
# blending two scales or a macro variation mask, which is a real material job and not
# this script's business. Start here, judge it from where the player will stand.

import unreal

MAT_DIR = "/Game/Environments/Materials"
MAT_NAME = "M_MeadowGround"
MAT_PATH = MAT_DIR + "/" + MAT_NAME

ALBEDO = "/Game/HouseFurniture/Textures/T_Grass_A"
NORMAL = "/Game/HouseFurniture/Textures/T_Grass_A_nm"

TILING = 150.0
ROUGHNESS = 0.92      # grass is not shiny; a wet-looking meadow reads as plastic
NORMAL_TIGHTEN = 1.0

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

albedo_tex = eal.load_asset(ALBEDO)
normal_tex = eal.load_asset(NORMAL)
if not albedo_tex:
    unreal.log_error("[meadow] missing %s - nothing to stand on" % ALBEDO)
    raise SystemExit(1)

# One TextureCoordinate feeds both samplers, so albedo and normal can never drift out
# of register with each other.
uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -800, 0)
uv.set_editor_property("u_tiling", TILING)
uv.set_editor_property("v_tiling", TILING)

base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -150)
base.set_editor_property("texture", albedo_tex)
mel.connect_material_expressions(uv, "", base, "UVs")
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
notes.append("albedo %s at %.0f tiling" % (ALBEDO.split("/")[-1], TILING))

if normal_tex:
    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 150)
    nrm.set_editor_property("texture", normal_tex)
    # A normal map sampled as colour comes out flat and slightly wrong-coloured; the
    # sampler type has to say normal or the lighting is a lie.
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(uv, "", nrm, "UVs")
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    notes.append("normal map wired")
else:
    notes.append("no normal map found - flat lighting")

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
    notes.append("NO LANDSCAPE IN THIS LEVEL - material built but not assigned. "
                 "Open L_Meadow and re-run.")
else:
    for ls in landscapes:
        ls.set_editor_property("landscape_material", mat)
        notes.append("assigned to %s" % ls.get_actor_label())
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    notes.append("level saved")

for n in notes:
    unreal.log("[meadow] " + n)
print("\n".join(notes))
