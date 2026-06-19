# dump_kit_bounds.py — SIB-47 PCG scatter curation. Loads every StaticMesh under a kit
# folder and dumps its REAL bounds so per-mesh scatter rules (upright vs free-yaw, scale,
# rest-on-floor, material slot count) are set from fact, not guesswork — see the memory
# [[kit-mesh-axis-convention]] (a marketplace kit's local axes are NOT engine-primitive
# conventions; read the bounding box before placing).
#
# Headless (kit is installed but gitignored; this only READS it). Run editor-CLOSED:
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript \
#       -script=".../Tools/Scripts/dump_kit_bounds.py" -unattended -nopause -nosplash -nullrhi -stdout
#
# Output: one "###KITB###" line per mesh, pipe-delimited, easy to grep/parse:
#   name | path | extX extY extZ (full cm) | originZ | zmin zmax | slots N | shape
# shape ∈ {UPRIGHT, FLAT-X, FLAT-Y, SQUAT, CUBE} from the extents; pivot ∈ {BASE, CENTER, OTHER}.
import unreal

ROOTS = ["/Game/ModularSciFiEnv_K/Meshes"]
TAG = "###KITB###"

def classify_shape(ex, ey, ez):
    fx, fy, fz = ex * 2, ey * 2, ez * 2           # full extents
    foot = max(fx, fy)
    if fz >= 1.5 * foot:
        return "UPRIGHT"
    if foot >= 1.5 * fz:
        # long & low — which horizontal axis dominates
        if fx >= 1.5 * fy:
            return "FLAT-X"
        if fy >= 1.5 * fx:
            return "FLAT-Y"
        return "SQUAT"
    return "CUBE"

def classify_pivot(originz, ez):
    zmin = originz - ez
    if abs(zmin) <= max(2.0, 0.05 * ez * 2):
        return "BASE"
    if abs(originz) <= max(2.0, 0.05 * ez * 2):
        return "CENTER"
    return "OTHER(zmin=%.0f)" % zmin

OUT = "%s/Saved/kit_bounds.txt" % unreal.Paths.project_dir().rstrip("/")

paths = []
for root in ROOTS:
    try:
        for a in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            paths.append(a)
    except Exception as e:
        unreal.log_error("%s list_assets(%s) FAILED: %r" % (TAG, root, e))

lines = []
n = 0
for ap in sorted(paths):
    try:
        obj = unreal.load_asset(ap)
        if not isinstance(obj, unreal.StaticMesh):
            continue
        b = obj.get_bounds()                      # BoxSphereBounds: origin + box_extent (half)
        ex, ey, ez = b.box_extent.x, b.box_extent.y, b.box_extent.z
        oz = b.origin.z
        zmin, zmax = oz - ez, oz + ez
        try:
            slots = len(obj.get_editor_property("static_materials"))
        except Exception:
            slots = -1
        shape = classify_shape(ex, ey, ez)
        pivot = classify_pivot(oz, ez)
        name = ap.split("/")[-1].split(".")[0]
        row = "%-34s | ext=%6.0f %6.0f %6.0f | oZ=%6.0f | z=[%6.0f,%6.0f] | slots=%d | %-7s | %s" % (
            name, ex * 2, ey * 2, ez * 2, oz, zmin, zmax, slots, shape, pivot)
        lines.append(row)
        unreal.log("%s %s" % (TAG, row))
        n += 1
    except Exception as e:
        unreal.log_error("%s mesh %s FAILED: %r" % (TAG, ap, e))

header = "# kit bounds (full extents cm) — %d static meshes\n" % n
with open(OUT, "w") as fh:
    fh.write(header + "\n".join(lines) + "\n")
unreal.log("%s DONE: %d static meshes dumped -> %s" % (TAG, n, OUT))
