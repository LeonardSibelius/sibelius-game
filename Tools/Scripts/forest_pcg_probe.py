# forest_pcg_probe.py — confirm PCG node classes + SurfaceSampler pin names for wiring the
# forest scatter graph. Creates + deletes a throwaway graph. Read-only otherwise.
import unreal, json
out = {}
for k in ["PCGSurfaceSamplerSettings", "PCGGetLandscapeSettings", "PCGTransformPointsSettings",
          "PCGStaticMeshSpawnerSettings", "PCGMeshSelectorWeighted", "PCGMeshSelectorWeightedEntry",
          "PCGDensityFilterSettings", "PCGGraph", "PCGGraphFactory", "PCGVolume", "PCGComponent"]:
    out[k] = hasattr(unreal, k)

try:
    at = unreal.AssetToolsHelpers.get_asset_tools()
    g = at.create_asset("PCG_TMP_PROBE", "/Game/PCG", unreal.PCGGraph, unreal.PCGGraphFactory())

    def add(cls):
        n = g.add_node_of_type(cls)
        return n[0] if isinstance(n, (tuple, list)) else n

    def pins(node):
        ins = [p.get_name() for p in node.input_pins] if hasattr(node, "input_pins") else "n/a"
        outs = [p.get_name() for p in node.output_pins] if hasattr(node, "output_pins") else "n/a"
        return ins, outs

    ss = add(unreal.PCGSurfaceSamplerSettings)
    out["surfacesampler_pins"] = pins(ss)
    sset = ss.get_settings()
    out["surfacesampler_density_props"] = [p for p in dir(sset) if ("point" in p.lower() or "density" in p.lower() or "radius" in p.lower() or "extent" in p.lower()) and not p.startswith("_")]

    gl = add(unreal.PCGGetLandscapeSettings)
    out["getlandscape_pins"] = pins(gl)

    sp = add(unreal.PCGStaticMeshSpawnerSettings)
    out["spawner_pins"] = pins(sp)

    unreal.EditorAssetLibrary.delete_asset("/Game/PCG/PCG_TMP_PROBE")
    out["cleanup"] = "ok"
except Exception as e:
    out["probe_err"] = repr(e)

open(r"C:/Users/wpark/projects/sibelius-game/forest-pcg-probe.json", "w").write(json.dumps(out, indent=1))
unreal.log("###PCGPROBE### done")
