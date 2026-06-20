# forest_tiling_fix2.py — read + set M_ForestGround's TextureCoordinate tiling to 127, in place.
# Tries the 5.7 expression-access variants and reports which worked + the tiling found.
import unreal
mat = unreal.EditorAssetLibrary.load_asset("/Game/Forest/M_ForestGround")

exprs, how = None, "none"
try:
    exprs = list(unreal.MaterialEditingLibrary.get_material_expressions(mat)); how = "MEL.get_material_expressions"
except Exception:
    try:
        exprs = list(mat.get_editor_property("expressions")); how = "prop:expressions"
    except Exception:
        try:
            exprs = list(mat.get_editor_property("expression_collection").expressions); how = "prop:expression_collection"
        except Exception as e:
            unreal.log("###T2### no expr access: %r" % e)

cnt, found = 0, []
for e in (exprs or []):
    if isinstance(e, unreal.MaterialExpressionTextureCoordinate):
        found.append(float(e.get_editor_property("u_tiling")))
        e.set_editor_property("u_tiling", 127.0)
        e.set_editor_property("v_tiling", 127.0)
        cnt += 1
if cnt:
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset("/Game/Forest/M_ForestGround")
unreal.log("###T2### via=%s exprs=%s texcoords_fixed=%d old_tiling=%s" % (how, len(exprs or []), cnt, found))
