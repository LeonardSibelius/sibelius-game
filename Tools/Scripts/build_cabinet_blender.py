"""
build_cabinet_blender.py - the Celestial Fortune cabinet, modelled in Blender.

Walt, 2026-10-16: "ok, build the custom cabinet in blender". The cathedral's machine had been a
stretched engine cube (build_cabinet_marquee.py) with a text sign floating over it. This is the
real thing, to docs/MARQUEE_SPEC.md: a midnight upright with a brass-bezelled screen tilted back
to the player, a button deck jutting out under it, light pipes up its front, the lit marquee on
top ("Celestial" over "FORTUNE"), and the maker's plate low on the door - Walt's name where a
manufacturer's goes. No real manufacturer's mark or cabinet design anywhere.

One mesh, its parts as material slots so Unreal can light each its own way:

    M_CabBody    the midnight lacquer          M_CabBrass   bezels, kick plate, door frame
    M_CabBlack   the deck's glass              M_CabGlow    the light pipes (cool accent)
    M_CabButton  the bet buttons (gold)        M_CabSpin    the SPIN button (heat red)
    M_CabScreen  T_CabScreen, the reels        M_CabMarquee T_Marquee_CelestialFortune
    M_CabBadge   T_CabBadge, the maker's plate

Built in metres, front facing +X, origin on the floor at the middle of its footprint; 1.00 m
wide, 0.92 deep with the deck, 2.48 tall. The painted panels come from make_cabinet_art.mjs.

Writes SourceArt/SlotCabinet/SM_CelestialCabinet.fbx and .blend, and two Cycles previews.

    blender.exe -b -P Tools/Scripts/build_cabinet_blender.py
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
ART = os.path.join(ROOT, "SourceArt", "SlotCabinet")

W = 1.00            # width (Blender Y)
HALF = W / 2.0

# The side silhouette, (x, z) in metres, front to the right: plinth, the deck jutting out,
# the screen face leaning back, the body's top.
PROFILE = [(-0.38, 0.00), (0.34, 0.00), (0.34, 0.88), (0.54, 0.93), (0.54, 0.99),
           (0.31, 1.05), (0.31, 1.10), (0.22, 1.95), (0.22, 2.00), (-0.38, 2.00)]
SCREEN_A, SCREEN_B = Vector((0.31, 0.0, 1.10)), Vector((0.22, 0.0, 1.95))   # the screen face
DECK_A, DECK_B = Vector((0.31, 0.0, 1.05)), Vector((0.54, 0.0, 0.99))       # rear -> front lip

Y = Vector((0.0, 1.0, 0.0))


def hexcol(h, a=1.0):
    h = h.lstrip("#")
    srgb = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb]
    return (*lin, a)


# --- materials -----------------------------------------------------------------------------

def material(name, color="#000000", metallic=0.0, roughness=0.5, emit=None, strength=0.0, image=None,
             image_emits=False):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = hexcol(color)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emit:
        bsdf.inputs["Emission Color"].default_value = hexcol(emit)
        bsdf.inputs["Emission Strength"].default_value = strength
    if image:
        tex = m.node_tree.nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(os.path.join(ART, image))
        m.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
        if image_emits:
            m.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = strength
    return m


# Start from an empty scene before any material exists (a reset afterwards would delete them).
bpy.ops.wm.read_factory_settings(use_empty=True)

MATS = {
    "body": material("M_CabBody", "#12142C", metallic=0.55, roughness=0.32),
    "brass": material("M_CabBrass", "#D4A836", metallic=1.0, roughness=0.35),
    "black": material("M_CabBlack", "#050508", metallic=0.0, roughness=0.12),
    "glow": material("M_CabGlow", "#C7EEFF", emit="#C7EEFF", strength=4.0),
    "button": material("M_CabButton", "#FFD24D", emit="#FFD24D", strength=1.6),
    "spin": material("M_CabSpin", "#D92E10", emit="#E8320E", strength=2.2),
    "screen": material("M_CabScreen", "#000000", roughness=0.08, image="T_CabScreen.png", image_emits=True,
                       strength=0.9),
    "marquee": material("M_CabMarquee", "#000000", roughness=0.1, image="T_Marquee_CelestialFortune.png",
                        image_emits=True, strength=1.1),
    "badge": material("M_CabBadge", metallic=1.0, roughness=0.35, image="T_CabBadge.png"),
}


# --- geometry helpers ------------------------------------------------------------------------

def new_object(name, bm, mat):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(MATS[mat])
    return ob


def obox(name, center, u, v, n, hu, hv, hn, mat):
    """A box on its own axes: u up its face, v across, n out of it; half sizes along each."""
    bm = bmesh.new()
    vs = {}
    for su in (-1, 1):
        for sv in (-1, 1):
            for sn in (-1, 1):
                vs[(su, sv, sn)] = bm.verts.new(center + u * (su * hu) + v * (sv * hv) + n * (sn * hn))
    for quad in (((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)),
                 ((-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)),
                 ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)),
                 ((-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1)),
                 ((-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)),
                 ((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1))):
        bm.faces.new([vs[k] for k in quad])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm, mat)


def panel(name, center, u, v, n, hu, hv, mat):
    """A textured quad facing n: UV (0,0) bottom-left as the viewer sees it, (1,1) top-right."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    # The viewer looks along -n with u up, so their right hand points along u x n.
    right = u.cross(n).normalized()
    corners = [center - u * hu - right * hv, center - u * hu + right * hv,
               center + u * hu + right * hv, center + u * hu - right * hv]
    f = bm.faces.new([bm.verts.new(p) for p in corners])
    if f.normal.dot(n) < 0:
        f.normal_flip()
    for loop in f.loops:
        p = loop.vert.co
        d = p - center
        loop[uv].uv = (0.5 + d.dot(right) / (2 * hv), 0.5 + d.dot(u) / (2 * hu))
    return new_object(name, bm, mat)


