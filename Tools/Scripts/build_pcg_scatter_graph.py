# build_pcg_scatter_graph.py — SIB-47 PCG spike. Authors /Game/PCG/PCG_ElsewhereScatter
# headless (re-runnable, rebuilds from scratch):
#   Create Points Grid (component-relative) -> Transform Points (seeded) -> Static Mesh
#   Spawner (_K detail meshes) -> Output.
#
# WHY Create Points Grid (the empty-floor fix, take 2): PIE showed an EMPTY floor and the
# log gave the real cause — "LogPCG: Error: [RegisterOrUpdatePCGComponent] Component has
# invalid bounds, not registered nor updated" -> "0 props". At BeginPlay (mid-AssembleGeometry)
# the runtime-built floor ISMs haven't had their world bounds recomputed yet, so the actor's
# aggregate bounds are INVALID. UPCGComponent::CreateGenerateTask aborts on invalid
# GetGridBounds() BEFORE the graph runs, so NO sampler (World Ray Hit included) ever executes.
# Two independent fixes, both applied:
#   (1) HERE: sample explicit bounds at a known floor Z instead of tracing the world. Create
#       Points Grid with CoordinateSpace=OriginalComponent centers a flat grid on the builder
#       actor's transform (its location is floor-stand Z) using its own GridExtents/CellSize —
#       no dependency on component bounds, world collision, or the physics scene.
#   (2) C++: AElsewhereBuilder::RunPCGScatter defers Generate() one tick (SetTimerForNextTick),
#       so by the time it runs the ISM bounds are valid and the component registers cleanly.
#       (Mandatory: without valid bounds, generation aborts regardless of the graph.)
#
# Determinism: Create Points Grid is a pure function of its settings; the per-prop yaw + XY
# jitter come from Transform Points seeded by the UPCGComponent Seed (RunPCGScatter sets it =
# the run's LayoutSeed), so same seed -> same layout. Run editor-closed:
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript -script=".../build_pcg_scatter_graph.py"
import unreal

GRAPH = "/Game/PCG/PCG_ElsewhereScatter"
# Richer scatter: a weighted mix of varied _K lamp/fixture bases (small detail props). The
# big deliberate machinery (pipe runs, bulkhead arch-ribs) is placed by the C++ structural
# pass — AElsewhereBuilder::SpawnStructuralProps — so this stays the "scattered detail" layer.
# (mesh_path, weight)
MESH_WEIGHTS = [
    ("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AA_Base.SM_Lamp_AA_Base", 3),
    ("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AB_Base.SM_Lamp_AB_Base", 3),
    ("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AC_Base.SM_Lamp_AC_Base", 2),
    ("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AD_Base.SM_Lamp_AD_Base", 2),
    ("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_BA_Base.SM_Lamp_BA_Base", 1),
    ("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_BB_Base.SM_Lamp_BB_Base", 1),
]
MESHES = [m for m, _ in MESH_WEIGHTS]   # kept for the DONE tally
def log(s): unreal.log("###PCG### " + str(s))

G = unreal.load_object(None, GRAPH + "." + GRAPH.split("/")[-1])
inp, outp = G.get_input_node(), G.get_output_node()
for n in list(G.nodes):
    if n not in (inp, outp):
        G.remove_node(n)

def add(cls):
    n = G.add_node_of_type(cls)
    return n[0] if isinstance(n, (tuple, list)) else n

grid = add(unreal.PCGCreatePointsGridSettings)   # explicit bounds at floor Z (no surface/raycast)
xform = add(unreal.PCGTransformPointsSettings)
spawn = add(unreal.PCGStaticMeshSpawnerSettings)

def wire(a, ap, b, bp):
    try:
        G.add_edge(a, ap, b, bp); log("edge %s.%s -> %s.%s OK" % (a.get_name(), ap, b.get_name(), bp))
    except Exception as e:
        unreal.log_error("###PCG### edge %s.%s -> %s.%s FAIL %r" % (a.get_name(), ap, b.get_name(), bp, e))
# Create Points Grid generates the points itself (no Surface input -> no World Ray Hit, no
# dependency on the floor's collision being live). The Input node stays unwired.
wire(grid, "Out", xform, "In")
wire(xform, "Out", spawn, "In")
wire(spawn, "Out", outp, "Out")

gS = grid.get_settings()
# Centered on the builder actor's transform (its location is the floor-stand Z, same Z the
# C++ fallback used for props). GridExtents 600 (half) fits inside every place-type's room
# (smallest is 800 half-extent); CellSize 600 -> a 2x2 grid = ~4 hero props (matches the C++
# fallback's 3-6). Flat: Z extent 0 -> one layer at floor height. COUNT KNOB: CellSize (down
# = more) and GridExtents. No culling -> no dependency on the (initially invalid) actor bounds.
gS.set_editor_property("coordinate_space", unreal.PCGCoordinateSpace.ORIGINAL_COMPONENT)
gS.set_editor_property("grid_extents", unreal.Vector(600, 600, 0))
gS.set_editor_property("cell_size", unreal.Vector(600, 600, 600))
gS.set_editor_property("cull_points_outside_volume", False)

tS = xform.get_settings()
# Break the grid look: random yaw + a generous XY jitter (seeded by the component Seed, so
# same seed -> same scatter). Z offset 0 keeps props on the floor.
tS.set_editor_property("rotation_min", unreal.Rotator(0, 0, 0))
tS.set_editor_property("rotation_max", unreal.Rotator(0, 0, 360))
tS.set_editor_property("offset_min", unreal.Vector(-200, -200, 0))
tS.set_editor_property("offset_max", unreal.Vector(200, 200, 0))

spS = spawn.get_settings()
spS.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
sel = spS.get_editor_property("mesh_selector_parameters")
entries = []
for mp, w in MESH_WEIGHTS:
    e = unreal.PCGMeshSelectorWeightedEntry()
    e.set_editor_property("weight", w)
    d = e.get_editor_property("descriptor")
    d.set_editor_property("static_mesh", unreal.load_asset(mp))
    e.set_editor_property("descriptor", d)
    entries.append(e)
sel.set_editor_property("mesh_entries", entries)

# Count edges (input-pin side) for a final tally.
ne = 0
for n in (grid, xform, spawn, outp):
    for p in n.input_pins:
        ne += len(p.edges)
unreal.EditorAssetLibrary.save_asset(GRAPH)
log("DONE: nodes=%d edges=%d meshes=%d" % (len(G.nodes), ne, len(entries)))
