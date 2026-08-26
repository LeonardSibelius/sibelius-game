# build_kaia_intro_shot.py — the opening cutscene shot (docs/CINEMATICS.md).
#
# Builds, idempotently:
#   /Game/Cinematics/L_Cine_KaiaIntro   an EMPTY level - no sky, no floor, no fog, so
#                                       everything the camera does not light is BLACK.
#                                       That is the "dark background" (Walt, 2026-08-25):
#                                       an absence, not a backdrop mesh, which means no
#                                       seams, no bounce and nothing to light.
#   Kaia (BP_MHC_Kaia)                  the plain MetaHuman, NOT BP_Dancer_Kaia - a
#                                       dancing body would fight the face performance.
#   Three lights                        key / fill / rim, the standard portrait setup.
#   CineCameraActor                     framed on her head.
#   /Game/Cinematics/LS_Kaia_Intro      Level Sequence: face animation + her voice +
#                                       a camera cut.
#
# *** RUN THIS FROM THE OPEN EDITOR, NOT HEADLESS ***
#
#   Cmd box at the bottom of the main window (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_kaia_intro_shot.py"
#
# HEADLESS CRASHES THE ENGINE. -run=pythonscript has no editor viewport world, so
# spawn_actor_from_object dies on an access violation inside EditorFramework.dll
# (verified 2026-08-25: it created the empty level, then took the whole commandlet
# down on the first spawn). Asset work headless is fine - that is how the audio and
# the Performance asset were built - but LEVEL work needs the real editor. The same
# rule the import scripts carry, extended to actors.
#
# SAVE YOUR CURRENT LEVEL FIRST. This switches the editor to L_Cine_KaiaIntro.
#
# FRAMING IS A GUESS AND EXPECTED TO NEED A NUDGE. MetaHumans do not agree with actor
# +X about which way is forward (the DancerAgentComponent header records yaw -90), and
# nobody here can see the viewport. HEAD_Z / CAM_* below are the knobs; move the camera
# in the level and re-save rather than editing numbers blind.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_shot_report.json"

LEVEL = "/Game/Cinematics/L_Cine_KaiaIntro"
SEQ_DIR = "/Game/Cinematics"
SEQ_NAME = "LS_Kaia_Intro"
KAIA = "/Game/MetaHumans/MHC_Kaia/BP_MHC_Kaia"
FACE_ANIM = "/Game/Cinematics/AS_MHP_Kaia_Intro_Face"
AUDIO = "/Game/Audio/Cinematics/kaia_intro"

HEAD_Z = 160.0          # eye height, cm
KAIA_YAW = 90.0         # so she faces +X, where the camera is
CAM_DIST = 75.0         # portrait distance from her face
FPS = 30

r = {"steps": []}


def step(name, ok, extra=None):
    e = {"step": name, "ok": bool(ok)}
    if extra:
        e.update(extra)
    r["steps"].append(e)