def frame(name, center, u, v, n, hu, hv, bar, depth, mat):
    """A bezel: four bars round a rectangle of half sizes (hu, hv), each bar wide, depth proud."""
    parts = [obox(f"{name}_t", center + u * (hu + bar / 2), u, v, n, bar / 2, hv + bar, depth / 2, mat),
             obox(f"{name}_b", center - u * (hu + bar / 2), u, v, n, bar / 2, hv + bar, depth / 2, mat),
             obox(f"{name}_l", center + v * (hv + bar / 2), u, v, n, hu, bar / 2, depth / 2, mat),
             obox(f"{name}_r", center - v * (hv + bar / 2), u, v, n, hu, bar / 2, depth / 2, mat)]
    return parts


def cylinder(name, center, axis, radius, half, mat, segments=32):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=half * 2)
    rot = Vector((0, 0, 1)).rotation_difference(axis).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=rot, verts=bm.verts)
    bmesh.ops.translate(bm, vec=center, verts=bm.verts)
    return new_object(name, bm, mat)


def face_axes(a, b):
    """Up the face from a to b, and its outward normal (front is +X)."""
    u = (b - a).normalized()
    n = Vector((u.z, 0.0, -u.x)).normalized()
    if n.x < 0:
        n = -n
    return u, n


# --- the cabinet -----------------------------------------------------------------------------

