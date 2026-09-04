# build_launch_shot.py — Sequencer launch cutscene on PackDev's SM_Rocket.
#
# *** RUN FROM THE OPEN EDITOR, NOT HEADLESS ***
#   Cmd box (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_launch_shot.py"
#
# SAVE YOUR CURRENT LEVEL FIRST. First run creates /Game/Cinematics/L_Cine_Launch.
# Later runs require that level to already be open — load_level fatals this editor
# (dump_launchpad_layout.py / EditorServer.cpp:2524).
#
# Builds, idempotently:
#   /Game/Cinematics/NS_RocketPlume     Niagara Fluids gas, orange, for the engine bell
#   /Game/Cinematics/L_Cine_Launch      pad + SM_Rocket + interiors + cameras + sky
#   /Game/Cinematics/LS_Rocket_Launch   transform on the hull, camera cuts, 22 s
#   SequenceCue_Launch                  plays it, then travels to L_City
#
# THE GAMEPLAY PATH DOES NOT WAIT FOR THIS. ASpaceport::BeginLaunch already flies the
# live rocket on the lawn with the same Niagara template. This script is the Sequencer
# shot: open L_Cine_Launch, press Play in Sequencer, or PIE this map, and Unreal is
# generating the cutscene from the project's own hull.
#
# HEADLESS CRASHES THE ENGINE. Same rule as build_kaia_intro_shot.py.

import json
import traceback

import unreal

TAG = "###LAUNCHSHOT###"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/launch_shot_report.json"

LEVEL = "/Game/Cinematics/L_Cine_Launch"
SEQ_DIR = "/Game/Cinematics"
SEQ_NAME = "LS_Rocket_Launch"
PLUME_DEST = "/Game/Cinematics/NS_RocketPlume"
PLUME_SRC = "/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_ColoredSmoke"

FPS = 30
DURATION_S = 22.0
HOLD_S = 2.5
CLIMB_CM = 80000.0
TURN_DEG = -12.0
FRAMES = int(DURATION_S * FPS)
HOLD_F = int(HOLD_S * FPS)

# Artist layout, copied from ASpaceport::MakeDefaultLayout (dump_launchpad_layout.py).
P_GROUND = "/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Ground.SM_Ground"
P_BASE = "/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Base.SM_Base"
P_PAD = "/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Launch_Pad.SM_Launch_Pad"
P_HOLDER = "/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Rocket_Holder.SM_Rocket_Holder"
P_PIPES = "/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Cooling_Pipes.SM_Cooling_Pipes"
P_DETAILS = "/Game/Rocket_Launch_Pad/Meshes/Environment/SM_Details.SM_Details"
P_ROCKET = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Rocket.SM_Rocket"
P_WALLS = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Walls.SM_Walls"
P_BACK = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Back_Wall.SM_Back_Wall"
P_IFACE = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Interface.SM_Interface"
P_POWER = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Power_Box.SM_Power_Box"
P_SEATS = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Seats.SM_Seats"
P_BAGS = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Storage_Bags.SM_Storage_Bags"
P_CONTROLS = "/Game/Rocket_Launch_Pad/Meshes/Rocket/SM_Controls.SM_Controls"

MANAGED = {
    "Launch_Ground", "Launch_Base", "Launch_Pad", "Launch_Details", "Launch_Pipes",
    "Launch_Holder", "Launch_Rocket", "Launch_Walls", "Launch_BackWall", "Launch_Iface",
    "Launch_Power", "Launch_Bags", "Launch_Seats", "Launch_Controls", "Launch_Plume",
    "Cam_Cabin", "Cam_Wide", "Cam_Plume", "Cam_Track", "Cam_Sky",
    "Launch_Sun", "Launch_Sky", "Launch_SkyLight", "Launch_Fog",
    "SequenceCue_Launch", "CutsceneStart",
}

ORANGE = unreal.LinearColor(1.0, 0.42, 0.08, 1.0)
r = {"steps": []}


def step(name, ok, extra=None):
    e = {"step": name, "ok": bool(ok)}
    if extra:
        e.update(extra)
    r["steps"].append(e)
    unreal.log("%s %s: %s %s" % (TAG, "OK" if ok else "FAIL", name, extra or ""))


