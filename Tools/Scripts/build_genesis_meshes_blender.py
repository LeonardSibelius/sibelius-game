"""
build_genesis_meshes_blender.py - the two shapes the AI city's lattice is built from.

Walt, 2026-10-18: "glowing geodesic lattice. i like it. go ahead and touch the game." The city's
shells become a Matrioshka brain - a web of struts and nodes round the star, thought running down
the struts - in place of the crates. AAICity::LayLattice places thousands of each:

    SM_GenesisStrut  a six-sided rod 1 m long and 1 m across, along +X, open at the ends (a node
                     covers each). Its U coordinate runs 0 to 1 along its length on every face, so
                     M_GenesisStrut can run a comet the whole length of a strut.
    SM_GenesisNode   a faceted ball 1 m across (an icosphere, 80 faces): cheap, and its facets
                     catch the glow.

Both a metre, like the engine's basic shapes, so the city scales them the same way. FBX into
SourceArt/Genesis; make_genesis_assets.py imports them.

    blender.exe -b -P Tools/Scripts/build_genesis_meshes_blender.py
"""
import math
import os

import bmesh
import bpy

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "SourceArt", "Genesis")


def export(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    mat = bpy.data.materials.new("M_" + name)
    ob.data.materials.append(mat)
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"), use_selection=True,
                             apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE",
                             bake_space_transform=True)
    print("###GMESH### %s: %d faces" % (name, len(me.polygons)))


def strut():
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    sides = 6
    ring = []
    for end in (-0.5, 0.5):
        ring.append([bm.verts.new((end, 0.5 * math.cos(2 * math.pi * k / sides), 0.5 * math.sin(2 * math.pi * k / sides)))
                     for k in range(sides)])
    for k in range(sides):
        j = (k + 1) % sides
        # Wound so the face points outward; U along the length, V round the rod.
        f = bm.faces.new((ring[0][k], ring[1][k], ring[1][j], ring[0][j]))
        for loop in f.loops:
            x = loop.vert.co.x
            around = k if loop.vert in (ring[0][k], ring[1][k]) else k + 1
            loop[uv].uv = (x + 0.5, around / sides)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # Recalculating may flip faces; keep U along +X whichever way they wind.
    for f in bm.faces:
        for loop in f.loops:
            loop[uv].uv = (loop.vert.co.x + 0.5, loop[uv].uv[1])
    export("SM_GenesisStrut", bm)


def node():
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=0.5)
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for loop in f.loops:
            p = loop.vert.co
            loop[uv].uv = (0.5 + math.atan2(p.y, p.x) / (2 * math.pi), 0.5 + math.asin(max(-1.0, min(1.0, p.z / 0.5))) / math.pi)
    export("SM_GenesisNode", bm)


bpy.ops.wm.read_factory_settings(use_empty=True)
os.makedirs(OUT, exist_ok=True)
strut()
node()