def build():
    parts = []

    # The body: the side silhouette pushed across the width, its edges softened.
    bm = bmesh.new()
    left = [bm.verts.new((x, -HALF, z)) for x, z in PROFILE]
    right = [bm.verts.new((x, HALF, z)) for x, z in PROFILE]
    bm.faces.new(left[::-1])
    bm.faces.new(right)
    k = len(PROFILE)
    for i in range(k):
        j = (i + 1) % k
        bm.faces.new([left[i], left[j], right[j], right[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    body = new_object("Body", bm, "body")
    bev = body.modifiers.new("Bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method, bev.harden_normals = 0.012, 3, "ANGLE", True
    parts.append(body)

    X, Z = Vector((1, 0, 0)), Vector((0, 0, 1))

    # A brass edge up the front of each side, tracing the silhouette: the chrome bezel.
    for side in (-1, 1):
        for i in range(1, 8):
            a = Vector((PROFILE[i][0], 0, PROFILE[i][1]))
            b = Vector((PROFILE[i + 1][0], 0, PROFILE[i + 1][1]))
            eu = (b - a).normalized()
            en = Vector((eu.z, 0, -eu.x))
            parts.append(obox(f"Edge{side}_{i}", (a + b) / 2 + Y * side * (HALF + 0.004) - en * 0.006, eu, Y, en,
                              (b - a).length / 2 + 0.006, 0.005, 0.012, "brass"))

    # Brass kick plate round the plinth.
    parts.append(obox("Kick", Vector((-0.02, 0, 0.03)), Z, Y, X, 0.03, HALF + 0.006, 0.365, "brass"))

    # The screen, leaning back: brass bezel, the reels, light pipes up both edges.
    su, sn = face_axes(SCREEN_A, SCREEN_B)
    length = (SCREEN_B - SCREEN_A).length
    sc = SCREEN_A + su * (length * 0.52) + sn * 0.004
    hu, hv = 0.30, 0.40                                   # 0.80 wide x 0.60 tall: the art's 4:3
    parts.append(panel("Screen", sc + sn * 0.002, su, Y, sn, hu, hv, "screen"))
    parts += frame("ScreenBezel", sc + sn * 0.010, su, Y, sn, hu, hv, 0.035, 0.022, "brass")
    for side in (-1, 1):
        mid = SCREEN_A + su * (length / 2) + sn * 0.006 + Y * side * 0.478
        parts.append(obox(f"PipeUpper{side}", mid, su, Y, sn, length / 2 - 0.02, 0.010, 0.008, "glow"))
        parts.append(obox(f"PipeLower{side}", Vector((0.344, side * 0.478, 0.47)), Z, Y, X, 0.38, 0.010, 0.008,
                          "glow"))

    # The deck: black glass, five bet buttons and SPIN, a brass lip along its front.
    du, dn = face_axes(DECK_A, DECK_B)
    dn = Vector((-dn.x, 0, -dn.z)) if dn.z < 0 else dn
    deck_len = (DECK_B - DECK_A).length
    dc = DECK_A + du * (deck_len / 2) + dn * 0.003
    parts.append(obox("DeckGlass", dc, du, Y, dn, deck_len / 2 - 0.02, HALF - 0.04, 0.003, "black"))
    parts.append(obox("DeckLip", Vector((0.548, 0, 0.962)), Z, Y, X, 0.032, HALF, 0.010, "brass"))
    bc = DECK_A + du * (deck_len * 0.5) + dn * 0.010
    for i in range(5):
        parts.append(obox(f"Bet{i}", bc + Y * (-0.36 + i * 0.12), du, Y, dn, 0.035, 0.045, 0.008, "button"))
    parts.append(cylinder("Spin", bc + Y * 0.30 + dn * 0.006, dn, 0.055, 0.012, "spin"))
    parts.append(cylinder("SpinRing", bc + Y * 0.30 + dn * 0.002, dn, 0.066, 0.008, "brass"))

    # The door below: a thin brass frame, and the maker's plate at 45 cm.
    door_c = Vector((0.343, 0, 0.47))
    parts += frame("DoorFrame", door_c, Z, Y, X, 0.34, 0.39, 0.012, 0.010, "brass")
    parts.append(obox("BadgePlate", Vector((0.343, 0, 0.45)), Z, Y, X, 0.025, 0.085, 0.004, "brass"))
    parts.append(panel("Badge", Vector((0.348, 0, 0.45)), Z, Y, X, 0.020, 0.080, "badge"))

    # The bill validator, upper right of the door: a black mouth in a brass surround.
    parts.append(obox("BillSurround", Vector((0.346, 0.24, 0.70)), Z, Y, X, 0.07, 0.06, 0.006, "brass"))
    parts.append(obox("BillMouth", Vector((0.351, 0.24, 0.70)), Z, Y, X, 0.012, 0.045, 0.004, "black"))

    # The top box: the marquee, lit, in its brass bezel, flush over the screen's top.
    parts.append(obox("TopBox", Vector((0.08, 0, 2.24)), Z, Y, X, 0.24, 0.48, 0.16, "body"))
    mc = Vector((0.242, 0, 2.24))
    parts.append(panel("Marquee", mc, Z, Y, X, 0.225, 0.45, "marquee"))
    parts += frame("MarqueeBezel", mc + X * 0.006, Z, Y, X, 0.225, 0.45, 0.03, 0.016, "brass")

    # One mesh, its parts as material slots.
    bpy.ops.object.select_all(action="DESELECT")
    for ob in parts:
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        for mod in list(ob.modifiers):
            bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    cab = bpy.context.view_layer.objects.active
    cab.name = cab.data.name = "SM_CelestialCabinet"
    return cab


def unwrap_untextured(cab):
    """Give the plain parts UVs (the panels keep their exact ones) so Unreal is happy."""
    bpy.context.view_layer.objects.active = cab
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(cab.data)
    textured = {i for i, m in enumerate(cab.data.materials) if m.name in ("M_CabScreen", "M_CabMarquee", "M_CabBadge")}
    for f in bm.faces:
        f.select = f.material_index not in textured
    bmesh.update_edit_mesh(cab.data)
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.01)
    bpy.ops.object.mode_set(mode="OBJECT")


def preview(cab):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 96
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y = 1200, 1500
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("Night")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = hexcol("#05050C")
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    scene.world = world

    floor = bpy.data.meshes.new("Floor")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=6)
    bm.to_mesh(floor)
    bm.free()
    fl = bpy.data.objects.new("Floor", floor)
    scene.collection.objects.link(fl)
    fl.data.materials.append(material("Floor", "#151518", roughness=0.25))

    def light(name, loc, energy, size, color="#FFFFFF"):
        ld = bpy.data.lights.new(name, "AREA")
        ld.energy, ld.size, ld.color = energy, size, hexcol(color)[:3]
        ob = bpy.data.objects.new(name, ld)
        ob.location = loc
        scene.collection.objects.link(ob)
        ob.rotation_euler = (Vector((0, 0, 1.2)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()

    light("Key", (2.6, -2.2, 2.6), 600, 2.0, "#FFE8C8")
    light("Rim", (-1.8, 2.4, 2.8), 500, 1.5, "#9DB8FF")
    light("Fill", (2.8, 2.0, 1.0), 120, 2.0)

    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 45
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    shots = {"preview_threequarter.png": (3.6, -2.9, 1.75), "preview_front.png": (4.4, 0.0, 1.35)}
    for name, loc in shots.items():
        cam.location = loc
        cam.rotation_euler = (Vector((0, 0, 1.22)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(ART, name)
        bpy.ops.render.render(write_still=True)
    fl.hide_render = True


def main():
    cab = build()
    unwrap_untextured(cab)
    bpy.ops.object.select_all(action="DESELECT")
    cab.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(ART, "SM_CelestialCabinet.fbx"), use_selection=True,
                             apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE", path_mode="STRIP",
                             bake_space_transform=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ART, "SM_CelestialCabinet.blend"))
    preview(cab)
    me = cab.data
    print("###CAB### %d faces, %d material slots, dims %s" % (len(me.polygons), len(me.materials),
                                                          tuple(round(d, 3) for d in cab.dimensions)))


main()
