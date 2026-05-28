# build_office_v2_phase1a.py
# Checkpoint-3 pivot, Phase 1a -- room SHELL + lighting only (NO furniture) for the QuadArt
# rebuild of Chapter 1. Builds /Game/Maps/L_Office_v2_QuadArt as a non-World-Partition level.
#
# Locked design decisions (Walt, 2026-05-27):
#   - 560x560 cm interior (4 x 140 cm kit grid).
#   - Exterior facade walls (443 cm tall) all around; real north window + golden-hour beam.
#   - North = window, South = (future) bookshelf/corkboard wall, East = entry, West = solid.
#   - Static meshes ONLY (QuadArt vendor Blueprints never ship). Source meshes never modified.
#
# Run headless:
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript \
#     -script="<abs>/Tools/build_office_v2_phase1a.py" \
#     -unattended -nopause -nosplash -NullRHI -stdout -EnablePlugins=PythonScriptPlugin
#
# Units = centimetres. Interior floor surface = z 0. Room centred on world origin.

import unreal

MAP_PATH = "/Game/Maps/L_Office_v2_QuadArt"
HALF = 280.0          # interior half-size -> 560x560
WALL_TOP_Z = 443.2    # exterior wall height (probed)

WALL800 = "/Game/ModularHouses/Meshes/Wall/Walls/SM_Wall_Floor1_800"
WALL400 = "/Game/ModularHouses/Meshes/Wall/Walls/SM_Wall_Floor1_400"
WALL200 = "/Game/ModularHouses/Meshes/Wall/Walls/SM_Wall_Floor1_200"
WINDOW400 = "/Game/ModularHouses/Meshes/Wall/Windows/SM_Window_Floor1_A1_400"
FLOOR400 = "/Game/ModularHouses/Meshes/Floors/Type_C/SM_Floor_400"
DOOR = "/Game/ModularHouses/Meshes/Doors/SM_Door_Outside_A1"
MANNEQUIN = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_lib = unreal.EditorAssetLibrary

created = []
warnings = []
shell_actors = []


def smc(actor):
    return actor.get_component_by_class(unreal.StaticMeshComponent)


def aabb(actor):
    o, e = actor.get_actor_bounds(False)
    return (o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z)


def spawn_mesh(path, yaw, label):
    mesh = unreal.load_asset(path)
    if mesh is None:
        warnings.append("LOAD FAILED %s" % path)
        return None
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0.0, 0.0, yaw))
    smc(a).set_static_mesh(mesh)
    a.set_actor_label(label)
    created.append(label)
    return a


def snap(actor, cx=None, minx=None, maxx=None, cy=None, miny=None, maxy=None, minz=None, maxz=None):
    """Shift actor so the chosen world-AABB feature lands on target. Robust to pivot/rotation."""
    x0, x1, y0, y1, z0, z1 = aabb(actor)
    ox, oy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    loc = actor.get_actor_location()
    dx = dy = dz = 0.0
    if cx is not None:   dx = cx - ox
    elif minx is not None: dx = minx - x0
    elif maxx is not None: dx = maxx - x1
    if cy is not None:   dy = cy - oy
    elif miny is not None: dy = miny - y0
    elif maxy is not None: dy = maxy - y1
    if minz is not None: dz = minz - z0
    elif maxz is not None: dz = maxz - z1
    actor.set_actor_location(unreal.Vector(loc.x + dx, loc.y + dy, loc.z + dz), False, False)
    return actor


