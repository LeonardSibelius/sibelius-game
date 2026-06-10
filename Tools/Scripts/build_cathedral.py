"""
build_cathedral.py  —  Sibelius Game, Ch7 cathedral assembly

RUN #2: dress the nave — god-ray shafts, pews, organ, entrance door + rose window —
keep the arch arcade + altar, and DROP the lumpy mosaic floor (the level's own floor is
the ground for now). Re-runnable: clears prior 'CathedralBuild' actors and rebuilds, so
you can tweak the CONSTANTS block and run again.

RUN NATIVELY (NOT the bridge). With L_Cathedral open, in the editor's bottom console box:
    py "C:/Users/wpark/Claude/build_cathedral.py"

Every placement constant is exposed up top — if a piece faces or sits wrong, it's a
one-line change here, then re-run.
"""
import unreal

# ===================== CONSTANTS (tweak, re-run) =====================
MESH_DIR = "/Game/UltimateGothicCathedralChurch/Mesh"
TAG      = "CathedralBuild"

NAVE_LEN = 3600.0   # cm  entrance (X=0)  ->  apse (X=NAVE_LEN)
NAVE_WID = 1200.0   # cm  across the nave

# Floor — OFF for now (mosaic tiles came out lumpy; using the level's own floor)
DO_FLOOR         = False
FLOOR_MESH       = "SM_Tile_Floor_Mosaic_00001__4405"
FLOOR_TILE_SCALE = 4.0

# Arch arcade (worked great in run #1)
ARCH_MESH  = "SM_Arch_Gothic_Pointed_00001__4094"
ARCH_COUNT = 8
ARCH_YAW   = 90.0

# Altar backdrop
ALTAR_MESH  = "SM_Altar_Main_Marble_00001__1341"
ALTAR_YAW   = 180.0
ALTAR_INSET = 300.0

# --- RUN #2 dressing ---
DOOR_MESH = "SM_Door_Cathedral_Huge_00001__6336"      # 511 x 14 x 315
DOOR_YAW  = 90.0
DOOR_X    = -50.0

ROSE_MESH  = "SM_Window_Stained_Rose_00001__1632"     # 116 x 116 x 3.4
ROSE_YAW   = 90.0
ROSE_SCALE = 3.0
ROSE_X     = -40.0
ROSE_Z     = 800.0

GODRAY_MESH  = "SM_Light_God_Ray_Shaft_00001__3691"   # 162 x 160 x 322
GODRAY_COUNT = 0     # OFF — mesh read as floating blobs; real god-rays come from lighting later
GODRAY_PITCH = 25.0
GODRAY_YAW   = 0.0
GODRAY_SCALE = 2.0
GODRAY_Y     = 250.0
GODRAY_Z     = 400.0

PEW_MESH = "SM_Pew_Bench_Oak_00001__1409"             # 346 x 57 x 90
PEW_ROWS = 0     # OFF — empty nave reads cleaner; side pews had no sightline to the altar
PEW_YAW  = 90.0
PEW_Y    = 350.0      # +/- from the central aisle
PEW_X0   = 500.0
PEW_DX   = 450.0

ORGAN_MESH = "SM_Organ_Pipe_Massive_00001__6407"      # 710 x 284 x 937
ORGAN_YAW  = -90.0
ORGAN_X    = 2850.0
ORGAN_Y    = 600.0
# ====================================================================

def _as():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    except Exception:
        return None
_AS = _as()

def _all():
    return _AS.get_all_level_actors() if _AS else unreal.EditorLevelLibrary.get_all_level_actors()

def _spawn(mesh, loc, rot):
    if _AS:
        return _AS.spawn_actor_from_object(mesh, loc, rot)
    return unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, loc, rot)

def _destroy(a):
    (_AS.destroy_actor if _AS else unreal.EditorLevelLibrary.destroy_actor)(a)

def load_mesh(n):
    return unreal.load_asset(MESH_DIR + "/" + n)

def min_z(mesh, s):
    try:
        return mesh.get_bounding_box().min.z * s
    except Exception:
        return 0.0