def vec(x, y, z):
    return unreal.Vector(x, y, z)


def rot(p, y, rll):
    return unreal.Rotator(p, y, rll)


def spawn_mesh(eas, path, loc, rotation, scale, label):
    eal = unreal.EditorAssetLibrary
    mesh = eal.load_asset(path.split(".")[0])
    if not mesh:
        step("mesh:%s" % label, False, {"path": path})
        return None
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, rotation)
    if not a:
        step("spawn:%s" % label, False)
        return None
    a.set_actor_label(label)
    a.set_actor_scale3d(scale)
    smc = a.static_mesh_component
    smc.set_editor_property("static_mesh", mesh)
    smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    return a


def ensure_plume():
    eal = unreal.EditorAssetLibrary
    if not eal.does_directory_exist(SEQ_DIR):
        eal.make_directory(SEQ_DIR)
    if eal.does_asset_exist(PLUME_DEST):
        step("plume_exists", True, {"path": PLUME_DEST})
        return eal.load_asset(PLUME_DEST)
    if not eal.does_asset_exist(PLUME_SRC):
        step("plume_src", False, {"path": PLUME_SRC})
        return None
    copied = eal.duplicate_asset(PLUME_SRC, PLUME_DEST)
    if copied:
        eal.save_asset(PLUME_DEST)
    step("plume_copy", copied is not None, {"src": PLUME_SRC})
    return copied


def add_cut(seq, track, binding, t0, t1):
    sec = track.add_section()
    f0, f1 = int(t0 * FPS), int(t1 * FPS)
    if hasattr(sec, "set_range"):
        sec.set_range(f0, f1)
    else:
        sec.set_start_frame_seconds(t0)
        sec.set_end_frame_seconds(t1)
    sec.set_camera_binding_id(
        unreal.MovieSceneSequenceExtensions.get_binding_id(seq, binding))
    return sec


def key_transform(section, actor, hold_f, end_f):
    loc = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scl = actor.get_actor_scale3d()
    ch = section.get_all_channels()
    if not ch or len(ch) < 9:
        step("transform_channels", False, {"n": len(ch) if ch else 0})
        return
    def key_all(frame, z, pitch):
        vals = [loc.x, loc.y, z, rotation.roll, pitch, rotation.yaw, scl.x, scl.y, scl.z]
        fn = unreal.FrameNumber(int(frame))
        for i, v in enumerate(vals):
            ch[i].add_key(fn, float(v))
    key_all(0, loc.z, rotation.pitch)
    key_all(hold_f, loc.z, rotation.pitch)
    for f in range(hold_f, end_f + 1, 10):
        t = float(f - hold_f) / float(max(1, end_f - hold_f))
        e = t * t * t
        turn = max(0.0, min(1.0, (t - 0.12) / 0.55))
        key_all(f, loc.z + CLIMB_CM * e, rotation.pitch + TURN_DEG * turn)
    step("rocket_keys", True, {"frames": end_f, "climb_cm": CLIMB_CM})


