"""
build_statues_blender.py - the Archaic Language Temple's five statues, from SMK's scans to FBX.

Walt, 2026-10-19: "I would like the old languages to be represented by ancient greek statues -
immobile figures to contrast with the lively AI dancers." The scans are SMK's public-domain
plaster casts (SourceArt/Statues/SOURCES.md); Unreal does not read STL, so this turns each into an
FBX that import_statues.py brings in.

For each statue:
  - read the STL, and merge its triangle soup into one connected surface (an STL repeats every
    corner once per triangle; unmerged, nothing can be shaded smooth);
  - turn it upright (UP below: which of the scan's axes is height - Zeus is wider than he is tall,
    so the longest side is not always up);
  - scale it to the cast's own height in SMK's catalogue (the scans arrive in mixed units);
  - stand it on its base: origin at the centre of its footprint, on the floor;
  - shade it smooth, and export it in metres - 1 m here is 100 cm in Unreal.

    blender.exe -b -P Tools/Scripts/build_statues_blender.py              # build all five
    blender.exe -b -P Tools/Scripts/build_statues_blender.py -- --probe   # only print each scan's extents
"""
import glob
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "SourceArt", "Statues")
TAG = "###STATUE###"

# folder -> (asset name, the cast's gross height in metres from SMK's catalogue, the scan's up axis)
STATUES = {
    "FORTRAN_Discobolus": ("SM_Statue_FORTRAN_Discobolus", 1.68, "Z"),
    "COBOL_Doryphoros": ("SM_Statue_COBOL_Doryphoros", 2.13, "Z"),
    "BASIC_Kore": ("SM_Statue_BASIC_Kore", 2.05, "Z"),
    "Java_Zeus": ("SM_Statue_Java_Zeus", 2.01, "Z"),
    "Python_Laocoon": ("SM_Statue_Python_Laocoon", 2.42, "Z"),
}

# Rotations that bring the scan's up axis to Blender's +Z.
TO_Z_UP = {
    "Z": Matrix.Identity(4),
    "-Z": Matrix.Rotation(3.14159265, 4, "X"),
    "Y": Matrix.Rotation(1.5707963, 4, "X"),
    "-Y": Matrix.Rotation(-1.5707963, 4, "X"),
    "X": Matrix.Rotation(-1.5707963, 4, "Y"),
    "-X": Matrix.Rotation(1.5707963, 4, "Y"),
}


def read_stl(path):
    before = set(bpy.data.objects)
    bpy.ops.wm.stl_import(filepath=path)
    new = [ob for ob in bpy.data.objects if ob not in before]
    return new[0]


def extents(ob):
    xs = [v.co.x for v in ob.data.vertices]
    ys = [v.co.y for v in ob.data.vertices]
    zs = [v.co.z for v in ob.data.vertices]
    return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))


def build(folder, probe):
    name, height, up = STATUES[folder]
    stl = glob.glob(os.path.join(SRC, folder, "*.stl"))[0]
    ob = read_stl(stl)
    me = ob.data
    ob.data.transform(ob.matrix_world)   # whatever the importer applied, into the vertices
    ob.matrix_world = Matrix.Identity(4)

    (x0, x1), (y0, y1), (z0, z1) = extents(ob)
    print("%s %s raw: x %.1f y %.1f z %.1f  (%d verts, %d faces)" % (
        TAG, folder, x1 - x0, y1 - y0, z1 - z0, len(me.vertices), len(me.polygons)))
    if probe:
        bpy.data.objects.remove(ob)
        return

    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5 * max(x1 - x0, y1 - y0, z1 - z0))
    bm.to_mesh(me)
    bm.free()

    me.transform(TO_Z_UP[up])
    (x0, x1), (y0, y1), (z0, z1) = extents(ob)
    s = height / (z1 - z0)
    me.transform(Matrix.Translation((-(x0 + x1) / 2, -(y0 + y1) / 2, -z0)))
    me.transform(Matrix.Scale(s, 4))
    for poly in me.polygons:
        poly.use_smooth = True
    ob.name = me.name = name

    mat = bpy.data.materials.new("M_StatueMarble")
    me.materials.clear()
    me.materials.append(mat)

    (x0, x1), (y0, y1), (z0, z1) = extents(ob)
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    out = os.path.join(SRC, folder, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", bake_space_transform=True)
    print("%s %s -> %s: %.2f x %.2f x %.2f m, %d verts, %d faces" % (
        TAG, folder, os.path.basename(out), x1 - x0, y1 - y0, z1 - z0, len(me.vertices), len(me.polygons)))
    bpy.data.objects.remove(ob)


probe = "--probe" in (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
bpy.ops.wm.read_factory_settings(use_empty=True)
for folder in STATUES:
    build(folder, probe)
print(TAG, "DONE")
