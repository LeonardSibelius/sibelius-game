# fix_symbols_p5b.py — P5b touch-up pass on L_SymbolStudio_All
#
# Run this AFTER build_symbol_studio.py, in a LATER editor frame (i.e. just run
# it now, normally — the point is that the Text3D glyph has finished its
# deferred rebuild by the time this runs).
#
# WHY THIS EXISTS (banked lesson): Text3DComponent logs "Rebuild ... deferred
# to the next tick" at spawn — its final geometry does not exist until AFTER
# the spawning script returns. build_symbol_studio.py therefore measured
# bounds of, scaled, centred, and painted a glyph that was rebuilt out from
# under it one tick later (grey + mis-framed render). ANY Text3D configure-
# and-measure work needs a second pass in a later frame. This is that pass.
#
# What it does:
#   1. seven   — re-apply M_seven (setters + a direct hammer on every child
#                StaticMeshComponent), re-measure REAL bounds, re-scale,
#                re-centre, then recreate SEQ_seven from the final transform.
#   2. wild    — delete the halo cylinder (a solid face-on cylinder renders as
#                a ball that swallows the crown; purple crown alone reads
#                better). SEQ_wild binds wild_band — unaffected.
#   3. scatter — delete the disc blob; rebuild as a cyan 6-point sparkle burst
#                + 4 orbiting sparks; recreate SEQ_scatter.
#   4. saturn  — ring tilt roll 78 -> 58 (less fried-egg, more Saturn).
#   5. galaxy  — brighten the card (recreate M_galaxy_card at emissive 2.0,
#                reassign to the card).
#
# HOW TO RUN (editor Output Log Cmd box):
#   py "C:/Users/wpark/projects/sibelius-game/Content/SlotFactory/Scripts/fix_symbols_p5b.py"
# Then: MRQ — delete ALL jobs, re-add all 9 SEQ_*, SymbolStill preset, render.

import math
import unreal

ROOT = "/Game/SlotFactory"
MAT_DIR = ROOT + "/Materials"
TAG = "###P5B###"

FPS = 30
DURATION = 120
FRAME_FILL = 0.80
TARGET_SIZE = 175.0
SPACING = 1200.0
BASE_Z = 140.0

GALAXY_TINT = (0.80, 0.55, 1.10)
GALAXY_EMISSIVE = 2.0      # was 1.4 — brighter spiral on the reels
SATURN_RING_ROLL = 58.0    # was 78 — classic saturn ellipse

CENTER = {
    "star": 0, "moon": 1, "galaxy": 2, "saturn": 3, "mars": 4,
    "crown": 5, "seven": 6, "wild": 7, "scatter": 8,
}