def main():
    unreal.log("=== Building L_Office_v2_QuadArt (Phase 1a shell+lighting) ===")
    if asset_lib.does_asset_exist(MAP_PATH):
        unreal.log_warning("Map exists; deleting for clean rebuild: %s" % MAP_PATH)
        asset_lib.delete_asset(MAP_PATH)
    les.new_level(MAP_PATH)   # basic (non-WP, non-OFPA) level

    # ---- Floor: 2x2 SM_Floor_400 (280 each) = 560x560, top surface snapped to z=0 ----
    for sx in (-1, 1):
        for sy in (-1, 1):
            f = spawn_mesh(FLOOR400, 0.0, "Floor_%s%s" % ("E" if sx > 0 else "W", "N" if sy > 0 else "S"))
            if f:
                snap(f, cx=sx * 140.0, cy=sy * 140.0, maxz=0.0)
                shell_actors.append(f)

    # ---- Ceiling: 2x2 SM_Floor_400 reused as slab, bottom snapped to wall top (z=443.2) ----
    for sx in (-1, 1):
        for sy in (-1, 1):
            c = spawn_mesh(FLOOR400, 0.0, "Ceiling_%s%s" % ("E" if sx > 0 else "W", "N" if sy > 0 else "S"))
            if c:
                snap(c, cx=sx * 140.0, cy=sy * 140.0, minz=WALL_TOP_Z)
                shell_actors.append(c)

    # ---- South wall (y=-280) : solid 560, room-facing (+Y) face at y=-280 ----
    s = spawn_mesh(WALL800, 0.0, "Wall_South")   # yaw0 -> length along X
    if s:
        snap(s, cx=0.0, maxy=-HALF, minz=0.0); shell_actors.append(s)

    # ---- West wall (x=-280) : solid 560, length along Y (yaw90), room-facing (+X) face at x=-280 ----
    w = spawn_mesh(WALL800, 90.0, "Wall_West")
    if w:
        snap(w, cy=0.0, maxx=-HALF, minz=0.0); shell_actors.append(w)

    # ---- East wall (x=+280, ENTRY) : solid 560 + closed door leaf against inner face ----
    e = spawn_mesh(WALL800, 90.0, "Wall_East")
    if e:
        snap(e, cy=0.0, minx=HALF, minz=0.0); shell_actors.append(e)
    door = spawn_mesh(DOOR, 90.0, "Door_Entry_East")   # closed leaf; CP4 makes it openable
    if door:
        snap(door, cy=0.0, maxx=HALF, minz=0.0)

    # ---- North wall (y=+280, WINDOW) : 140 fill + 280 window (centred) + 140 fill = 560 ----
    nwin = spawn_mesh(WINDOW400, 0.0, "Wall_North_Window")   # 280 wide, centred
    if nwin:
        snap(nwin, cx=0.0, miny=HALF, minz=0.0); shell_actors.append(nwin)
    nl = spawn_mesh(WALL200, 0.0, "Wall_North_FillW")        # 140 fill, west side
    if nl:
        snap(nl, maxx=-140.0, miny=HALF, minz=0.0); shell_actors.append(nl)
    nr = spawn_mesh(WALL200, 0.0, "Wall_North_FillE")        # 140 fill, east side
    if nr:
        snap(nr, minx=140.0, miny=HALF, minz=0.0); shell_actors.append(nr)

    # ---- Glass plane behind the window aperture (W-1: inset a few mm), CodeVision.Translucent ----
    try:
        cube = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")
        g = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        smc(g).set_static_mesh(cube)
        # 280 wide x 4cm thick x 250 tall, sat in the window opening, inset 1.5cm behind face
        g.set_actor_scale3d(unreal.Vector(280 / 100.0, 4 / 100.0, 250 / 100.0))
        g.set_actor_label("Window_Glass")
        g.set_editor_property("tags", [unreal.Name("CodeVision.Translucent")])
        snap(g, cx=0.0, maxy=HALF - 1.5, minz=90.0)   # sill ~90cm like CP2
        created.append("Window_Glass")
    except Exception as ex:
        warnings.append("glass: %s" % ex)

    # ---- Lighting (golden hour) -- reused from CP2 ----
    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 380), unreal.Rotator(0, 0, 0))
    sun.set_actor_rotation(unreal.Rotator(-10.0, -100.0, 0.0), False)   # low warm sun from north window
    sun.set_actor_label("Sun_GoldenHour")
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_editor_property("intensity", 6.0)
    sc.set_light_color(unreal.LinearColor(1.0, 0.78, 0.46, 1.0))
    sc.set_editor_property("atmosphere_sun_light", True)
    sc.set_editor_property("use_temperature", True)
    sc.set_editor_property("temperature", 4200.0)
    created.append("Sun_GoldenHour")

    for label, cls in [("SkyAtmosphere", unreal.SkyAtmosphere),
                       ("SkyLight", unreal.SkyLight),
                       ("HeightFog", unreal.ExponentialHeightFog)]:
        try:
            a = eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
            a.set_actor_label(label)
            created.append(label)
        except Exception as ex:
            warnings.append("%s: %s" % (label, ex))
    try:
        for a in eas.get_all_level_actors():
            if isinstance(a, unreal.SkyLight):
                a.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
    except Exception as ex:
        warnings.append("skylight recapture: %s" % ex)

    # ---- PlayerStart : near north, facing SOUTH (-Y) toward the future bookshelf/corkboard ----
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 180, 95), unreal.Rotator(0, -90, 0))
    ps.set_actor_label("PlayerStart")
    created.append("PlayerStart")

    # ---- Mannequin scale reference (S-1) : standing on floor near future desk spot ----
    try:
        man = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, -40, 0), unreal.Rotator(0, -90, 0))
        smk = unreal.load_asset(MANNEQUIN)
        mc = man.get_component_by_class(unreal.SkeletalMeshComponent)
        try:
            mc.set_skeletal_mesh_asset(smk)
        except Exception:
            mc.set_editor_property("skeletal_mesh_asset", smk)
        man.set_actor_label("ScaleRef_Mannequin")
        created.append("ScaleRef_Mannequin")
    except Exception as ex:
        warnings.append("mannequin: %s" % ex)

    # ---- Save ----
    les.save_current_level()
    asset_lib.save_asset(MAP_PATH)

    # ---- Self-verification ----
    unreal.log("=== VERIFY ===")
    unreal.log("VERIFY actors created: %d" % len(created))
    # enclosure: combined wall AABB should bracket the 560x560 interior
    walls = [a for a in shell_actors if a.get_actor_label().startswith("Wall_")]
    if walls:
        xs0 = min(aabb(a)[0] for a in walls); xs1 = max(aabb(a)[1] for a in walls)
        ys0 = min(aabb(a)[2] for a in walls); ys1 = max(aabb(a)[3] for a in walls)
        zb = min(aabb(a)[4] for a in walls); zt = max(aabb(a)[5] for a in walls)
        unreal.log("VERIFY wall_AABB X[%.1f,%.1f] Y[%.1f,%.1f] Z[%.1f,%.1f]" % (xs0, xs1, ys0, ys1, zb, zt))
    # per-wall room-facing + base z
    for a in shell_actors:
        lbl = a.get_actor_label()
        x0, x1, y0, y1, z0, z1 = aabb(a)
        col = smc(a).get_collision_enabled()
        unreal.log("VERIFY %-20s X[%.0f,%.0f] Y[%.0f,%.0f] Zbase=%.1f Ztop=%.1f col=%s"
                   % (lbl, x0, x1, y0, y1, z0, z1, str(col)))
    if warnings:
        unreal.log_warning("Non-fatal issues (%d):" % len(warnings))
        for w in warnings:
            unreal.log_warning("  ! %s" % w)
    exists = asset_lib.does_asset_exist(MAP_PATH)
    unreal.log("VERIFY map asset exists -> %s" % exists)
    if not exists:
        raise RuntimeError("L_Office_v2_QuadArt was not saved")
    unreal.log("=== Phase 1a build complete ===")


main()