def main():
    eal = unreal.EditorAssetLibrary
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    plume = ensure_plume()

    open_path = ues.get_editor_world().get_path_name().split(".")[0]
    if eal.does_asset_exist(LEVEL):
        if not open_path.endswith("L_Cine_Launch"):
            raise Exception(
                "Open %s in the editor first (load_level fatals). Currently: %s" % (LEVEL, open_path))
        step("level_open", True, {"level": LEVEL})
    else:
        les.new_level(LEVEL)
        step("new_level", True, {"level": LEVEL})

    removed = 0
    for a in list(eas.get_all_level_actors()):
        if a.get_actor_label() in MANAGED:
            eas.destroy_actor(a)
            removed += 1
    step("clear_previous", True, {"removed": removed})

    one = vec(1, 1, 1)
    flat = rot(0, 0, 0)

    spawn_mesh(eas, P_GROUND, vec(2212.7, 0.2, -95.4), rot(0, 90, 0), one, "Launch_Ground")
    spawn_mesh(eas, P_BASE, vec(2410.6, 0.0, 0.0), flat, one, "Launch_Base")
    spawn_mesh(eas, P_PAD, vec(0.0, 0.0, 700.3), flat, one, "Launch_Pad")
    spawn_mesh(eas, P_DETAILS, vec(1665.6, 1757.5, 651.7), flat, one, "Launch_Details")
    spawn_mesh(eas, P_PIPES, vec(0.0, 128.5, 1128.9), flat, one, "Launch_Pipes")
    spawn_mesh(eas, P_HOLDER, vec(0.0, -73.7, 1389.1), flat, one, "Launch_Holder")

    rocket = spawn_mesh(eas, P_ROCKET, vec(0.0, 360.2, 1433.2), rot(0, 0.96, 0), one, "Launch_Rocket")
    step("rocket", rocket is not None)

    iscale = vec(1.352, 1.352, 1.352)
    irot = rot(0, -52.28, 0)
    iat = vec(-19.6, 314.1, 11900.5)
    interiors = [
        spawn_mesh(eas, P_WALLS, iat, irot, iscale, "Launch_Walls"),
        spawn_mesh(eas, P_BACK, iat, rot(0, -52.94, 0), iscale, "Launch_BackWall"),
        spawn_mesh(eas, P_IFACE, iat, irot, iscale, "Launch_Iface"),
        spawn_mesh(eas, P_POWER, iat, irot, iscale, "Launch_Power"),
        spawn_mesh(eas, P_BAGS, iat, irot, iscale, "Launch_Bags"),
        spawn_mesh(eas, P_SEATS, vec(-52.8, 294.4, 11900.5), rot(0, -60.22, 0), iscale, "Launch_Seats"),
        spawn_mesh(eas, P_CONTROLS, vec(-62.0, 287.2, 11930.0), irot, one, "Launch_Controls"),
    ]
    attached = 0
    if rocket:
        for a in interiors:
            if not a:
                continue
            a.attach_to_actor(
                rocket, "",
                unreal.AttachmentRule.KEEP_WORLD,
                unreal.AttachmentRule.KEEP_WORLD,
                unreal.AttachmentRule.KEEP_WORLD,
                False)
            attached += 1
    step("attach_interior", attached == 7, {"attached": attached})

    # Niagara on the engine bell, parented so it climbs with the hull.
    plume_actor = None
    if rocket and plume and hasattr(unreal, "NiagaraActor"):
        engine = vec(0.0, 360.2, 1033.2)
        plume_actor = eas.spawn_actor_from_class(
            unreal.NiagaraActor, engine, rot(180, 0, 0))
        if plume_actor:
            plume_actor.set_actor_label("Launch_Plume")
            nc = plume_actor.get_component_by_class(unreal.NiagaraComponent)
            if nc:
                nc.set_asset(plume)
                nc.set_relative_scale3d(vec(5, 5, 5))
                nc.set_auto_activate(True)
                for name in (
                    "User.Color", "Color", "User.SmokeColor", "Smoke Color",
                    "User.Albedo", "Albedo", "Grid3D_Gas.Color",
                ):
                    try:
                        nc.set_variable_linear_color(name, ORANGE)
                    except Exception:
                        pass
            plume_actor.attach_to_actor(
                rocket, "",
                unreal.AttachmentRule.KEEP_WORLD,
                unreal.AttachmentRule.KEEP_WORLD,
                unreal.AttachmentRule.KEEP_WORLD,
                False)
    step("plume_actor", plume_actor is not None)

    def cine(label, loc, look, fov):
        yaw_pitch = unreal.KismetMathLibrary.find_look_at_rotation(loc, look)
        cam = eas.spawn_actor_from_class(unreal.CineCameraActor, loc, yaw_pitch)
        if not cam:
            return None
        cam.set_actor_label(label)
        comp = cam.get_cine_camera_component()
        comp.set_editor_property("current_focal_length", fov)
        return cam

    cam_cabin = cine("Cam_Cabin", vec(15.7, 325.6, 12010.0), vec(-200, 200, 12010), 40.0)
    cam_wide = cine("Cam_Wide", vec(-14000, -9000, 2200), vec(0, 360, 4000), 35.0)
    cam_plume = cine("Cam_Plume", vec(3400, 2800, 380), vec(0, 360, 900), 40.0)
    cam_track = cine("Cam_Track", vec(5200, -3200, 5200), vec(0, 360, 6500), 50.0)
    cam_sky = cine("Cam_Sky", vec(-18000, 11000, 28000), vec(0, 360, 22000), 35.0)
    cams = [cam_cabin, cam_wide, cam_plume, cam_track, cam_sky]
    step("cameras", all(c is not None for c in cams),
         {"n": sum(1 for c in cams if c)})

    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, vec(0, 0, 8000), rot(-42, 35, 0))
    if sun:
        sun.set_actor_label("Launch_Sun")
    sky = eas.spawn_actor_from_class(unreal.SkyAtmosphere, vec(0, 0, 0), flat)
    if sky:
        sky.set_actor_label("Launch_Sky")
    skylight = eas.spawn_actor_from_class(unreal.SkyLight, vec(0, 0, 4000), flat)
    if skylight:
        skylight.set_actor_label("Launch_SkyLight")
        try:
            skylight.sky_light_component.set_editor_property("real_time_capture", True)
        except Exception:
            pass
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, vec(0, 0, 0), flat)
    if fog:
        fog.set_actor_label("Launch_Fog")
    step("sky_rig", all(x is not None for x in (sun, sky, skylight, fog)))

    start = eas.spawn_actor_from_class(unreal.PlayerStart, vec(-4000, -4000, 200), flat)
    if start:
        start.set_actor_label("CutsceneStart")

    les.save_current_level()
    step("save_level", True)

    # ---- sequence -------------------------------------------------------
    seq_path = "%s/%s" % (SEQ_DIR, SEQ_NAME)
    if eal.does_asset_exist(seq_path):
        eal.delete_asset(seq_path)
    seq = tools.create_asset(SEQ_NAME, SEQ_DIR, unreal.LevelSequence,
                             unreal.LevelSequenceFactoryNew())
    step("create_sequence", seq is not None)
    if not seq:
        return

    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(FRAMES)

    if rocket:
        rb = seq.add_possessable(rocket)
        ts = rb.add_track(unreal.MovieScene3DTransformTrack).add_section()
        ts.set_start_frame_seconds(0.0)
        ts.set_end_frame_seconds(DURATION_S)
        key_transform(ts, rocket, HOLD_F, FRAMES)

    cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
    bindings = []
    for cam in cams:
        bindings.append(seq.add_possessable(cam) if cam else None)

    # cabin 0-2.5, wide 2.5-5.5, plume 5.5-10, track 10-16, sky 16-22
    times = [0.0, 2.5, 5.5, 10.0, 16.0, DURATION_S]
    for i in range(5):
        if bindings[i]:
            add_cut(seq, cut_track, bindings[i], times[i], times[i + 1])
    step("camera_cuts", True, {"shots": 5, "seconds": DURATION_S})

    eal.save_asset(seq_path)
    step("save_sequence", True, {"sequence": seq_path})

    cue = eas.spawn_actor_from_class(unreal.SequenceCue, vec(0, 0, 0), flat)
    if cue:
        cue.set_actor_label("SequenceCue_Launch")
        cue.set_editor_property("sequence", unreal.SoftObjectPath(seq_path))
        cue.set_editor_property("next_level", "L_City")
        cue.set_editor_property("cue_id", "RocketLaunch")
        cue.set_editor_property("play_on_begin_play", True)
        cue.set_editor_property("start_delay", 0.4)
        cue.set_editor_property("skippable", True)
        cue.set_editor_property("max_seconds", 40.0)
        cue.set_editor_property("fade_in_seconds", 1.0)
    step("sequence_cue", cue is not None)

    les.save_current_level()
    step("save_level_final", True)


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()
    unreal.log_error("%s %s" % (TAG, r["traceback"]))

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("%s wrote %s" % (TAG, OUT))
