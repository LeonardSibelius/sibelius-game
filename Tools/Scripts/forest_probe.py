# forest_probe.py — SIB Forest Phase 1: probe the live editor for the landscape-creation API,
# the PN ground textures (for the auto-material), and the tree/grass mesh paths. Read-only.
# Run via the bridge: exec-python "exec(open(r'.../forest_probe.py').read())"
import unreal, json
ar = unreal.AssetRegistryHelpers.get_asset_registry()

def names(path):
    try:
        return [str(a.package_name) for a in ar.get_assets_by_path(path, recursive=True)]
    except Exception as e:
        return ["ERR:%r" % e]

out = {}
# 1) landscape creation API surface in this engine build
out["unreal_landscape_symbols"] = [x for x in dir(unreal) if "andscape" in x]
try:
    out["Landscape_import_methods"] = [m for m in dir(unreal.Landscape) if ("import" in m.lower() or "heightmap" in m.lower())]
except Exception as e:
    out["Landscape_import_methods"] = "ERR:%r" % e
out["has_LandscapeEditorSubsystem"] = hasattr(unreal, "LandscapeEditorSubsystem")
out["has_new_landscape_helpers"] = [x for x in dir(unreal) if "ewLandscape" in x or "andscapeImport" in x]

# 2) trees (full spruce, high LOD, summer preferred)
trees = names("/Game/PN_interactiveSpruceForest")
out["spruce_full_high"] = [n for n in trees if "spruce_full" in n.lower() and "_low" not in n.lower()][:8]

# 3) grass meshes
gl = names("/Game/PN_GrassLibrary")
out["grass_meshes"] = [n for n in gl if "/meshes/" in n.lower()][:8]

# 4) ground textures usable as grass / rock landscape layers
def texlike(lst):
    return [n for n in lst if n.split("/")[-1].lower().startswith("t_") or "/textures" in n.lower()]
cand = texlike(gl)
out["grass_textures"] = [n for n in cand if any(k in n.lower() for k in ["grass","moss","ground","forest","green","base"])][:25]
out["rock_textures"]  = [n for n in cand if any(k in n.lower() for k in ["rock","cliff","stone","gravel"])][:25]
out["all_PNgrass_textures_sample"] = cand[:40]

# 5) the landscape master material + its layer info assets
out["MA_LayerGround_exists"] = unreal.EditorAssetLibrary.does_asset_exist("/Game/PN_GrassLibrary/Materials/LandscapeMaterials/defaultLandscape/MA_LayerGround")

open(r"C:/Users/wpark/projects/sibelius-game/forest-probe.json", "w").write(json.dumps(out, indent=1))
unreal.log("###FORESTPROBE### wrote forest-probe.json")
