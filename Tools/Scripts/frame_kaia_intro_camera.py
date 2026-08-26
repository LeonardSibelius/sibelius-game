# frame_kaia_intro_camera.py — point the cutscene camera at Kaia's FACE.
#
# *** RUN FROM THE OPEN EDITOR with L_Cine_KaiaIntro loaded ***
#   Cmd box (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/frame_kaia_intro_camera.py"
#
# Safe to re-run, and it touches ONLY the camera - Kaia, the lights and the sequence
# are left alone, so re-framing costs nothing else.
#
# WHY BONES AND NOT COORDINATES. The first pass guessed head height and facing and got
# both wrong: MetaHumans do not agree with actor +X about forward. So this asks HER -
# eye bones for where the face is, nose minus eyes for which way it points. Same method
# UDancerAgentComponent uses for the in-game talk close-up (GetEyeCenter /
# GetFaceForward) on these same characters.
#
# USE get_socket_location, NOT get_bone_transform. get_bone_transform came back in a
# space where her eyes sat at Z=0 - the floor - and the camera was duly parked on the
# floor aiming at nothing (2026-08-25). get_socket_location takes a bone name and
# returns WORLD space, which is what we actually need.
#
# FRAME_HEIGHT_CM is the knob. 45 is head-and-shoulders; ~30 is a tight face; ~70 shows
# her chest. Distance is DERIVED from the real filmback, so framing survives a lens change.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_frame_report.json"

FOCAL_MM = 85.0         # portrait lens - flatters a face, no wide-angle nose
FRAME_HEIGHT_CM = 45.0  # visible vertical slice of her: head + shoulders
EYELINE_DROP = 4.0      # aim a touch below the eyes so the mouth sits centre-frame

r = {}


def socket(comp, names):
    """World-space location of the first bone/socket that exists and is not at origin."""
    for n in names:
        try:
            loc = comp.get_socket_location(n)
        except Exception:
            continue
        if loc is None:
            continue
        if abs(loc.x) + abs(loc.y) + abs(loc.z) > 0.001:
            return loc
    return None


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    kaia = cam = None
    for a in eas.get_all_level_actors():
        lbl = a.get_actor_label()
        if lbl == "Kaia":
            kaia = a
        elif lbl == "CineCam_KaiaIntro":
            cam = a
    r["found_kaia"] = kaia is not None
    r["found_cam"] = cam is not None
    if not (kaia and cam):
        r["error"] = "Kaia or CineCam_KaiaIntro not found in this level."
        return

    face = None
    for c in kaia.get_components_by_class(unreal.SkeletalMeshComponent):
        n = c.get_name()
        if "Face" in n and "PostProcess" not in n:
            face = c
            break
    r["face_component"] = face.get_name() if face else None
    if not face:
        r["error"] = "No Face skeletal mesh component."
        return

    le = socket(face, ["FACIAL_L_Eye", "FACIAL_L_EyeParallel"])
    re_ = socket(face, ["FACIAL_R_Eye", "FACIAL_R_EyeParallel"])
    nose = socket(face, ["FACIAL_C_NoseTip", "FACIAL_C_Nose", "FACIAL_C_NoseLower"])
    head = socket(face, ["head", "FACIAL_C_FacialRoot"])
    r["bones"] = {"l_eye": le is not None, "r_eye": re_ is not None,
                  "nose": nose is not None, "head": head is not None}

    if le and re_:
        eyes = unreal.Vector((le.x + re_.x) / 2, (le.y + re_.y) / 2, (le.z + re_.z) / 2)
    elif head:
        eyes = head
    else:
        eyes = face.bounds.origin
    r["eye_center"] = [round(eyes.x, 1), round(eyes.y, 1), round(eyes.z, 1)]

    # SANITY GATE. A standing MetaHuman's eyes are ~150-175 cm up. Anything near the
    # floor means the lookup lied again, and moving the camera on a bad number just
    # hides the problem behind a black render.
    if eyes.z < 50.0:
        r["error"] = ("Eye height %.1f cm is impossible for a standing character - "
                      "bone lookup failed. Camera NOT moved." % eyes.z)
        return

    if nose:
        fwd = unreal.Vector(nose.x - eyes.x, nose.y - eyes.y, 0.0)
        if fwd.length() < 1.0:
            fwd = kaia.get_actor_forward_vector()
    else:
        fwd = kaia.get_actor_forward_vector()
    fwd = unreal.Vector(fwd.x, fwd.y, 0.0).normal()
    r["face_forward"] = [round(fwd.x, 3), round(fwd.y, 3)]

    comp = cam.get_cine_camera_component()
    comp.set_editor_property("current_focal_length", FOCAL_MM)

    fb = comp.get_editor_property("filmback")
    sensor_h = fb.get_editor_property("sensor_height")     # mm
    r["sensor_height_mm"] = round(sensor_h, 2)

    # Similar triangles: distance = focal * subject_height / sensor_height.
    dist_cm = (FOCAL_MM * (FRAME_HEIGHT_CM * 10.0) / sensor_h) / 10.0
    r["distance_cm"] = round(dist_cm, 1)

    look_at = unreal.Vector(eyes.x, eyes.y, eyes.z - EYELINE_DROP)
    cam_loc = unreal.Vector(look_at.x + fwd.x * dist_cm,
                            look_at.y + fwd.y * dist_cm,
                            eyes.z)
    cam.set_actor_location(cam_loc, False, False)

    rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, look_at)
    cam.set_actor_rotation(rot, False)

    fs = comp.get_editor_property("focus_settings")
    fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
    fs.set_editor_property("manual_focus_distance", dist_cm)
    comp.set_editor_property("focus_settings", fs)

    r["camera_location"] = [round(cam_loc.x, 1), round(cam_loc.y, 1), round(cam_loc.z, 1)]
    r["camera_rotation"] = [round(rot.pitch, 1), round(rot.yaw, 1), round(rot.roll, 1)]

    les.save_current_level()
    r["saved"] = True


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###FRAME### %s" % json.dumps(r))