def center_of(sid):
    return unreal.Vector(CENTER[sid] * SPACING, 0.0, BASE_Z)


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}

    def find(label):
        a = actors.get(label)
        if a is None:
            unreal.log_error("%s actor not found: %s" % (TAG, label))
        return a

    MESH = {
        "sphere": unreal.load_asset("/Engine/BasicShapes/Sphere"),
        "cone": unreal.load_asset("/Engine/BasicShapes/Cone"),
    }

    def add_part(sid, parent, mesh, loc, rot, scale, mat, label):
        a = eas.spawn_actor_from_object(mesh, loc, rot)
        a.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)
        if mat is not None:
            smc.set_material(0, mat)
        a.set_actor_label("%s_%s" % (sid, label))
        try:
            a.set_folder_path("SlotFactory/%s" % sid)
        except Exception:
            pass
        if parent is not None:
            a.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD,
                              unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
        return a

    def add_root_track(seq, cls):
        return seq.add_track(cls) if hasattr(seq, "add_track") else seq.add_master_track(cls)

    def recreate_sequence(sid, body, cam):
        full = "%s/SEQ_%s" % (ROOT, sid)
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        seq = asset_tools.create_asset("SEQ_" + sid, ROOT, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
        seq.set_display_rate(unreal.FrameRate(FPS, 1))
        seq.set_playback_start(0)
        seq.set_playback_end(DURATION)

        cam_binding = seq.add_possessable(cam)
        cut = add_root_track(seq, unreal.MovieSceneCameraCutTrack).add_section()
        cut.set_start_frame_seconds(0.0)
        cut.set_end_frame_seconds(DURATION / float(FPS))
        bid = unreal.MovieSceneObjectBindingID()
        bid.set_editor_property("guid", cam_binding.get_id())
        cut.set_camera_binding_id(bid)

        b = seq.add_possessable(body)
        ts = b.add_track(unreal.MovieScene3DTransformTrack).add_section()
        ts.set_start_frame_seconds(0.0)
        ts.set_end_frame_seconds(DURATION / float(FPS))
        loc, rot, scl = body.get_actor_location(), body.get_actor_rotation(), body.get_actor_scale3d()
        ch = ts.get_all_channels()
        f0, fN = unreal.FrameNumber(0), unreal.FrameNumber(DURATION)
        vals0 = [loc.x, loc.y, loc.z, rot.roll, rot.pitch, rot.yaw, scl.x, scl.y, scl.z]
        for i, v in enumerate(vals0):
            ch[i].add_key(f0, float(v))
        ch[5].add_key(fN, float(rot.yaw) + 360.0)
        eal.save_asset(full)
        unreal.log("%s recreated SEQ_%s" % (TAG, sid))
        return seq

    # ---- 1. SEVEN: finalize after the deferred Text3D rebuild --------------
    seven = find("seven_glyph")
    if seven:
        mat = unreal.load_asset(MAT_DIR + "/M_seven")
        comp = seven.get_component_by_class(unreal.Text3DComponent)
        hits = 0
        if comp:
            for setter in ("set_front_material", "set_back_material", "set_extrude_material", "set_bevel_material"):
                fn = getattr(comp, setter, None)
                if callable(fn):
                    try:
                        fn(mat)
                        hits += 1
                    except Exception as e:
                        unreal.log("%s seven %s failed: %s" % (TAG, setter, e))
        # the hammer: paint every child StaticMeshComponent directly — the
        # StaticMeshesRenderer's actual meshes, whatever the setters did.
        smcs = seven.get_components_by_class(unreal.StaticMeshComponent)
        for smc in smcs:
            try:
                n = smc.get_num_materials()
                for slot in range(max(n, 1)):
                    smc.set_material(slot, mat)
            except Exception as e:
                unreal.log("%s seven SMC paint failed: %s" % (TAG, e))
        unreal.log("%s seven: %d setters + %d child SMCs painted" % (TAG, hits, len(list(smcs))))

        # re-frame from REAL bounds (geometry is final now)
        origin, extent = seven.get_actor_bounds(False)
        if extent.z > 1.0:
            cur = seven.get_actor_scale3d().z
            desired_half = (TARGET_SIZE * FRAME_FILL) / 2.0
            s = cur * (desired_half / extent.z)
            seven.set_actor_scale3d(unreal.Vector(s, s, s))
            origin, extent = seven.get_actor_bounds(False)
            tgt = center_of("seven")
            seven.add_actor_world_offset(unreal.Vector(tgt.x - origin.x, tgt.y - origin.y, tgt.z - origin.z), False, False)
            origin, extent = seven.get_actor_bounds(False)
            unreal.log("%s seven: rescaled to %.2f, bounds %.0fx%.0fcm, recentred at (%.0f, %.0f, %.0f)" %
                       (TAG, s, extent.x * 2, extent.z * 2, origin.x, origin.y, origin.z))
            cam = find("CAM_seven")
            if cam:
                recreate_sequence("seven", seven, cam)
        else:
            unreal.log_error("%s seven bounds still degenerate — glyph never built?" % TAG)

    # ---- 2. WILD: delete the halo (solid cylinder = purple ball) -----------
    halo = actors.get("wild_halo")
    if halo:
        eas.destroy_actor(halo)
        unreal.log("%s wild: halo deleted (purple crown stands alone)" % TAG)
    else:
        unreal.log("%s wild: no halo found (already gone)" % TAG)

    # ---- 3. SCATTER: rebuild as a cyan sparkle burst ------------------------
    mat_sc = unreal.load_asset(MAT_DIR + "/M_scatter")
    for lbl in ("scatter_disc", "scatter_core", "scatter_spark0", "scatter_spark1", "scatter_spark2", "scatter_spark3"):
        a = actors.get(lbl)
        if a:
            eas.destroy_actor(a)
    c = center_of("scatter")
    body = add_part("scatter", None, MESH["sphere"], c, unreal.Rotator(roll=0, pitch=0, yaw=0), (0.42, 0.42, 0.42), mat_sc, "core")
    cone_scale = (0.13, 0.13, 0.55)
    offset_r = 21.0 + (100.0 * cone_scale[2]) / 2.0 - 8.0
    for i in range(6):
        theta = 60.0 * i
        rad = math.radians(theta)
        dx, dz = -math.sin(rad) * offset_r, math.cos(rad) * offset_r
        add_part("scatter", body, MESH["cone"], unreal.Vector(c.x + dx, c.y, c.z + dz),
                 unreal.Rotator(roll=0, pitch=theta, yaw=0), cone_scale, mat_sc, "ray%d" % i)
    for i, (dx, dz) in enumerate(((68, 40), (-64, 48), (60, -52), (-56, -42))):
        add_part("scatter", body, MESH["sphere"], unreal.Vector(c.x + dx, c.y, c.z + dz),
                 unreal.Rotator(roll=0, pitch=0, yaw=0), (0.13, 0.13, 0.13), mat_sc, "spark%d" % i)
    unreal.log("%s scatter: rebuilt as 6-ray sparkle burst" % TAG)
    cam = find("CAM_scatter")
    if cam:
        recreate_sequence("scatter", body, cam)

    # ---- 4. SATURN: relax the ring tilt -------------------------------------
    ring = actors.get("saturn_ring")
    if ring:
        ring.set_actor_rotation(unreal.Rotator(roll=SATURN_RING_ROLL, pitch=0, yaw=0), False)
        unreal.log("%s saturn: ring roll -> %.0f" % (TAG, SATURN_RING_ROLL))

    # ---- 5. GALAXY: brighter card -------------------------------------------
    card = actors.get("galaxy_card")
    tex = unreal.load_asset(ROOT + "/Textures/T_galaxy")
    if card and tex:
        full = "%s/M_galaxy_card" % MAT_DIR
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        mat = asset_tools.create_asset("M_galaxy_card", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("opacity_mask_clip_value", 0.06)
        ts2 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -800, 0)
        ts2.set_editor_property("texture", tex)

        def c3(rgb, x, y):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
            n.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
            return n

        def c1(v, x, y):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
            n.set_editor_property("r", v)
            return n

        tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -560, 0)
        mel.connect_material_expressions(ts2, "", tint, "A")
        mel.connect_material_expressions(c3(GALAXY_TINT, -800, 160), "", tint, "B")
        glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -380, 0)
        mel.connect_material_expressions(tint, "", glow, "A")
        mel.connect_material_expressions(c1(GALAXY_EMISSIVE, -560, 160), "", glow, "B")
        mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        lum = mel.create_material_expression(mat, unreal.MaterialExpressionDotProduct, -560, 320)
        mel.connect_material_expressions(ts2, "", lum, "A")
        mel.connect_material_expressions(c3((0.333, 0.333, 0.333), -800, 380), "", lum, "B")
        mel.connect_material_property(lum, "", unreal.MaterialProperty.MP_OPACITY_MASK)
        mel.recompile_material(mat)
        eal.save_asset(full)
        smc = card.get_component_by_class(unreal.StaticMeshComponent)
        smc.set_material(0, mat)
        unreal.log("%s galaxy: card brightened (emissive %.1f)" % (TAG, GALAXY_EMISSIVE))

    # ---- save ----------------------------------------------------------------
    try:
        les.save_current_level()
    except Exception:
        pass
    eal.save_directory(ROOT, True, True)
    unreal.log("%s DONE — re-render: MRQ, delete ALL jobs, re-add all 9 SEQ_*, SymbolStill preset." % TAG)


main()
