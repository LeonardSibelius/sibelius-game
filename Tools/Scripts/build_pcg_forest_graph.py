# build_pcg_forest_graph.py — SIB Forest scatter. Authors /Game/PCG/PCG_ForestScatter:
#   Get Landscape Data -> (per layer) Surface Sampler -> Transform Points (random yaw + scale)
#   -> Static Mesh Spawner (weighted) -> Output. Unbounded samplers cover the whole landscape
#   (and follow hills if sculpted). Deterministic from the PCG component seed (1337).
#
# ART DIRECTION: this Elsewhere is an AI DREAMWORLD, not a natural biome. Lush + surreal —
# winter spruce + rocks, but with abundant flowers + ferns blooming THROUGH the snow. Density
# cranked; err toward outrageous.
#
# RETUNE: everything is in the LAYERS list below — density (ppm = points/m^2), scale band, and the
# mesh set per layer. Add/remove a layer = add/remove a dict. Future place-types get their own copy
# of this recipe (or their own graph); keep the layer shape identical so retuning stays trivial.
import unreal

GRAPH_PKG, GRAPH_NAME = "/Game/PCG", "PCG_ForestScatter"
GRAPH_PATH = GRAPH_PKG + "/" + GRAPH_NAME

_TREE = "/Game/PN_interactiveSpruceForest/ExampleContent/Winter/Meshes/full/high"
_ROCK = "/Game/Fab/Rocks_Set_Stone_Collection_Scan/rocks_set_stone_collection_scan/StaticMeshes"
_GRASS = "/Game/PN_GrassLibrary/Meshes/grassMesh"
_FLOWER = "/Game/PN_OpenWorldFoliage/Meshes"
_FERN = "/Game/PN_FoliageCollection/Meshes/groundPlantMesh"

def M(d, n, w=1):
    return ("%s/%s.%s" % (d, n, n), w)

# (name, points/m^2, scaleMin, scaleMax, uniformScale, [ (meshPath, weight), ... ])
LAYERS = [
    ("trees",   0.010, 0.85, 1.25, True, [M(_TREE, "winter_spruce_full_01"), M(_TREE, "winter_spruce_full_02"), M(_TREE, "winter_spruce_full_03")]),
    ("rocks",   0.0015, 0.5, 1.8, False, [M(_ROCK, "rocks_set_stone_collection_scan")]),
    ("ferns",   0.12, 0.7, 1.5, True, [M(_FERN, "ground_01_01"), M(_FERN, "ground_01_03"), M(_FERN, "ground_02_01"), M(_FERN, "ground_02_03"), M(_FERN, "ground_03_01"), M(_FERN, "ground_03_03")]),
    ("flowers", 0.18, 0.7, 1.4, True, [M(_FLOWER, "openWorld_flower_01_01"), M(_FLOWER, "openWorld_flower_02_01"), M(_FLOWER, "openWorld_flower_03_01"), M(_FLOWER, "openWorld_flower_04_01"), M(_FLOWER, "openWorld_flower_05_01"), M(_FLOWER, "openWorld_flower_06_01")]),
    ("grass",   0.40, 0.7, 1.3, True, [M(_GRASS, "grass_01_01_mesh"), M(_GRASS, "grass_01_03_mesh"), M(_GRASS, "grass_01_05_mesh"), M(_GRASS, "grass_01_07_mesh")]),
]

at = unreal.AssetToolsHelpers.get_asset_tools()
if unreal.EditorAssetLibrary.does_asset_exist(GRAPH_PATH):
    G = unreal.load_asset(GRAPH_PATH)
else:
    G = at.create_asset(GRAPH_NAME, GRAPH_PKG, unreal.PCGGraph, unreal.PCGGraphFactory())
inp, outp = G.get_input_node(), G.get_output_node()
for n in list(G.nodes):
    if n not in (inp, outp):
        G.remove_node(n)

def add(cls):
    n = G.add_node_of_type(cls)
    return n[0] if isinstance(n, (tuple, list)) else n

def wire(a, ap, b, bp):
    G.add_edge(a, ap, b, bp)

gl = add(unreal.PCGGetLandscapeSettings)
try:
    gl.get_settings().set_editor_property("unbounded", True)
except Exception as e:
    unreal.log("###FORESTPCG### gl unbounded skip %r" % e)

for (name, ppm, smin, smax, uniform, meshes) in LAYERS:
    ss = add(unreal.PCGSurfaceSamplerSettings)
    sset = ss.get_settings()
    sset.set_editor_property("points_per_squared_meter", float(ppm))
    try:
        sset.set_editor_property("unbounded", True)
    except Exception as e:
        unreal.log("###FORESTPCG### ss unbounded skip %r" % e)
    tr = add(unreal.PCGTransformPointsSettings)
    ts = tr.get_settings()
    ts.set_editor_property("rotation_min", unreal.Rotator(0.0, 0.0, 0.0))
    ts.set_editor_property("rotation_max", unreal.Rotator(0.0, 0.0, 360.0))
    ts.set_editor_property("scale_min", unreal.Vector(smin, smin, smin))
    ts.set_editor_property("scale_max", unreal.Vector(smax, smax, smax))
    ts.set_editor_property("uniform_scale", uniform)
    sp = add(unreal.PCGStaticMeshSpawnerSettings)
    sps = sp.get_settings()
    sps.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
    sel = sps.get_editor_property("mesh_selector_parameters")
    entries = []
    for path, w in meshes:
        e = unreal.PCGMeshSelectorWeightedEntry()
        e.set_editor_property("weight", w)
        d = e.get_editor_property("descriptor")
        d.set_editor_property("static_mesh", unreal.load_asset(path))
        e.set_editor_property("descriptor", d)
        entries.append(e)
    sel.set_editor_property("mesh_entries", entries)
    wire(gl, "Out", ss, "Surface")
    wire(ss, "Out", tr, "In")
    wire(tr, "Out", sp, "In")
    wire(sp, "Out", outp, "Out")
    unreal.log("###FORESTPCG### layer %s: %d meshes, ppm=%s" % (name, len(entries), ppm))

unreal.EditorAssetLibrary.save_asset(GRAPH_PATH)
ne = sum(len(p.edges) for n in G.nodes for p in n.input_pins)
unreal.log("###FORESTPCG### DONE nodes=%d in-edges=%d layers=%d" % (len(G.nodes), ne, len(LAYERS)))
