# build_symbol_studio.py — Celestial Fortune Symbol Studio production pass (P3a)
#
# Programmatically builds, for each of the 9 model symbols (star, moon, galaxy,
# saturn, mars, crown, seven, wild, scatter):
#   1. geometry from engine basic shapes (Sphere/Cube/Cylinder/Cone) — distinct,
#      recognizable silhouettes (no plugin deps; ★ glyphs don't extrude so the
#      star is a primitive starburst);
#   2. a material M_<id> in the M_SlotGold *recipe family* (metallic + emissive,
#      celestial palette) — standalone so it doesn't depend on M_SlotGold's
#      internal params; distinct colour identity per symbol;
#   3. a level sequence SEQ_<id> matching SEQ_Coin's framing (square 24×24
#      filmback, symbol ~80% of frame) with a slow 360° rotation.
#
# All 9 symbols live in ONE level (L_SymbolStudio_All) at spaced locations, each
# with its own camera framing only its symbol. Asset names use the model symbol
# id EXACTLY. Content-only — no gates.
#
# HOW TO RUN: see the step-by-step in the chat message. In short, from the editor
# Output Log:  py "C:/Users/wpark/projects/sibelius-game/Content/SlotFactory/Scripts/build_symbol_studio.py"
#
# Re-runnable: with RESET=True it deletes and rebuilds the level + M_/SEQ_ assets.

import unreal

# ----------------------------------------------------------------------------
# config
# ----------------------------------------------------------------------------
ROOT = "/Game/SlotFactory"
MAT_DIR = ROOT + "/Materials"
LEVEL_PATH = ROOT + "/L_SymbolStudio_All"

RESET = True
SPACING = 1200.0       # cm between symbols along X (keeps neighbours out of frame)
BASE_Z = 140.0         # symbols float above the (empty) origin
FPS = 30
DURATION = 120         # frames -> 4 s, one slow 360° turn
FILMBACK = 24.0        # square sensor, matches SEQ_Coin
CAM_DIST = 460.0       # cm from symbol centre
FRAME_FILL = 0.80      # symbol fills ~80% of the square frame
TARGET_SIZE = 175.0    # cm the frame height spans (symbol ~ FRAME_FILL of this)
FOCAL = (CAM_DIST / TARGET_SIZE) * FILMBACK  # ~63mm

# id, base colour (linear rgb), emissive strength, metallic, roughness, builder key
SYMBOLS = [
    ("star",    (1.00, 0.80, 0.22), 2.5, 1.0, 0.20, "star"),
    ("moon",    (0.82, 0.86, 1.00), 0.6, 0.25, 0.55, "moon"),
    ("galaxy",  (0.55, 0.32, 0.95), 2.2, 0.30, 0.40, "galaxy"),
    ("saturn",  (0.92, 0.74, 0.38), 0.8, 0.85, 0.30, "saturn"),
    ("mars",    (0.85, 0.26, 0.09), 0.9, 0.25, 0.65, "mars"),
    ("crown",   (1.00, 0.84, 0.30), 1.4, 1.0, 0.18, "crown"),
    ("seven",   (0.92, 0.11, 0.18), 1.6, 0.60, 0.30, "seven"),
    ("wild",    (0.72, 0.32, 1.00), 3.0, 1.0, 0.22, "wild"),
    ("scatter", (0.20, 0.72, 1.00), 2.8, 0.40, 0.35, "scatter"),
]

