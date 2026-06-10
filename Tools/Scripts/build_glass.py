"""
build_glass.py — Church of AI: wrap the nave in stained glass.
  - two SIDE walls (lancet glass filling the bays between the stone arches)
  - an APSE end-wall (glass closing the far vista behind the slot)
  - a CEILING canopy of glass overhead
All visual-only (no collision; the columns channel the player), cycling the
StainedGlass3D vertical-lancet palette. Re-runnable (clears prior 'GlassWall' actors).

Run NATIVELY (not the bridge), L_Cathedral open, in the editor's bottom Cmd box:
    py "C:/Users/wpark/Claude/build_glass.py"     then Ctrl+S.
"""
import unreal

MAT_DIR  = "/Game/StainedGlass3D/Materials"
CUBE     = "/Engine/BasicShapes/Cube"
TAG      = "GlassWall"

NAVE_LEN = 3600.0
ARCH_CNT = 8
WALL_Y   = 620.0      # side walls, both sides (+/-)
PANEL_H  = 700.0      # wall height
PANEL_T  = 5.0        # panel thickness
CEIL_Z   = 1200.0     # ceiling height (below the arch peaks ~1374)
DO_CEILING = True
KEEP     = "column"   # vertical lancet skins only

def _as():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    except Exception:
        return None
_AS = _as()

def all_actors():
    return _AS.get_all_level_actors() if _AS else unreal.EditorLevelLibrary.get_all_level_actors()
def spawn(obj, loc, rot):
    return (_AS.spawn_actor_from_object if _AS else unreal.EditorLevelLibrary.spawn_actor_from_object)(obj, loc, rot)
def destroy(a):
    (_AS.destroy_actor if _AS else unreal.EditorLevelLibrary.destroy_actor)(a)

# usable (vertical lancet) materials
ar = unreal.AssetRegistryHelpers.get_asset_registry()
mats = []
for a in ar.get_assets_by_path(MAT_DIR, recursive=True):
    try:
        o = a.get_asset()
        if isinstance(o, unreal.MaterialInterface) and KEEP in o.get_name().lower():
            mats.append(o)
    except Exception:
        pass
mats.sort(key=lambda m: m.get_name())
unreal.log("[Glass] %d lancet materials" % len(mats))

# clear prior glass (idempotent)
removed = 0
for a in list(all_actors()):
    try:
        if unreal.Name(TAG) in a.tags:
            destroy(a); removed += 1
    except Exception:
        pass

cube = unreal.load_asset(CUBE)
bays  = max(1, ARCH_CNT - 1)
bay_w = NAVE_LEN / bays

def make_panel(cx, cy, cz, sx, sy, sz, mat, label):
    a = spawn(cube, unreal.Vector(cx, cy, cz), unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    if a:
        a.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, sz / 100.0))
        try:
            a.tags = [unreal.Name(TAG)]
        except Exception:
            pass
        a.set_actor_label(label)
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        if c:
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            if mat:
                c.set_material(0, mat)
        return True
    return False

built = 0
idx = 0
if mats:
    # clerestory band: fills the open-sky gap between wall-top and the ceiling
    BAND_H  = CEIL_Z - PANEL_H            # 500 with current tunables
    BAND_CZ = PANEL_H + BAND_H / 2.0      # band center Z
    # side walls (lower row + clerestory row)
    for side in (-1.0, 1.0):
        for i in range(bays):
            x = (i + 0.5) * bay_w
            if make_panel(x, side * WALL_Y, PANEL_H / 2.0, bay_w, PANEL_T, PANEL_H,
                          mats[idx % len(mats)], "Glass_%s_%02d" % ("L" if side < 0 else "R", i)):
                built += 1
            idx += 1
            if BAND_H > 0 and make_panel(x, side * WALL_Y, BAND_CZ, bay_w, PANEL_T, BAND_H,
                          mats[idx % len(mats)], "Glass_%sTop_%02d" % ("L" if side < 0 else "R", i)):
                built += 1
            idx += 1
    # end-walls — apse (behind the slot) AND entrance (behind the gate) — lower + clerestory rows
    apse_x = NAVE_LEN + 60.0
    ent_x  = -60.0
    apse_n = 5
    apse_w = (2.0 * WALL_Y) / apse_n
    for ex, nm in ((apse_x, "Apse"), (ent_x, "Ent")):
        for i in range(apse_n):
            y = -WALL_Y + (i + 0.5) * apse_w
            if make_panel(ex, y, PANEL_H / 2.0, PANEL_T, apse_w, PANEL_H,
                          mats[idx % len(mats)], "Glass_%s_%02d" % (nm, i)):
                built += 1
            idx += 1
            if BAND_H > 0 and make_panel(ex, y, BAND_CZ, PANEL_T, apse_w, BAND_H,
                          mats[idx % len(mats)], "Glass_%sTop_%02d" % (nm, i)):
                built += 1
            idx += 1
    # ceiling canopy
    if DO_CEILING:
        for i in range(bays):
            x = (i + 0.5) * bay_w
            if make_panel(x, 0.0, CEIL_Z, bay_w, 2.0 * WALL_Y, PANEL_T,
                          mats[idx % len(mats)], "Glass_Ceil_%02d" % i):
                built += 1
            idx += 1
        # strips closing the slivers between ceiling ends and the two end-walls
        for sx, nm in ((NAVE_LEN + 30.0, "Apse"), (-30.0, "Ent")):
            if make_panel(sx, 0.0, CEIL_Z, 60.0 + PANEL_T, 2.0 * WALL_Y, PANEL_T,
                          mats[idx % len(mats)], "Glass_Ceil_%s" % nm):
                built += 1
            idx += 1
else:
    unreal.log_error("[Glass] no lancet materials found under %s" % MAT_DIR)

msg = "[Glass] built %d panels (walls + apse + ceiling), removed %d" % (built, removed)
unreal.log(msg); print(msg)
