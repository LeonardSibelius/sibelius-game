# forest_ground_mat.py — SIB Forest Phase 1: a simple, reliable landscape ground material.
# Grass (PN springGrass 8K albedo) tiled across the terrain. No paint layers (renders immediately),
# no slope logic yet — the slope/rock-on-steep blend is the next refinement now the rock pack is in.
# Run via the bridge. Creates /Game/Forest/M_ForestGround.
import unreal

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
PKG = "/Game/Forest"
unreal.EditorAssetLibrary.make_directory(PKG)
if unreal.EditorAssetLibrary.does_asset_exist(PKG + "/M_ForestGround"):
    unreal.EditorAssetLibrary.delete_asset(PKG + "/M_ForestGround")

mat = at.create_asset("M_ForestGround", PKG, unreal.Material, unreal.MaterialFactoryNew())
grass = unreal.EditorAssetLibrary.load_asset(
    "/Game/PN_GrassLibrary/Textures/grassTextures/MasterTextures/springGrass_Albedo_8k_I")

ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
if grass:
    ts.set_editor_property("texture", grass)
tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -750, 0)
tc.set_editor_property("u_tiling", 127.0)   # 508 m / 127 ~= 4 m grass tiles
tc.set_editor_property("v_tiling", 127.0)
mel.connect_material_expressions(tc, "", ts, "UVs")
mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
rough.set_editor_property("r", 0.9)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(PKG + "/M_ForestGround")
unreal.log("###FORESTMAT### M_ForestGround created (grass=%s)" % (bool(grass)))
