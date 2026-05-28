# build_office_v2_phase1b.py
# Checkpoint-3 pivot, Phase 1b -- FURNITURE + light dressing into the existing
# /Game/Maps/L_Office_v2_QuadArt shell (built by build_office_v2_phase1a.py).
#
# Layout (locked with Walt 2026-05-27): desk faces the north window; player faces SOUTH
# toward the bookshelf/corkboard wall. Two bookshelves conceal the hidden Ch1-exit door.
# Static meshes ONLY (QuadArt vendor Blueprints never ship). Hidden door is a SEPARATE
# placed mesh embedded in the wall -- the QuadArt wall .uasset is never modified (LC-4).
# 15-item corkboard planting + Code Vision port are LATER phases.
#
# Idempotent: re-running first deletes every actor tagged "Phase1bFurniture", then re-adds.
#
# Run headless:
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript \
#     -script="<abs>/Tools/build_office_v2_phase1b.py" \
#     -unattended -nopause -nosplash -NullRHI -stdout -EnablePlugins=PythonScriptPlugin

import unreal

MAP_PATH = "/Game/Maps/L_Office_v2_QuadArt"
PHASE_TAG = "Phase1bFurniture"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_lib = unreal.EditorAssetLibrary

created = []
warnings = []

_index = {}
def build_index():
    for root in ("/Game/HouseFurniture", "/Game/ModularHouses"):
        for ap in unreal.EditorAssetLibrary.list_assets(root, True, False):
            short = ap.split("/")[-1].split(".")[0]
            _index.setdefault(short, ap)

def smc(a):
    return a.get_component_by_class(unreal.StaticMeshComponent)

def aabb(a):
    o, e = a.get_actor_bounds(False)
    return (o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z)

def snap(a, cx=None, minx=None, maxx=None, cy=None, miny=None, maxy=None, minz=None, maxz=None):
    x0, x1, y0, y1, z0, z1 = aabb(a)
    ox, oy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    loc = a.get_actor_location()
    dx = dy = dz = 0.0
    if cx is not None: dx = cx - ox
    elif minx is not None: dx = minx - x0
    elif maxx is not None: dx = maxx - x1
    if cy is not None: dy = cy - oy
    elif miny is not None: dy = miny - y0
    elif maxy is not None: dy = maxy - y1
    if minz is not None: dz = minz - z0
    elif maxz is not None: dz = maxz - z1
    a.set_actor_location(unreal.Vector(loc.x + dx, loc.y + dy, loc.z + dz), False, False)

def place(name, label, tags=None, yaw=0.0, stencil=None, optional=False, **snapkw):
    path = _index.get(name)
    if not path:
        msg = "MESH NOT FOUND: %s (%s)" % (name, label)
        warnings.append(msg)
        if not optional:
            unreal.log_warning("  ! " + msg)
        return None
    mesh = unreal.load_asset(path)
    if mesh is None:
        warnings.append("LOAD FAILED: %s" % path); return None
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0.0, 0.0, yaw))
    smc(a).set_static_mesh(mesh)
    a.set_actor_label(label)
    snap(a, **snapkw)
    taglist = [PHASE_TAG] + (tags or [])
    a.set_editor_property("tags", [unreal.Name(t) for t in taglist])
    if stencil is not None:
        c = smc(a)
        c.set_editor_property("render_custom_depth", True)
        c.set_editor_property("custom_depth_stencil_value", stencil)
    created.append(label)
    return a

