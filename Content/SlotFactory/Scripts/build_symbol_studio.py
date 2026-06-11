# build_symbol_studio.py — Celestial Fortune Symbol Studio (P3a build + P4 theme + P5 art pass)
#
# P5 (June 10, 2026): seven rebuilt (loud material setters + bounds-based
# framing — the P4 seven shipped GREY + mis-framed); star rebuilt as a true
# camera-facing 5-point; galaxy = NASA-texture emissive card (disc fallback);
# ROTATOR FIX on every disc tilt (positional args are roll,pitch,yaw — the P4
# tilts landed in the wrong slots, so saturn ring / galaxy / scatter / wild
# halo all rendered edge-on); emissive rebalanced 1.4-2.4 -> 0.25-0.55 + a
# cubemap SkyLight so metallic gold finally reads as gold.
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

# P5: galaxy is a textured emissive card (NASA public-domain face-on spiral).
# Drop the image in Downloads under one of these names; it's imported to
# /Game/SlotFactory/Textures/T_galaxy each run. Missing file -> loud log +
# the primitive-disc fallback (now correctly tilted).
GALAXY_TEXTURE_CANDIDATES = [
    "C:/Users/wpark/Downloads/galaxy_m101.png",
    "C:/Users/wpark/Downloads/galaxy_m101.jpg",
    "C:/Users/wpark/Downloads/galaxy.png",
    "C:/Users/wpark/Downloads/galaxy.jpg",
]
GALAXY_TINT = (0.80, 0.55, 1.10)   # push the photo toward the celestial purple identity
GALAXY_EMISSIVE = 1.4              # card brightness (unlit material)
SKYLIGHT_INTENSITY = 1.0           # SY7: keep low so golds stay warm

