# rebuild_kaia_sequence.py — rebuild ONLY LS_Kaia_Intro, binding to the actors that are
# already in L_Cine_KaiaIntro.
#
# *** RUN FROM THE OPEN EDITOR with L_Cine_KaiaIntro loaded ***
#   Cmd box (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/rebuild_kaia_sequence.py"
#
# DOES NOT TOUCH THE LEVEL. Kaia, the lights and - most importantly - the camera framing
# Walt flew by hand are left exactly as they are. Only the sequence asset is replaced.
#
# WHY THIS EXISTS. build_kaia_intro_shot.py died on the camera-cut call the first time
# (unreal.MovieSceneSequenceID does not exist) AFTER creating the sequence but BEFORE
# saving it. The half-built asset had no camera cut and no playback range, which in game
# looked like "Kaia flashed on screen for half a second, small, then the level loaded":
# no cut means you watch through the player's camera, no range means it ends instantly.
# Re-running the original builder would have fixed the sequence and wiped the framing,
# so this does the sequence half alone.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_seq_report.json"
SEQ_DIR = "/Game/Cinematics"
SEQ_NAME = "LS_Kaia_Intro"
FACE_ANIM = "/Game/Cinematics/AS_MHP_Kaia_Intro_Face"
AUDIO = "/Game/Audio/Cinematics/kaia_intro"
FPS = 30

r = {}

try:
    eal = unreal.EditorAssetLibrary
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    kaia = cam = None
    for a in eas.get_all_level_actors():
        lbl = a.get_actor_label()
        if lbl == "Kaia":
            kaia = a
        elif lbl == "CineCam_KaiaIntro":
            cam = a
    r["found"] = {"kaia": kaia is not None, "camera": cam is not None}
    if not (kaia and cam):
        raise Exception("Open L_Cine_KaiaIntro first - Kaia / CineCam_KaiaIntro not found.")

    # Record the framing we are preserving, so the report proves it was not disturbed.
    loc = cam.get_actor_location()
    rot = cam.get_actor_rotation()
    r["camera_kept"] = {
        "location": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
        "rotation": [round(rot.pitch, 1), round(rot.yaw, 1), round(rot.roll, 1)],
    }

    seq_path = "%s/%s" % (SEQ_DIR, SEQ_NAME)
    if eal.does_asset_exist(seq_path):
        eal.delete_asset(seq_path)
    seq = tools.create_asset(SEQ_NAME, SEQ_DIR, unreal.LevelSequence,
                             unreal.LevelSequenceFactoryNew())
    if not seq:
        raise Exception("could not create the Level Sequence")

    anim = eal.load_asset(FACE_ANIM)
    frames = unreal.AnimationLibrary.get_num_frames(anim)
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(frames)
    r["frames"] = frames
    r["seconds"] = round(float(frames) / FPS, 2)

    # Face animation goes on the FACE component, not the actor - an actor binding looks
    # for a body skeleton and finds nothing to play.
    seq.add_possessable(kaia)
    face = None
    for c in kaia.get_components_by_class(unreal.SkeletalMeshComponent):
        n = c.get_name()
        if "Face" in n and "PostProcess" not in n:
            face = c
            break
    r["face_component"] = face.get_name() if face else None
    if face:
        fb = seq.add_possessable(face)
        atrack = fb.add_track(unreal.MovieSceneSkeletalAnimationTrack)
        asec = atrack.add_section()
        asec.set_range(0, frames)
        params = asec.get_editor_property("params")
        params.set_editor_property("animation", anim)
        asec.set_editor_property("params", params)
        r["face_track"] = True

    audio_track = seq.add_track(unreal.MovieSceneAudioTrack)
    audio_sec = audio_track.add_section()
    audio_sec.set_editor_property("sound", eal.load_asset(AUDIO))
    audio_sec.set_range(0, frames)
    r["audio_track"] = True

    # THE CAMERA CUT - the piece that was missing. Without it the sequence plays but the
    # view stays on the player's camera, which is why Kaia appeared small and far away.
    cam_binding = seq.add_possessable(cam)
    cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
    cut_sec = cut_track.add_section()
    cut_sec.set_range(0, frames)
    cut_sec.set_camera_binding_id(
        unreal.MovieSceneSequenceExtensions.get_binding_id(seq, cam_binding))
    r["camera_cut"] = True

    eal.save_asset(seq_path)
    r["saved"] = seq_path

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###SEQ### %s" % json.dumps(r))