TAG = "###STUDIO###"


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary

    MESH = {
        "sphere": unreal.load_asset("/Engine/BasicShapes/Sphere"),
        "cube": unreal.load_asset("/Engine/BasicShapes/Cube"),
        "cylinder": unreal.load_asset("/Engine/BasicShapes/Cylinder"),
        "cone": unreal.load_asset("/Engine/BasicShapes/Cone"),
    }
    for k, m in MESH.items():
        if m is None:
            unreal.log_error("%s missing engine basic shape: %s" % (TAG, k))
            return

    # -- fresh level --------------------------------------------------------
    if RESET and eal.does_asset_exist(LEVEL_PATH):
        eal.delete_asset(LEVEL_PATH)
    les.new_level(LEVEL_PATH)
    world = ues.get_editor_world()
    unreal.log("%s new level: %s" % (TAG, LEVEL_PATH))

    # even, position-independent lighting (two directionals) so metallic shape
    # reads across every symbol; the empty level keeps the bg transparent.
    key = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 600), unreal.Rotator(-50, -40, 0))
    kc = key.get_component_by_class(unreal.DirectionalLightComponent)
    kc.set_mobility(unreal.ComponentMobility.MOVABLE)
    kc.set_editor_property("intensity", 6.0)
    fill = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 600), unreal.Rotator(-18, 135, 0))
    fc = fill.get_component_by_class(unreal.DirectionalLightComponent)
    fc.set_mobility(unreal.ComponentMobility.MOVABLE)
    fc.set_editor_property("intensity", 2.5)
    fc.set_light_color(unreal.LinearColor(0.7, 0.8, 1.0, 1.0))

    # -- helpers ------------------------------------------------------------
    def make_material(sid, color, emissive, metallic, rough):
        full = "%s/M_%s" % (MAT_DIR, sid)
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        mat = asset_tools.create_asset("M_" + sid, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -450, 0)
        col.set_editor_property("constant", unreal.LinearColor(color[0], color[1], color[2], 1.0))
        mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
        met = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -450, 160)
        met.set_editor_property("r", metallic)
        mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
        rgh = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -450, 250)
        rgh.set_editor_property("r", rough)
        mel.connect_material_property(rgh, "", unreal.MaterialProperty.MP_ROUGHNESS)
        es = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -450, 360)
        es.set_editor_property("r", emissive)
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 300)
        mel.connect_material_expressions(col, "", mul, "A")
        mel.connect_material_expressions(es, "", mul, "B")
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(mat)
        eal.save_asset(full)
        return mat

    def add_part(sid, parent, mesh, loc, rot, scale, mat, label):
        a = eas.spawn_actor_from_object(mesh, loc, rot)
        a.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)  # so the sequencer can spin it
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

    def V(c, x=0.0, y=0.0, z=0.0):
        return unreal.Vector(c.x + x, c.y + y, c.z + z)

    # -- per-symbol geometry (returns the body actor used for the spin) -----
    def build_star(sid, c, mat):
        body = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (0.55, 0.55, 0.55), mat, "core")
        for i in range(5):
            add_part(sid, body, MESH["cone"], V(c), unreal.Rotator(72.0 * i, 0, 0), (0.28, 0.28, 0.85), mat, "point%d" % i)
        return body

    def build_moon(sid, c, mat):
        return add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (1.5, 1.5, 1.5), mat, "body")

    def build_mars(sid, c, mat):
        return add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (1.4, 1.4, 1.4), mat, "body")

    def build_saturn(sid, c, mat):
        body = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (0.95, 0.95, 0.95), mat, "planet")
        # ring: flattened cylinder disc, tilted so it reads as an ellipse
        add_part(sid, body, MESH["cylinder"], V(c), unreal.Rotator(0, 0, 78.0), (1.6, 1.6, 0.05), mat, "ring")
        return body

    def build_crown_parts(sid, c, mat):
        band = add_part(sid, None, MESH["cube"], V(c, z=-15), unreal.Rotator(0, 0, 0), (1.4, 0.55, 0.45), mat, "band")
        for i, x in enumerate((-58, -29, 0, 29, 58)):
            h = 0.8 if i == 2 else 0.6
            add_part(sid, band, MESH["cone"], V(c, x=x, z=40), unreal.Rotator(0, 0, 0), (0.28, 0.28, h), mat, "spike%d" % i)
        return band

    def build_crown(sid, c, mat):
        return build_crown_parts(sid, c, mat)

    def build_seven(sid, c, mat):
        bar = add_part(sid, None, MESH["cube"], V(c, x=0, z=48), unreal.Rotator(0, 0, 0), (1.0, 0.18, 0.16), mat, "bar")
        add_part(sid, bar, MESH["cube"], V(c, x=10, z=-12), unreal.Rotator(20.0, 0, 0), (0.16, 0.18, 0.95), mat, "stem")
        return bar

    def build_galaxy(sid, c, mat):
        disc = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 68.0), (1.55, 1.55, 0.22), mat, "disc")
        add_part(sid, disc, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (0.45, 0.45, 0.45), mat, "core")
        return disc

    def build_wild(sid, c, mat):
        band = build_crown_parts(sid, c, mat)
        # glow halo ring facing camera, behind the crown
        add_part(sid, band, MESH["cylinder"], V(c, y=40, z=15), unreal.Rotator(0, 0, 90.0), (1.7, 1.7, 0.05), mat, "halo")
        return band

    def build_scatter(sid, c, mat):
        disc = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 68.0), (1.45, 1.45, 0.22), mat, "disc")
        add_part(sid, disc, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (0.4, 0.4, 0.4), mat, "core")
        for i, (dx, dz) in enumerate(((70, 35), (-65, 45), (60, -50), (-55, -40))):
            add_part(sid, disc, MESH["sphere"], V(c, x=dx, z=dz), unreal.Rotator(0, 0, 0), (0.16, 0.16, 0.16), mat, "spark%d" % i)
        return disc

    BUILDERS = {
        "star": build_star, "moon": build_moon, "galaxy": build_galaxy, "saturn": build_saturn,
        "mars": build_mars, "crown": build_crown, "seven": build_seven, "wild": build_wild,
        "scatter": build_scatter,
    }

    def add_root_track(seq, cls):
        return seq.add_track(cls) if hasattr(seq, "add_track") else seq.add_master_track(cls)

    def make_sequence(sid, center, body, cam):
        full = "%s/SEQ_%s" % (ROOT, sid)
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        seq = asset_tools.create_asset("SEQ_" + sid, ROOT, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
        seq.set_display_rate(unreal.FrameRate(FPS, 1))
        seq.set_playback_start(0)
        seq.set_playback_end(DURATION)

        # camera cut -> this symbol's camera
        cam_binding = seq.add_possessable(cam)
        cut = add_root_track(seq, unreal.MovieSceneCameraCutTrack).add_section()
        cut.set_start_frame_seconds(0.0)
        cut.set_end_frame_seconds(DURATION / float(FPS))
        bid = unreal.MovieSceneObjectBindingID()
        bid.set_editor_property("guid", cam_binding.get_id())
        cut.set_camera_binding_id(bid)

        # slow 360° yaw on the body (attached parts follow)
        b = seq.add_possessable(body)
        ts = b.add_track(unreal.MovieScene3DTransformTrack).add_section()
        ts.set_start_frame_seconds(0.0)
        ts.set_end_frame_seconds(DURATION / float(FPS))
        loc, rot, scl = body.get_actor_location(), body.get_actor_rotation(), body.get_actor_scale3d()
        ch = ts.get_all_channels()  # [Lx,Ly,Lz, Rroll,Rpitch,Ryaw, Sx,Sy,Sz]
        f0, fN = unreal.FrameNumber(0), unreal.FrameNumber(DURATION)
        vals0 = [loc.x, loc.y, loc.z, rot.roll, rot.pitch, rot.yaw, scl.x, scl.y, scl.z]
        for i, v in enumerate(vals0):
            ch[i].add_key(f0, float(v))
        ch[5].add_key(fN, float(rot.yaw) + 360.0)  # yaw sweep

        eal.save_asset(full)
        return seq

    # -- build each symbol --------------------------------------------------
    built = []
    for idx, (sid, color, emissive, metallic, rough, key_name) in enumerate(SYMBOLS):
        try:
            center = unreal.Vector(idx * SPACING, 0.0, BASE_Z)
            mat = make_material(sid, color, emissive, metallic, rough)
            body = BUILDERS[key_name](sid, center, mat)

            cam_loc = unreal.Vector(center.x, center.y - CAM_DIST, center.z)
            cam_rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, center)
            cam = eas.spawn_actor_from_class(unreal.CineCameraActor, cam_loc, cam_rot)
            cam.set_actor_label("CAM_%s" % sid)
            try:
                cam.set_folder_path("SlotFactory/%s" % sid)
            except Exception:
                pass
            ccc = cam.get_cine_camera_component()
            fb = ccc.get_editor_property("filmback")
            fb.set_editor_property("sensor_width", FILMBACK)
            fb.set_editor_property("sensor_height", FILMBACK)
            ccc.set_editor_property("filmback", fb)
            ccc.set_editor_property("current_focal_length", FOCAL)

            make_sequence(sid, center, body, cam)
            unreal.log("%s built %-8s  M_%s + SEQ_%s" % (TAG, sid, sid, sid))
            built.append(sid)
        except Exception as ex:
            unreal.log_error("%s FAILED %s: %s" % (TAG, sid, ex))

    # -- save ---------------------------------------------------------------
    try:
        les.save_current_level()
    except Exception:
        unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    eal.save_directory(ROOT, True, True)

    unreal.log("%s DONE  built %d/9: %s" % (TAG, len(built), ", ".join(built)))
    unreal.log("%s focal=%.1fmm  filmback=%.0fx%.0f  camDist=%.0f" % (TAG, FOCAL, FILMBACK, FILMBACK, CAM_DIST))
    unreal.log("%s next: open %s, scrub each SEQ_<id>, then MRQ-render with the coin recipe." % (TAG, LEVEL_PATH))


main()
