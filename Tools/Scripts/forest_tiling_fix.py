# forest_tiling_fix.py — set M_ForestGround's grass tiling to 127 (4 m tiles on the 508 m landscape)
# in place, without delete/recreate (which fails while the landscape references it).
import unreal
mat = unreal.EditorAssetLibrary.load_asset("/Game/Forest/M_ForestGround")
n = 0
exprs = []
try:
    exprs = list(mat.get_editor_property("expression_collection").expressions)
except Exception as e:
    unreal.log("###TILEFIX### expression_collection failed %r" % e)
for e in exprs:
    if isinstance(e, unreal.MaterialExpressionTextureCoordinate):
        e.set_editor_property("u_tiling", 127.0)
        e.set_editor_property("v_tiling", 127.0)
        n += 1
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset("/Game/Forest/M_ForestGround")
unreal.log("###TILEFIX### set 127 tiling on %d TexCoord node(s)" % n)