def main():
    unreal.log("=== Phase 1b: furniture into L_Office_v2_QuadArt ===")
    if not asset_lib.does_asset_exist(MAP_PATH):
        raise RuntimeError("Shell map missing -- run Phase 1a first")
    les.load_level(MAP_PATH)
    build_index()

    # Idempotency: remove any prior Phase-1b furniture; also retire the 1a scale-ref
    # mannequin (its purpose is done; a furnished room conveys scale via the furniture).
    removed = 0
    for a in eas.get_all_level_actors():
        tags = a.get_editor_property("tags")
        if (tags and unreal.Name(PHASE_TAG) in tags) or a.get_actor_label() == "ScaleRef_Mannequin":
            eas.destroy_actor(a); removed += 1
    unreal.log("removed %d prior Phase1b/scale-ref actors" % removed)

    # ---- Desk (faces north window); capture its top for on-desk items ----
    desk = place("SM_WorkingTable_A1", "Desk_Oak", tags=["CodeVision.Label=Desk_Oak"],
                 yaw=0.0, cx=0.0, cy=150.0, minz=0.0)
    desk_top = aabb(desk)[5] if desk else 78.0
    unreal.log("desk_top z = %.1f" % desk_top)

    # ---- Chair (faces the desk / window, +Y at yaw 0). Pulled back to Y=59 per Walt's
    #      manual edit (2026-05-27), re-centered X=0 and snapped to floor (minz=0). ----
    place("SM_LeatherChair_A1", "Chair_Office", tags=["CodeVision.Label=Chair_Office"],
          yaw=0.0, cx=0.0, cy=59.0, minz=0.0)

    # ---- Dual monitors + peripherals on the desk (screens face the chair, -Y) ----
    place("SM_Monitor_A1", "Monitor_L", tags=["CodeVision.Label=Monitor"], yaw=180.0, cx=-45.0, cy=172.0, minz=desk_top)
    place("SM_Monitor_A1", "Monitor_R", tags=["CodeVision.Label=Monitor"], yaw=180.0, cx=45.0, cy=172.0, minz=desk_top)
    place("SM_Keyboard_A1", "Keyboard", yaw=180.0, cx=0.0, cy=128.0, minz=desk_top, optional=True)
    place("SM_Mouse_A1", "Mouse", yaw=180.0, cx=30.0, cy=128.0, minz=desk_top, optional=True)

    # ---- Two bookshelves against the south wall (conceal the hidden door); capture top ----
    bl = place("SM_BookShelf_A", "Bookshelf_Wood_L", tags=["CodeVision.Reveal", "CodeVision.Label=Bookshelf_Wood"],
               stencil=2, yaw=0.0, cx=-54.0, miny=-280.0, minz=0.0)
    place("SM_BookShelf_A", "Bookshelf_Wood_R", tags=["CodeVision.Reveal", "CodeVision.Label=Bookshelf_Wood"],
          stencil=2, yaw=0.0, cx=54.0, miny=-280.0, minz=0.0)
    shelf_top = aabb(bl)[5] if bl else 220.0

    # ---- Hidden Ch1-exit door: separate mesh embedded in the south wall, behind shelves ----
    place("SM_Door_Inside_A1", "Door_Hidden_Ch1Exit",
          tags=["CodeVision.Hidden", "CodeVision.Label=Door_Hidden_Ch1Exit"],
          stencil=1, yaw=90.0, cx=0.0, maxy=-280.0, minz=0.0)

    # ---- Corkboard (mesh only; 15-item planting later) on the south wall, west of shelves ----
    place("SM_Board_A1", "Investigation_Board", tags=["CodeVision.Label=Investigation_Board"],
          yaw=90.0, cx=-185.0, miny=-280.0, minz=108.0)

    # ---- Light dressing (optional; facing may need a later visual tweak) ----
    place("SM_TableLamp_A1", "Desk_Lamp", yaw=0.0, cx=78.0, cy=170.0, minz=desk_top, optional=True)
    place("SM_Mug_A1", "Coffee_Mug", yaw=0.0, cx=-78.0, cy=138.0, minz=desk_top, optional=True)
    place("SM_Papers_A1", "Desk_Papers", yaw=15.0, cx=-25.0, cy=130.0, minz=desk_top, optional=True)
    place("SM_Photo_A1_Worn", "Desk_Photo", yaw=200.0, cx=48.0, cy=128.0, minz=desk_top, optional=True)
    place("SM_BookSet_A1", "Desk_Books", yaw=0.0, cx=72.0, cy=132.0, minz=desk_top, optional=True)
    place("SM_BookSet_A2", "Shelf_Books_L", yaw=0.0, cx=-54.0, cy=-258.0, minz=shelf_top, optional=True)
    place("SM_BookSet_A3", "Shelf_Books_R", yaw=8.0, cx=54.0, cy=-258.0, minz=shelf_top, optional=True)
    place("SM_OldClock_A1", "Wall_Clock", yaw=0.0, minx=-280.0, cy=20.0, minz=300.0, optional=True)

    # ---- Move PlayerStart to room centre, facing south toward the bookshelf wall ----
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.PlayerStart):
            # NOTE: UE's headless commandlet save does NOT reliably persist (or read back)
            # PlayerStart rotation -- verified across multiple methods. The spawn facing is
            # finalized MANUALLY in the editor; the .umap is the source of truth. This call
            # only seeds a best-effort starting location/rotation as scaffolding.
            a.set_actor_location_and_rotation(unreal.Vector(0.0, -30.0, 95.0),
                                              unreal.Rotator(0.0, -90.0, 0.0), False, True)
            unreal.log("PlayerStart -> (0,-30,95) yaw -90 (faces south, clear of chair)")
            break

    # ---- Save ----
    les.save_current_level()
    asset_lib.save_asset(MAP_PATH)

    # ---- Self-verification ----
    unreal.log("=== VERIFY (%d furniture actors) ===" % len(created))
    door_x = None; shelf_x_span = None
    for a in eas.get_all_level_actors():
        tags = a.get_editor_property("tags")
        if not (tags and unreal.Name(PHASE_TAG) in tags):
            continue
        lbl = a.get_actor_label()
        x0, x1, y0, y1, z0, z1 = aabb(a)
        c = smc(a)
        col = c.get_collision_enabled()
        st = c.get_editor_property("custom_depth_stencil_value") if c.get_editor_property("render_custom_depth") else "-"
        tagstr = ",".join([str(t) for t in tags if str(t) != PHASE_TAG])
        unreal.log("VERIFY %-20s X[%.0f,%.0f] Y[%.0f,%.0f] Zbase=%.1f Ztop=%.1f stencil=%s col=%s tags=[%s]"
                   % (lbl, x0, x1, y0, y1, z0, z1, str(st), str(col).split('.')[-1].split(':')[0], tagstr))
        if lbl == "Door_Hidden_Ch1Exit": door_x = (x0, x1)
        if lbl == "Bookshelf_Wood_L": shelf_x_span = [x0, x1]
        if lbl == "Bookshelf_Wood_R" and shelf_x_span:
            shelf_x_span = [min(shelf_x_span[0], x0), max(shelf_x_span[1], x1)]
    if door_x and shelf_x_span:
        covered = shelf_x_span[0] <= door_x[0] and shelf_x_span[1] >= door_x[1]
        unreal.log("VERIFY door X%s vs bookshelves X[%.0f,%.0f] -> concealed=%s"
                   % (str(door_x), shelf_x_span[0], shelf_x_span[1], covered))
    if warnings:
        unreal.log_warning("Non-fatal issues (%d): %s" % (len(warnings), "; ".join(warnings)))
    unreal.log("=== Phase 1b complete ===")

main()