def spawn(mesh, x, y, z, pitch=0.0, yaw=0.0, roll=0.0, s=1.0, label=None):
    # NOTE: keyword Rotator args — positional order is a known UE-Python trap.
    a = _spawn(mesh, unreal.Vector(x, y, z),
               unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
    if a:
        a.set_actor_scale3d(unreal.Vector(s, s, s))
        try:
            a.tags = [unreal.Name(TAG)]
        except Exception:
            pass
        if label:
            a.set_actor_label(label)
    return a

def on_floor(mesh, x, y, yaw=0.0, s=1.0, pitch=0.0, label=None):
    return spawn(mesh, x, y, -min_z(mesh, s), pitch, yaw, 0.0, s, label)

def clear_previous():
    n = 0
    for a in _all():
        try:
            if unreal.Name(TAG) in a.tags:
                _destroy(a); n += 1
        except Exception:
            pass
    return n

def build():
    removed = clear_previous()
    c = 0

    if DO_FLOOR:
        try:
            fm = load_mesh(FLOOR_MESH); t = 85.2 * FLOOR_TILE_SCALE
            for i in range(int(NAVE_LEN/t)+1):
                for j in range(int(NAVE_WID/t)+1):
                    if on_floor(fm, i*t, -NAVE_WID/2 + j*t, 0.0, FLOOR_TILE_SCALE):
                        c += 1
        except Exception as e:
            unreal.log_error("[Cathedral] FLOOR: %s" % e)

    try:
        am = load_mesh(ARCH_MESH); step = NAVE_LEN/(ARCH_COUNT-1)
        for i in range(ARCH_COUNT):
            if on_floor(am, i*step, 0.0, ARCH_YAW, 1.0, label="Arch_%02d" % i):
                c += 1
    except Exception as e:
        unreal.log_error("[Cathedral] ARCH: %s" % e)

    try:
        if on_floor(load_mesh(ALTAR_MESH), NAVE_LEN-ALTAR_INSET, 0.0, ALTAR_YAW, 1.0, label="Altar_Main"):
            c += 1
    except Exception as e:
        unreal.log_error("[Cathedral] ALTAR: %s" % e)

    try:
        if on_floor(load_mesh(DOOR_MESH), DOOR_X, 0.0, DOOR_YAW, 1.0, label="Door_Entrance"):
            c += 1
    except Exception as e:
        unreal.log_error("[Cathedral] DOOR: %s" % e)

    try:
        if spawn(load_mesh(ROSE_MESH), ROSE_X, 0.0, ROSE_Z, 0.0, ROSE_YAW, 0.0, ROSE_SCALE, "Rose_Window"):
            c += 1
    except Exception as e:
        unreal.log_error("[Cathedral] ROSE: %s" % e)

    try:
        gm = load_mesh(GODRAY_MESH); step = NAVE_LEN/(GODRAY_COUNT+1)
        for i in range(1, GODRAY_COUNT+1):
            y = GODRAY_Y if (i % 2) else -GODRAY_Y
            if spawn(gm, i*step, y, GODRAY_Z, GODRAY_PITCH, GODRAY_YAW, 0.0, GODRAY_SCALE, "GodRay_%02d" % i):
                c += 1
    except Exception as e:
        unreal.log_error("[Cathedral] GODRAY: %s" % e)

    try:
        pm = load_mesh(PEW_MESH)
        for r in range(PEW_ROWS):
            x = PEW_X0 + r*PEW_DX
            for side in (-1, 1):
                if on_floor(pm, x, side*PEW_Y, PEW_YAW, 1.0, label="Pew_%d%s" % (r, "L" if side < 0 else "R")):
                    c += 1
    except Exception as e:
        unreal.log_error("[Cathedral] PEW: %s" % e)

    DO_ORGAN = False     # OFF — read white/odd against the gold stone; deleted
    if DO_ORGAN:
        try:
            if on_floor(load_mesh(ORGAN_MESH), ORGAN_X, ORGAN_Y, ORGAN_YAW, 1.0, label="Organ"):
                c += 1
        except Exception as e:
            unreal.log_error("[Cathedral] ORGAN: %s" % e)

    msg = "[Cathedral] RUN #2 done: removed %d, spawned %d" % (removed, c)
    unreal.log(msg); print(msg)

build()
