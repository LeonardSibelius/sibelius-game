# build_pcg_forest_graph.py — SIB Forest Phase 2/3. Authors /Game/PCG/PCG_ForestScatter:
#   Get Landscape Data -> (per layer) Surface Sampler -> Transform Points (random yaw + scale)
#   -> Static Mesh Spawner (weighted) -> Output.
# Three layers at different densities: trees (sparse) / rocks (sparser) / grass (dense).
# Deterministic from the PCG component seed. Densities/scales are the tuning knobs (top of file).
# Run via the bridge (editor open) or -run=pythonscript (editor closed).
import unreal

GRAPH_PKG, GRAPH_NAME = "/Game/PCG", "PCG_ForestScatter"
GRAPH_PATH = GRAPH_PKG + "/" + GRAPH_NAME

T = "/Game/PN_interactiveSpruceForest/ExampleContent/Winter/Meshes/full/high"
TREES = [(T + "/winter_spruce_full_01.winter_spruce_full_01", 1),
         (T + "/winter_spruce_full_02.winter_spruce_full_02", 1),
         (T + "/winter_spruce_full_03.winter_spruce_full_03", 1)]
ROCK = [("/Game/Fab/Rocks_Set_Stone_Collection_Scan/rocks_set_stone_collection_scan/StaticMeshes/rocks_set_stone_collection_scan.rocks_set_stone_collection_scan", 1)]
GR = "/Game/PN_GrassLibrary/Meshes/grassMesh"
GRASS = [(GR + "/grass_01_01_mesh.grass_01_01_mesh", 1),
         (GR + "/grass_01_03_mesh.grass_01_03_mesh", 1),
         (GR + "/grass_01_05_mesh.grass_01_05_mesh", 1),
         (GR + "/grass_01_07_mesh.grass_01_07_mesh", 1)]

# --- tuning knobs (per square metre) ---
PPM_TREES, PPM_ROCKS, PPM_GRASS = 0.010, 0.0015, 0.06

at = unreal.AssetToolsHelpers.get_asset_tools()
if unreal.EditorAssetLibrary.does_asset_exist(GRAPH_PATH):
    G = unreal.load_asset(GRAPH_PATH)
else:
    G = at.create_asset(GRAPH_NAME, GRAPH_PKG, unreal.PCGGraph, unreal.PCGGraphFactory())
inp, outp = G.get_input_node(), G.get_output_node()
# idempotent: clear any existing non-IO nodes before rebuilding
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
    gl.get_settings().set_editor_property("unbounded", True)   # get the whole landscape
except Exception as e:
    unreal.log("###FORESTPCG### gl unbounded skip %r" % e)

def layer(name, ppm, meshes, smin, smax, uniform):
    ss = add(unreal.PCGSurfaceSamplerSettings)
    sset = ss.get_settings()
    sset.set_editor_property("points_per_squared_meter", float(ppm))
    try:
        sset.set_editor_property("unbounded", True)   # sample the whole surface, not just component bounds
    except Exception as e:
        unreal.log("###FORESTPCG### ss unbounded skip %r" % e)
    tr = add(unreal.PCGTransformPointsSettings)
    ts = tr.get_settings()
    ts.set_editor_property("rotation_min", unreal.Rotator(0.0, 0.0, 0.0))
    ts.set_editor_property("rotation_max", unreal.Rotator(0.0, 0.0, 360.0))   # random yaw
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

layer("trees", PPM_TREES, TREES, 0.85, 1.25, True)
layer("rocks", PPM_ROCKS, ROCK, 0.5, 1.8, False)
layer("grass", PPM_GRASS, GRASS, 0.7, 1.3, True)

unreal.EditorAssetLibrary.save_asset(GRAPH_PATH)
ne = 0
for n in G.nodes:
    for p in n.input_pins:
        ne += len(p.edges)
unreal.log("###FORESTPCG### DONE nodes=%d edges=%d" % (len(G.nodes), ne))
