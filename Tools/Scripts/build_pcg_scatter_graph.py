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
# Debris scatter: a weighted mix of bounds-verified SciFi BOXES A & B crates/containers/barrels
# (dump_box_bounds.py). KEPT IN LOCKSTEP with the C++ builder ScatterSet in ElsewhereBuilder.cpp
# so the PCG path and the C++ fallback read as the same pile. (The _K kit has no small props, so
# the scatter layer now uses the dedicated box props instead of _K machinery.)
#
# NB: the PCG path does NOT do the C++ path's exclusion zones / open corridor / per-seed subset /
# clustering / wall-bias (those are C++-only — see the C++ scatter + docs), so it can spawn near
# the curio. It's behind bUsePCGScatter (default OFF; the C++ path is what ships + what the gate
# verifies). Its LOOK is Walt's PIE gate, not headless. (mesh_path, weight)
_A = "/Game/SciFiBoxes_A/Meshes"
_B = "/Game/SciFi_Box_B/Meshes"
MESH_WEIGHTS = [
    ("%s/Box3/Box3_v0.Box3_v0" % _A, 5),
    ("%s/Box3/Box3_v3.Box3_v3" % _A, 3),
    ("%s/Box2/Box2_v0.Box2_v0" % _A, 4),
    ("%s/Box2/Box2_v4.Box2_v4" % _A, 3),
    ("%s/Box1/Box1_Closed_v0.Box1_Closed_v0" % _A, 3),
    ("%s/Box1/Box1_Closed_v3.Box1_Closed_v3" % _A, 2),
    ("%s/Box1/Box1_Container.Box1_Container" % _A, 4),
    ("%s/SM_SciFiBox_B_C.SM_SciFiBox_B_C" % _B, 3),
    ("%s/SM_SciFiBox_B_Barrel_A.SM_SciFiBox_B_Barrel_A" % _B, 3),
    ("%s/SM_SciFiBox_B_F.SM_SciFiBox_B_F" % _B, 2),
    ("%s/SM_SciFiBox_B_G.SM_SciFiBox_B_G" % _B, 2),
    ("%s/Box1/Box1_Base_v0.Box1_Base_v0" % _A, 2),
    ("%s/SM_SciFiBox_B_E.SM_SciFiBox_B_E" % _B, 2),
    ("%s/SM_SciFiBox_B_A.SM_SciFiBox_B_A" % _B, 1),
    ("%s/SM_SciFiBox_B_D.SM_SciFiBox_B_D" % _B, 1),
    ("%s/SM_SciFiBox_I.SM_SciFiBox_I" % _B, 1),
    ("%s/Box1/Box1_Open_Full_v0.Box1_Open_Full_v0" % _A, 1),
]
MESHES = [m for m, _ in MESH_WEIGHTS]   # kept for the DONE tally
def log(s): unreal.log("###PCG### " + str(s))

G = unreal.load_object(None, GRAPH + "." + GRAPH.split("/")[-1])
inp, outp = G.get_input_node(), G.get_output_node()
for n in list(G.nodes):
    if n not in (inp, outp):
        G.remove_node(n)

# Opt-in clustering experiment: spatial noise -> density filter so points bunch into lived-in
# clumps with bare patches instead of an even grid. DEFAULT OFF: its point yield + look can't be
# verified headlessly (PCG generation needs live PIE bounds), and a mis-tuned density filter could
# cull the whole set (empty floor). The guaranteed PCG win is the curated-mesh swap above; the C++
# path already does deterministic, gate-verified clustering. Flip this to experiment in PIE.
ADD_NOISE = False

def add(cls):
    n = G.add_node_of_type(cls)
    return n[0] if isinstance(n, (tuple, list)) else n

grid = add(unreal.PCGCreatePointsGridSettings)   # explicit bounds at floor Z (no surface/raycast)
xform = add(unreal.PCGTransformPointsSettings)
spawn = add(unreal.PCGStaticMeshSpawnerSettings)

# Build the optional noise/density-filter nodes defensively (class names can differ across engine
# builds) — only when ADD_NOISE; any failure falls back to the core grid->transform->spawner chain.
noise = densf = None
if ADD_NOISE:
    try:
        noise = add(unreal.PCGSpatialNoiseSettings)
        densf = add(unreal.PCGDensityFilterSettings)
    except Exception as e:
        unreal.log_warning("###PCG### noise/density-filter node unavailable (%r) — core chain only" % e)
        noise = densf = None

def wire(a, ap, b, bp):
    try:
        G.add_edge(a, ap, b, bp); log("edge %s.%s -> %s.%s OK" % (a.get_name(), ap, b.get_name(), bp))
    except Exception as e:
        unreal.log_error("###PCG### edge %s.%s -> %s.%s FAIL %r" % (a.get_name(), ap, b.get_name(), bp, e))

# Create Points Grid generates the points itself (no Surface input -> no World Ray Hit, no
# dependency on the floor's collision being live). The Input node stays unwired.
wire(grid, "Out", xform, "In")
if noise is not None and densf is not None:
    wire(xform, "Out", noise, "In")
    wire(noise, "Out", densf, "In")
    wire(densf, "Out", spawn, "In")
    # Noise -> $Density; conservative lower bound so the worst case is a near-no-op (never an
    # empty floor). The noise reads the component Seed (= LayoutSeed) -> per-seed + deterministic.
    try:
        nS = noise.get_settings()
        nS.set_editor_property("mode", unreal.PCGSpatialNoiseMode.VORONOI2D)
        vt = nS.get_editor_property("value_target")
        vt.set_editor_property("point_property", unreal.PCGPointProperties.DENSITY)
        nS.set_editor_property("value_target", vt)
        dS = densf.get_settings()
        dS.set_editor_property("lower_bound", 0.20)
        dS.set_editor_property("upper_bound", 1.0)
        log("noise/density clustering wired (Voronoi2D -> $Density, keep [0.20,1.0])")
    except Exception as e:
        unreal.log_warning("###PCG### noise/filter config skipped (%r) — nodes present, defaults" % e)
else:
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
for n in [x for x in (grid, xform, noise, densf, spawn, outp) if x is not None]:
    for p in n.input_pins:
        ne += len(p.edges)
unreal.EditorAssetLibrary.save_asset(GRAPH)
log("DONE: nodes=%d edges=%d meshes=%d" % (len(G.nodes), ne, len(entries)))
