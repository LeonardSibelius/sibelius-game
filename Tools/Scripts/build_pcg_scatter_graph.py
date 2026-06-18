# build_pcg_scatter_graph.py — SIB-47 PCG spike. Authors /Game/PCG/PCG_ElsewhereScatter
# headless (re-runnable, rebuilds from scratch):
#   World Ray Hit Query -> Surface Sampler -> Transform Points (seeded) -> Static Mesh
#   Spawner (_K detail meshes) -> Output.
#
# WHY World Ray Hit (the empty-floor fix): the Surface Sampler's "Surface" pin needs real
# UPCGSurfaceData. The graph used to feed it the Input node's "In" (the component's actor
# data) — which is NOT a surface, so the sampler logged "No surfaces found from which to
# generate", aborted, and the floor came up EMPTY. World Ray Hit Query produces a
# UPCGSurfaceData by tracing world collision: with default params it casts a ray straight
# DOWN from the top of the PCG component's own actor bounds to the bottom, so it lands on
# the Elsewhere floor mesh (the floor ISMs use a BlockAll profile -> they block the node's
# default ECC_WorldStatic channel). No manual Z/extent tuning — it's all derived from the
# builder actor's bounds, and the Surface Sampler (Unbounded=false, no Bounding Shape wired)
# uses those same bounds as the sampling footprint.
#
# Determinism comes from the UPCGComponent Seed (AElsewhereBuilder::RunPCGScatter sets it
# = the run's LayoutSeed); World Ray Hit is a pure function of the (fixed) geometry. Run
# editor-closed:
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript -script=".../build_pcg_scatter_graph.py"
import unreal

GRAPH = "/Game/PCG/PCG_ElsewhereScatter"
MESHES = [
    "/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AA_Base.SM_Lamp_AA_Base",
    "/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AB_Base.SM_Lamp_AB_Base",
]
def log(s): unreal.log("###PCG### " + str(s))

G = unreal.load_object(None, GRAPH + "." + GRAPH.split("/")[-1])
inp, outp = G.get_input_node(), G.get_output_node()
for n in list(G.nodes):
    if n not in (inp, outp):
        G.remove_node(n)

def add(cls):
    n = G.add_node_of_type(cls)
    return n[0] if isinstance(n, (tuple, list)) else n

worldray = add(unreal.PCGWorldRayHitSettings)   # the Surface for the sampler (traces the floor)
samp = add(unreal.PCGSurfaceSamplerSettings)
xform = add(unreal.PCGTransformPointsSettings)
spawn = add(unreal.PCGStaticMeshSpawnerSettings)

def wire(a, ap, b, bp):
    try:
        G.add_edge(a, ap, b, bp); log("edge %s.%s -> %s.%s OK" % (a.get_name(), ap, b.get_name(), bp))
    except Exception as e:
        unreal.log_error("###PCG### edge %s.%s -> %s.%s FAIL %r" % (a.get_name(), ap, b.get_name(), bp, e))
# World Ray Hit produces the surface (NOT the Input node — that was the empty-floor bug).
wire(worldray, "Out", samp, "Surface")
wire(samp, "Out", xform, "In")
wire(xform, "Out", spawn, "In")
wire(spawn, "Out", outp, "Out")

sS = samp.get_settings()
# COUNT KNOB: points ~= density * floor_area_m². ServerCathedral floor is ~20x20m = 400 m²,
# so 0.01/m² -> ~4 hero props (matches the C++ fallback's 3-6). The old 0.0008 gave an
# expected 0.32 points -> usually ZERO, which read as "empty floor" even when sampling worked.
sS.set_editor_property("points_per_squared_meter", 0.01)
sS.set_editor_property("point_extents", unreal.Vector(50, 50, 50))
sS.set_editor_property("unbounded", False)   # use the component (room) bounds as the footprint

tS = xform.get_settings()
tS.set_editor_property("rotation_min", unreal.Rotator(0, 0, 0))
tS.set_editor_property("rotation_max", unreal.Rotator(0, 0, 360))
tS.set_editor_property("offset_min", unreal.Vector(-40, -40, 0))
tS.set_editor_property("offset_max", unreal.Vector(40, 40, 0))

spS = spawn.get_settings()
spS.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
sel = spS.get_editor_property("mesh_selector_parameters")
entries = []
for mp in MESHES:
    e = unreal.PCGMeshSelectorWeightedEntry()
    e.set_editor_property("weight", 1)
    d = e.get_editor_property("descriptor")
    d.set_editor_property("static_mesh", unreal.load_asset(mp))
    e.set_editor_property("descriptor", d)
    entries.append(e)
sel.set_editor_property("mesh_entries", entries)

# Count edges (input-pin side) for a final tally.
ne = 0
for n in (samp, xform, spawn, outp):
    for p in n.input_pins:
        ne += len(p.edges)
unreal.EditorAssetLibrary.save_asset(GRAPH)
log("DONE: nodes=%d edges=%d meshes=%d" % (len(G.nodes), ne, len(entries)))