def main():
    eal = unreal.EditorAssetLibrary
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    for p in (KAIA, FACE_ANIM, AUDIO):
        step("input:%s" % p, eal.does_asset_exist(p))

    # ---- empty level = black world ------------------------------------
    # Re-runnable: create it the first time, LOAD it after. new_level on an existing
    # path fails ("An asset already exists at this location") and leaves you editing
    # whatever level happened to be open, which is a good way to spawn a MetaHuman
    # into the office by accident.
    if eal.does_asset_exist(LEVEL):
        les.load_level(LEVEL)
        step("load_level", True, {"level": LEVEL})
    else:
        les.new_level(LEVEL)
        step("new_level", True, {"level": LEVEL})

    # Clear anything a previous run left, so the shot never accumulates duplicate
    # Kaias and stacked lights.
    MANAGED = {"Kaia", "Key", "Fill", "Rim", "CineCam_KaiaIntro"}
    removed = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_label() in MANAGED:
            eas.destroy_actor(a)
            removed += 1
    step("clear_previous", True, {"removed": removed})

    # ---- Kaia ----------------------------------------------------------
    kaia_bp = eal.load_asset(KAIA)
    kaia = eas.spawn_actor_from_object(
        kaia_bp, unreal.Vector(0, 0, 0), unreal.Rotator(0, KAIA_YAW, 0))
    step("spawn_kaia", kaia is not None)
    if kaia:
        kaia.set_actor_label("Kaia")

    # ---- portrait lighting --------------------------------------------
    # Rect lights: soft, and a rect reads as a softbox on skin. Key is warm and
    # off-axis; fill is dim and cool; rim separates her from the black.
    def rect(label, loc, rot, intensity, color, w, h):
        a = eas.spawn_actor_from_class(unreal.RectLight, loc, rot)
        if not a:
            return None
        a.set_actor_label(label)
        c = a.rect_light_component
        c.set_editor_property("intensity", intensity)
        c.set_editor_property("light_color", color)
        c.set_editor_property("source_width", w)
        c.set_editor_property("source_height", h)
        c.set_editor_property("attenuation_radius", 800.0)
        return a

    key = rect("Key", unreal.Vector(90, -70, HEAD_Z + 30),
               unreal.Rotator(-15, 215, 0), 18000.0,
               unreal.Color(255, 241, 224), 120, 160)
    fill = rect("Fill", unreal.Vector(70, 90, HEAD_Z - 10),
                unreal.Rotator(-5, 140, 0), 4500.0,
                unreal.Color(214, 226, 255), 160, 160)
    rim = rect("Rim", unreal.Vector(-90, 60, HEAD_Z + 60),
               unreal.Rotator(-25, 35, 0), 9000.0,
               unreal.Color(210, 225, 255), 80, 120)
    step("lights", all(x is not None for x in (key, fill, rim)))

    # ---- camera --------------------------------------------------------
    cam = eas.spawn_actor_from_class(
        unreal.CineCameraActor,
        unreal.Vector(CAM_DIST, 0, HEAD_Z),
        unreal.Rotator(0, 180, 0))          # look back down -X at her
    step("spawn_camera", cam is not None)
    if cam:
        cam.set_actor_label("CineCam_KaiaIntro")
        comp = cam.get_cine_camera_component()
        # 50mm on a 36mm sensor: a portrait lens. Wider would distort her face,
        # longer would need the camera further back than the lighting allows.
        comp.set_editor_property("current_focal_length", 50.0)
        fs = comp.get_editor_property("focus_settings")
        fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
        fs.set_editor_property("manual_focus_distance", CAM_DIST)
        comp.set_editor_property("focus_settings", fs)

    les.save_current_level()
    step("save_level", True)

    # ---- level sequence -------------------------------------------------
    seq_path = "%s/%s" % (SEQ_DIR, SEQ_NAME)
    if eal.does_asset_exist(seq_path):
        eal.delete_asset(seq_path)
    seq = tools.create_asset(SEQ_NAME, SEQ_DIR, unreal.LevelSequence,
                             unreal.LevelSequenceFactoryNew())
    step("create_sequence", seq is not None)
    if not seq:
        return

    anim = eal.load_asset(FACE_ANIM)
    frames = unreal.AnimationLibrary.get_num_frames(anim)
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(frames)
    step("playback_range", True, {"frames": frames})

    # Face animation. The anim must go on the FACE component, not the actor:
    # the actor binding would look for a body skeleton and find nothing to play.
    kaia_binding = seq.add_possessable(kaia)
    face_comp = None
    for c in kaia.get_components_by_class(unreal.SkeletalMeshComponent):
        n = c.get_name()
        if "Face" in n and "PostProcess" not in n:
            face_comp = c
            break
    step("find_face_component", face_comp is not None,
         {"name": face_comp.get_name() if face_comp else None})

    if face_comp:
        face_binding = seq.add_possessable(face_comp)
        atrack = face_binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
        asec = atrack.add_section()
        asec.set_range(0, frames)
        params = asec.get_editor_property("params")
        params.set_editor_property("animation", anim)
        asec.set_editor_property("params", params)
        step("face_anim_track", True)

    # Voice.
    audio = eal.load_asset(AUDIO)
    audio_track = seq.add_track(unreal.MovieSceneAudioTrack)
    audio_sec = audio_track.add_section()
    audio_sec.set_editor_property("sound", audio)
    audio_sec.set_range(0, frames)
    step("audio_track", True)

    # Camera cut, so the sequence renders through the cine camera.
    cam_binding = seq.add_possessable(cam)
    cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
    cut_sec = cut_track.add_section()
    cut_sec.set_range(0, frames)
    # MovieSceneObjectBindingID cannot be built by hand from Python - it has no
    # constructor args, and unreal.MovieSceneSequenceID does not exist at all
    # (verified 2026-08-25, the exception that killed the first run). The sequence
    # makes the id for you:
    cut_sec.set_camera_binding_id(
        unreal.MovieSceneSequenceExtensions.get_binding_id(seq, cam_binding))
    step("camera_cut", True)

    eal.save_asset(seq_path)
    step("save_sequence", True, {"sequence": seq_path})


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
