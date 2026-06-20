# forest_mat_v2.py — create M_ForestGround_4m (grass, 127x tiling = ~4 m tiles on the 508 m
# landscape) under a fresh name (sidesteps the delete/recreate-in-use quirk) and assign it.
import unreal
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
NAME, PKG = "M_ForestGround_4m", "/Game/Forest"
path = PKG + "/" + NAME

if not unreal.EditorAssetLibrary.does_asset_exist(path):
    mat = at.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
    grass = unreal.EditorAssetLibrary.load_asset(
        "/Game/PN_GrassLibrary/Textures/grassTextures/MasterTextures/springGrass_Albedo_8k_I")
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
    if grass:
        ts.set_editor_property("texture", grass)
    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -750, 0)
    tc.set_editor_property("u_tiling", 127.0)
    tc.set_editor_property("v_tiling", 127.0)
    mel.connect_material_expressions(tc, "", ts, "UVs")
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
else:
    mat = unreal.EditorAssetLibrary.load_asset(path)

for a in eas.get_all_level_actors():
    if isinstance(a, unreal.Landscape):
        a.set_editor_property("landscape_material", mat)
les.save_current_level()
unreal.log("###MATV2### created+assigned %s (mat=%s)" % (path, bool(mat)))
