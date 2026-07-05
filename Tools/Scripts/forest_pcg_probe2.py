# forest_pcg_probe2.py — get PCG node pin LABELS (for add_edge) + Transform Points randomize props.
import unreal, json
at = unreal.AssetToolsHelpers.get_asset_tools()
g = at.create_asset("PCG_TMP_PROBE2", "/Game/PCG", unreal.PCGGraph, unreal.PCGGraphFactory())

def add(cls):
    n = g.add_node_of_type(cls)
    return n[0] if isinstance(n, (tuple, list)) else n

def labels(node):
    out = {"in": [], "out": []}
    for p in node.input_pins:
        try:
            out["in"].append(str(p.get_editor_property("properties").get_editor_property("label")))
        except Exception as e:
            out["in"].append("ERR:%r" % e)
    for p in node.output_pins:
        try:
            out["out"].append(str(p.get_editor_property("properties").get_editor_property("label")))
        except Exception as e:
            out["out"].append("ERR:%r" % e)
    return out

res = {}
ss = add(unreal.PCGSurfaceSamplerSettings); res["surfacesampler"] = labels(ss)
gl = add(unreal.PCGGetLandscapeSettings); res["getlandscape"] = labels(gl)
tr = add(unreal.PCGTransformPointsSettings); res["transform"] = labels(tr)
trs = tr.get_settings()
res["transform_props"] = [p for p in dir(trs) if any(k in p.lower() for k in ["rotation", "scale", "offset", "absolute"]) and not p.startswith("_")]
sp = add(unreal.PCGStaticMeshSpawnerSettings); res["spawner"] = labels(sp)

unreal.EditorAssetLibrary.delete_asset("/Game/PCG/PCG_TMP_PROBE2")
open(r"C:/Users/wpark/projects/sibelius-game/forest-pcg-probe2.json", "w").write(json.dumps(res, indent=1))
unreal.log("###PCGPROBE2### done")