# P4 theme knobs. Per symbol: color = base linear rgb; em = emissive strength;
# met/rough = PBR; optional rim = (rgb, strength) Fresnel rim light; optional
# mottle = (rgb, noiseScale, strength) world-space noise that varies base colour
# (moon craters / mars mottling). A warm GOLD_RIM unifies the set as one
# "celestial gold" family; moon keeps a cool rim per its icy identity. Emissive
# is balanced so every symbol reads at reel-cell (~100px) size without blowing out.
GOLD_RIM = (1.00, 0.86, 0.52)
# P5 emissive rebalance (SY6): the P4 values (1.4-2.4) were UNSHADED glow that
# flattened every metallic form into a pastel cutout — that's why star/wild/
# scatter/galaxy read flat and the moon's craters vanished. Emissive now sits
# at 0.25-0.55 (accent, not paint) and a SkyLight gives metallic something to
# reflect, so the gold reads as gold.
SYMBOLS = [
    dict(id="star",    builder="star",    color=(1.00, 0.80, 0.25), em=0.45, met=1.0,  rough=0.22, rim=(GOLD_RIM, 0.6)),
    dict(id="moon",    builder="moon",    color=(0.80, 0.84, 0.92), em=0.25, met=0.15, rough=0.60,
         mottle=((0.20, 0.22, 0.30), 0.030, 0.85), rim=((0.75, 0.86, 1.00), 0.9)),
    dict(id="galaxy",  builder="galaxy",  color=(0.55, 0.32, 0.95), em=0.50, met=0.30, rough=0.40, rim=(GOLD_RIM, 0.5)),
    dict(id="saturn",  builder="saturn",  color=(0.95, 0.76, 0.40), em=0.30, met=0.85, rough=0.30, rim=(GOLD_RIM, 0.5)),
    # polar = (iceColor, smoothstep lo, hi on normal.Z, edgeNoiseAmt, noiseFreq).
    # Small cap (top ~10%) with a noise-broken, feathered edge — a frosty patch,
    # not a clean latitude line.
    dict(id="mars",    builder="mars",    color=(0.70, 0.18, 0.06), em=0.30, met=0.20, rough=0.70,
         mottle=((0.16, 0.04, 0.02), 0.035, 0.85),
         polar=((0.84, 0.91, 1.00), 0.78, 0.88, 0.10, 0.05),
         rim=((1.00, 0.62, 0.36), 0.5)),
    dict(id="crown",   builder="crown",   color=(1.00, 0.84, 0.32), em=0.40, met=1.0,  rough=0.18, rim=(GOLD_RIM, 0.6)),
    dict(id="seven",   builder="seven",   color=(1.00, 0.80, 0.30), em=0.45, met=1.0,  rough=0.20, rim=(GOLD_RIM, 0.6)),
    dict(id="wild",    builder="wild",    color=(0.72, 0.32, 1.00), em=0.55, met=1.0,  rough=0.22, rim=(GOLD_RIM, 0.7)),
    dict(id="scatter", builder="scatter", color=(0.20, 0.72, 1.00), em=0.50, met=0.40, rough=0.35, rim=(GOLD_RIM, 0.5)),
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
        "plane": unreal.load_asset("/Engine/BasicShapes/Plane"),
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
    # Defensive clean slate: if the level couldn't be fully reset (e.g. it was the
    # OPEN editor level, so delete_asset/new_level silently left it as-is), destroy
    # every actor already present so NO stale geometry — like an old mars_icecap
    # cap sphere from an earlier run — survives into the rebuild.
    stale = list(eas.get_all_level_actors())
    for a in stale:
        try:
            eas.destroy_actor(a)
        except Exception:
            pass
    unreal.log("%s new level: %s (cleared %d pre-existing actors)" % (TAG, LEVEL_PATH, len(stale)))

    # even, position-independent lighting (two directionals) so metallic shape
    # reads across every symbol; the empty level keeps the bg transparent.
    # SY3 fix: these rotations were positional too — the key got roll=-50 (a
    # no-op on a directional light) and the FILL got pitch=135, i.e. pointing
    # UP from below. Keyword args aim them as intended.
    key = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 600), unreal.Rotator(roll=0, pitch=-50, yaw=-40))
    kc = key.get_component_by_class(unreal.DirectionalLightComponent)
    kc.set_mobility(unreal.ComponentMobility.MOVABLE)
    kc.set_editor_property("intensity", 6.0)
    fill = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 600), unreal.Rotator(roll=0, pitch=-18, yaw=135))
    fc = fill.get_component_by_class(unreal.DirectionalLightComponent)
    fc.set_mobility(unreal.ComponentMobility.MOVABLE)
    fc.set_editor_property("intensity", 2.5)
    fc.set_light_color(unreal.LinearColor(0.7, 0.8, 1.0, 1.0))

    # P5 (SY6/SY7): a low-intensity cubemap SkyLight so metallic=1.0 surfaces
    # have something to reflect — in the empty level the golds previously
    # reflected pure black and only the (overdriven) emissive was visible.
    try:
        sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400), unreal.Rotator(0, 0, 0))
        sc = sky.get_component_by_class(unreal.SkyLightComponent)
        sc.set_mobility(unreal.ComponentMobility.MOVABLE)
        sc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        cubemap = unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")
        if cubemap:
            sc.set_editor_property("cubemap", cubemap)
            unreal.log("%s skylight: DaylightAmbientCubemap @ %.1f" % (TAG, SKYLIGHT_INTENSITY))
        else:
            unreal.log("%s skylight: engine cubemap missing, captured-scene source" % TAG)
        sc.set_editor_property("intensity", SKYLIGHT_INTENSITY)
        sc.recapture_sky()
    except Exception as ex:
        unreal.log("%s skylight FAILED (%s) — symbols will read flatter" % (TAG, ex))

    # -- helpers ------------------------------------------------------------
    def make_material(spec):
        # Emissive+metallic PBR in the M_SlotGold recipe family, with optional
        # world-space noise mottling (craters/skin) and a Fresnel rim light.
        sid = spec["id"]
        full = "%s/M_%s" % (MAT_DIR, sid)
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        mat = asset_tools.create_asset("M_" + sid, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

        def c3(rgb, x, y):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
            n.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
            return n

        def c1(v, x, y):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
            n.set_editor_property("r", v)
            return n

        def mul(a, b, x, y):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
            mel.connect_material_expressions(a, "", n, "A")
            mel.connect_material_expressions(b, "", n, "B")
            return n

        col = c3(spec["color"], -700, 0)

        # base colour, optionally mottled toward a darker shade by 3D noise.
        # We drive the Noise Position with WorldPosition * freq so the feature
        # size is explicit (~1/freq cm) and reads at ~100px — the node's own
        # "scale" was ambiguous and washed flat at this cam distance.
        base_out = col
        if "mottle" in spec:
            mc, freq, strength = spec["mottle"]
            wpos = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1000, 180)
            scaled = mul(wpos, c1(freq, -1000, 330), -820, 200)
            noise = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -640, 180)
            mel.connect_material_expressions(scaled, "", noise, "Position")
            noise.set_editor_property("scale", 1.0)
            noise.set_editor_property("output_min", 0.0)
            noise.set_editor_property("output_max", 1.0)
            alpha = mul(noise, c1(strength, -640, 330), -480, 220)
            lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -320, 40)
            mel.connect_material_expressions(col, "", lerp, "A")
            mel.connect_material_expressions(c3(mc, -640, 90), "", lerp, "B")
            mel.connect_material_expressions(alpha, "", lerp, "Alpha")
            base_out = lerp
            unreal.log("%s %s: mottle freq=%.3f strength=%.2f" % (TAG, sid, freq, strength))

        # optional polar ice cap baked into base colour: mask the top of the
        # sphere by world-normal Z (preserved under the yaw spin, so the cap stays
        # on the pole) and smoothstep rust -> ice with a soft edge — the white
        # follows the curvature exactly instead of perching on top.
        if "polar" in spec:
            ice_color, lo, hi, edge_amt, pfreq = spec["polar"]
            # latitude = dot(worldNormal, +Z): +1 at the north pole, 0 at equator
            nrm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1100, 560)
            dot = mel.create_material_expression(mat, unreal.MaterialExpressionDotProduct, -900, 560)
            mel.connect_material_expressions(nrm, "", dot, "A")
            mel.connect_material_expressions(c3((0.0, 0.0, 1.0), -1100, 700), "", dot, "B")
            # break the latitude line with signed world-space noise so the cap
            # edge is irregular/feathered (rust pokes through) — not a hat brim
            pn = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -1100, 820)
            ps = mul(mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1320, 820),
                     c1(pfreq, -1320, 960), -1200, 840)
            mel.connect_material_expressions(ps, "", pn, "Position")
            pn.set_editor_property("scale", 1.0)
            pn.set_editor_property("output_min", -1.0)
            pn.set_editor_property("output_max", 1.0)
            edge = mul(pn, c1(edge_amt, -900, 860), -760, 840)
            nz = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -640, 640)
            mel.connect_material_expressions(dot, "", nz, "A")
            mel.connect_material_expressions(edge, "", nz, "B")
            ss = mel.create_material_expression(mat, unreal.MaterialExpressionSmoothStep, -460, 600)
            try:
                ss.set_editor_property("const_min", lo)
                ss.set_editor_property("const_max", hi)
            except Exception:
                pass
            mel.connect_material_expressions(nz, "", ss, "Value")
            cap = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -300, 460)
            mel.connect_material_expressions(base_out, "", cap, "A")
            mel.connect_material_expressions(c3(ice_color, -640, 470), "", cap, "B")
            mel.connect_material_expressions(ss, "", cap, "Alpha")
            base_out = cap
        mel.connect_material_property(base_out, "", unreal.MaterialProperty.MP_BASE_COLOR)

        mel.connect_material_property(c1(spec["met"], -700, 420), "", unreal.MaterialProperty.MP_METALLIC)
        mel.connect_material_property(c1(spec["rough"], -700, 500), "", unreal.MaterialProperty.MP_ROUGHNESS)

        # emissive = base colour * strength (+ optional Fresnel rim light)
        emis_out = mul(col, c1(spec["em"], -700, 600), -320, 560)
        if "rim" in spec:
            rc, rstr = spec["rim"]
            fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -700, 720)
            fres.set_editor_property("exponent", 4.0)
            rim = mul(mul(fres, c1(rstr, -700, 860), -500, 760), c3(rc, -700, 940), -320, 760)
            add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -150, 600)
            mel.connect_material_expressions(emis_out, "", add, "A")
            mel.connect_material_expressions(rim, "", add, "B")
            emis_out = add
        mel.connect_material_property(emis_out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

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
    # ⚠️ ROTATOR LESSON, THIRD STRIKE (SY3): unreal.Rotator positional args are
    # (ROLL, PITCH, YAW) — the P3a/P4 code passed tilts positionally and every
    # one landed in the wrong slot. The star's cones got ROLL (fanned toward/
    # away from camera = the "chess pawn"); saturn/galaxy/scatter/wild discs got
    # YAW, which does NOTHING to a rotationally symmetric disc = every disc
    # rendered edge-on (the "UFOs", the brim-line halo). Keyword args ONLY here.
    def build_star(sid, c, mat):
        import math
        # Proper 5-point star facing the camera (camera looks along +Y, so the
        # star lives in the XZ plane). A +Z-pointing cone under pitch θ points
        # (-sinθ, 0, cosθ); each point sits offset from the core along that
        # same direction so the TIP faces outward.
        body = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(roll=0, pitch=0, yaw=0), (0.50, 0.50, 0.50), mat, "core")
        cone_scale = (0.24, 0.24, 0.60)        # 60cm tall points
        offset_r = 25.0 + (100.0 * cone_scale[2]) / 2.0 - 10.0   # core radius + half-height - overlap
        for i in range(5):
            theta = 72.0 * i
            rad = math.radians(theta)
            dx, dz = -math.sin(rad) * offset_r, math.cos(rad) * offset_r
            add_part(sid, body, MESH["cone"], V(c, x=dx, z=dz),
                     unreal.Rotator(roll=0, pitch=theta, yaw=0), cone_scale, mat, "point%d" % i)
        return body

    def build_moon(sid, c, mat):
        return add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (1.5, 1.5, 1.5), mat, "body")

    def build_mars(sid, c, mat):
        body = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(0, 0, 0), (1.4, 1.4, 1.4), mat, "body")
        # Ice cap is now baked into M_mars (normal-Z smoothstep polar mask) so it
        # hugs the curvature — drop the old separate cap sphere + its material.
        try:
            if eal.does_asset_exist("%s/M_mars_ice" % MAT_DIR):
                eal.delete_asset("%s/M_mars_ice" % MAT_DIR)
        except Exception:
            pass
        unreal.log("%s mars: ice cap (material polar mask in M_mars)" % TAG)
        return body

    def build_saturn(sid, c, mat):
        body = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(roll=0, pitch=0, yaw=0), (0.95, 0.95, 0.95), mat, "planet")
        # ring: flattened cylinder disc, tilted so it reads as an ellipse.
        # SY3 fix: the 78° was previously passed as YAW (no-op on a disc) —
        # the ring rendered as a flat edge-on line. ROLL tilts it toward camera.
        add_part(sid, body, MESH["cylinder"], V(c), unreal.Rotator(roll=78.0, pitch=0, yaw=0), (1.6, 1.6, 0.05), mat, "ring")
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
        # P5 REBUILD. The P4 seven shipped as a GREY SLAB: the material setters
        # silently no-op'd (the old call() swallowed every exception) and the
        # eyeballed glyph scale mis-framed the camera onto one stroke of a huge
        # default-grey "7". Two fixes (SY1/SY2):
        #   1. every setter logs tried/failed; if NO material setter lands we
        #      RAISE to the gold box fallback — grey can never ship again;
        #   2. framing is computed from MEASURED bounds (get_actor_bounds), and
        #      the actor is recentred on its bounds origin — no eyeball scale.
        try:
            if not hasattr(unreal, "Text3DActor"):
                raise RuntimeError("Text3DActor unavailable (Text 3D plugin off)")
            # face the camera at -Y: keyword args (SY3)
            actor = eas.spawn_actor_from_class(unreal.Text3DActor, V(c), unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
            comp = actor.get_component_by_class(unreal.Text3DComponent)

            def call(names, *args):
                tried = []
                for n in names:
                    fn = getattr(comp, n, None)
                    if callable(fn):
                        try:
                            fn(*args)
                            return True
                        except Exception as e:
                            tried.append("%s!%s" % (n, e))
                    else:
                        tried.append("%s?missing" % n)
                if tried:
                    unreal.log("%s seven setter FAILED: %s" % (TAG, "; ".join(tried)))
                return False

            try:
                comp.set_editor_property("text", "7")
            except Exception as ex:
                raise RuntimeError("set text failed: %s" % ex)
            call(["set_extrude"], 25.0)
            call(["set_bevel"], 3.0)
            call(["set_bevel_segments"], 4)
            if hasattr(unreal, "Text3DBevelType"):
                call(["set_bevel_type"], unreal.Text3DBevelType.CONVEX)
            if hasattr(unreal, "Text3DHorizontalTextAlignment"):
                call(["set_horizontal_alignment"], unreal.Text3DHorizontalTextAlignment.CENTER)
            if hasattr(unreal, "Text3DVerticalTextAlignment"):
                call(["set_vertical_alignment"], unreal.Text3DVerticalTextAlignment.CENTER)

            # SY1: the gold MUST land. Try the per-face setters, then property
            # spellings; zero successes -> fallback (never ship default grey).
            mat_hits = 0
            for setter in ("set_front_material", "set_back_material", "set_extrude_material", "set_bevel_material"):
                if call([setter], mat):
                    mat_hits += 1
            if mat_hits == 0:
                for prop in ("front_material", "back_material", "extrude_material", "bevel_material"):
                    try:
                        comp.set_editor_property(prop, mat)
                        mat_hits += 1
                    except Exception:
                        pass
            unreal.log("%s seven: %d material slot(s) set to M_seven" % (TAG, mat_hits))
            if mat_hits == 0:
                raise RuntimeError("NO material setter landed — would ship grey")

            # SY2: measured framing. Scale so glyph height = FRAME_FILL of the
            # frame, then recentre the actor on its measured bounds origin.
            origin, extent = actor.get_actor_bounds(False)
            if extent.z < 1.0:
                raise RuntimeError("glyph bounds degenerate (extent.z=%.2f) — text never built" % extent.z)
            desired_half = (TARGET_SIZE * FRAME_FILL) / 2.0
            s = desired_half / extent.z
            actor.set_actor_scale3d(unreal.Vector(s, s, s))
            origin, extent = actor.get_actor_bounds(False)
            target = V(c)
            actor.add_actor_world_offset(unreal.Vector(target.x - origin.x, target.y - origin.y, target.z - origin.z), False, False)
            unreal.log("%s seven: Text3D glyph scaled %.2f, bounds %.0fx%.0fcm, recentred" % (TAG, s, extent.x * 2, extent.z * 2))

            actor.set_actor_label("%s_glyph" % sid)
            try:
                actor.set_folder_path("SlotFactory/%s" % sid)
            except Exception:
                pass
            return actor
        except Exception as ex:
            unreal.log("%s seven: Text3D failed (%s) -> GOLD box fallback" % (TAG, ex))
            bar = add_part(sid, None, MESH["cube"], V(c, x=0, z=48), unreal.Rotator(roll=0, pitch=0, yaw=0), (1.0, 0.18, 0.16), mat, "bar")
            add_part(sid, bar, MESH["cube"], V(c, x=10, z=-12), unreal.Rotator(roll=0, pitch=20.0, yaw=0), (0.16, 0.18, 0.95), mat, "stem")
            return bar

    def build_galaxy(sid, c, mat):
        # P5: textured emissive card — a NASA face-on spiral on a camera-facing
        # plane (engine Plane normal is +Z; roll=90 turns it toward the camera
        # at -Y). The card material comes from make_galaxy_card_material via
        # ctx; if the texture file was missing we fall back to the primitive
        # disc — with the SY3 tilt actually applied this time (roll, not yaw).
        if ctx.get("galaxy_card_mat") is not None:
            card = add_part(sid, None, MESH["plane"], V(c),
                            unreal.Rotator(roll=90.0, pitch=0, yaw=0), (1.6, 1.6, 1.0),
                            ctx["galaxy_card_mat"], "card")
            unreal.log("%s galaxy: textured card (T_galaxy)" % TAG)
            return card
        unreal.log("%s galaxy: NO texture -> primitive disc fallback (tilted)" % TAG)
        disc = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(roll=68.0, pitch=0, yaw=0), (1.55, 1.55, 0.22), mat, "disc")
        add_part(sid, disc, MESH["sphere"], V(c), unreal.Rotator(roll=0, pitch=0, yaw=0), (0.45, 0.45, 0.45), mat, "core")
        return disc

    def build_wild(sid, c, mat):
        band = build_crown_parts(sid, c, mat)
        # glow halo ring facing camera, behind the crown.
        # SY3 fix: 90° was YAW (no-op on a cylinder) — the halo rendered as a
        # horizontal brim-line above the band. ROLL turns it to face the camera.
        add_part(sid, band, MESH["cylinder"], V(c, y=40, z=15), unreal.Rotator(roll=90.0, pitch=0, yaw=0), (1.7, 1.7, 0.05), mat, "halo")
        return band

    def build_scatter(sid, c, mat):
        # SY3 fix: 68° tilt was YAW (no-op on a flattened sphere) — the disc
        # rendered edge-on, the "UFO". ROLL tilts it toward the camera.
        disc = add_part(sid, None, MESH["sphere"], V(c), unreal.Rotator(roll=68.0, pitch=0, yaw=0), (1.45, 1.45, 0.22), mat, "disc")
        add_part(sid, disc, MESH["sphere"], V(c), unreal.Rotator(roll=0, pitch=0, yaw=0), (0.4, 0.4, 0.4), mat, "core")
        for i, (dx, dz) in enumerate(((70, 35), (-65, 45), (60, -50), (-55, -40))):
            add_part(sid, disc, MESH["sphere"], V(c, x=dx, z=dz), unreal.Rotator(roll=0, pitch=0, yaw=0), (0.16, 0.16, 0.16), mat, "spark%d" % i)
        return disc

    BUILDERS = {
        "star": build_star, "moon": build_moon, "galaxy": build_galaxy, "saturn": build_saturn,
        "mars": build_mars, "crown": build_crown, "seven": build_seven, "wild": build_wild,
        "scatter": build_scatter,
    }

    # -- P5: galaxy texture import + unlit masked card material (SY4/SY5) ---
    def import_galaxy_texture():
        import os
        for p in GALAXY_TEXTURE_CANDIDATES:
            if os.path.exists(p):
                task = unreal.AssetImportTask()
                task.filename = p
                task.destination_path = ROOT + "/Textures"
                task.destination_name = "T_galaxy"
                task.automated = True
                task.save = True
                task.replace_existing = True
                unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
                tex = unreal.load_asset(ROOT + "/Textures/T_galaxy")
                if tex:
                    unreal.log("%s galaxy texture imported: %s -> T_galaxy" % (TAG, p))
                    return tex
                unreal.log_error("%s galaxy texture import FAILED from %s" % (TAG, p))
        unreal.log("%s galaxy texture: NO source file found (tried %d paths) -> disc fallback" % (TAG, len(GALAXY_TEXTURE_CANDIDATES)))
        return None

    def make_galaxy_card_material(tex):
        # Unlit + MASKED (SY5): masked pixels write clean alpha through MRQ's
        # Alpha Output; unlit means only emissive matters — a photo card, not a
        # lit surface. Mask = luminance with a low clip so the black background
        # of the NASA image cuts away.
        full = "%s/M_galaxy_card" % MAT_DIR
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        mat = asset_tools.create_asset("M_galaxy_card", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("opacity_mask_clip_value", 0.06)

        ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -800, 0)
        ts.set_editor_property("texture", tex)

        def c3(rgb, x, y):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, x, y)
            n.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
            return n

        def c1(v, x, y):
            n = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
            n.set_editor_property("r", v)
            return n

        tint = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -560, 0)
        mel.connect_material_expressions(ts, "", tint, "A")
        mel.connect_material_expressions(c3(GALAXY_TINT, -800, 160), "", tint, "B")
        glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -380, 0)
        mel.connect_material_expressions(tint, "", glow, "A")
        mel.connect_material_expressions(c1(GALAXY_EMISSIVE, -560, 160), "", glow, "B")
        mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

        lum = mel.create_material_expression(mat, unreal.MaterialExpressionDotProduct, -560, 320)
        mel.connect_material_expressions(ts, "", lum, "A")
        mel.connect_material_expressions(c3((0.333, 0.333, 0.333), -800, 380), "", lum, "B")
        mel.connect_material_property(lum, "", unreal.MaterialProperty.MP_OPACITY_MASK)

        mel.recompile_material(mat)
        eal.save_asset(full)
        return mat

    galaxy_tex = import_galaxy_texture()
    ctx = {"galaxy_card_mat": make_galaxy_card_material(galaxy_tex) if galaxy_tex else None}

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
    for idx, spec in enumerate(SYMBOLS):
        sid = spec["id"]
        try:
            center = unreal.Vector(idx * SPACING, 0.0, BASE_Z)
            mat = make_material(spec)
            body = BUILDERS[spec["builder"]](sid, center, mat)

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
    unreal.log("%s P4b STANDING RULE: delete + re-add ALL MRQ jobs before rendering — stale jobs point at dead assets." % TAG)


main()
