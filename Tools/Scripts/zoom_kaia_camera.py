# zoom_kaia_camera.py — frame the cutscene camera on Kaia's head and shoulders.
#
# *** RUN FROM THE OPEN EDITOR with L_Cine_KaiaIntro loaded ***
#   Cmd box (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/zoom_kaia_camera.py"
#
# KEEPS THE CAMERA WHERE WALT PUT IT. It only changes the LENS and the aim - the
# position he flew to by hand is left alone. Zooming rather than dollying also keeps the
# lighting exactly as lit, which moving the camera would not.
#
# BOUNDS, NOT BONES. frame_kaia_intro_camera.py tried to read eye bones and got Z=0 -
# the Face component is not evaluated in the editor, so its bone transforms are
# meaningless there. The actor's bounding box IS real at edit time, and a standing human
# has her eyes at a very predictable fraction of her height.
#
# FRAME_HEIGHT_CM is the knob: 45 = head and shoulders, ~30 = tight face, ~70 = chest.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_zoom_report.json"
FRAME_HEIGHT_CM = 45.0
EYE_FRACTION = 0.93      # eye height as a fraction of standing height
EYELINE_DROP = 3.0       # aim a touch low so the mouth sits centre-frame

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    kaia = cam = None
    for a in eas.get_all_level_actors():
        lbl = a.get_actor_label()
        if lbl == "Kaia":
            kaia = a
        elif lbl == "CineCam_KaiaIntro":
            cam = a
    if not (kaia and cam):
        raise Exception("Open L_Cine_KaiaIntro - Kaia / CineCam_KaiaIntro not found.")

    origin, extent = kaia.get_actor_bounds(False)
    floor_z = origin.z - extent.z
    height = extent.z * 2.0
    eye_z = floor_z + height * EYE_FRACTION
    r["kaia_height_cm"] = round(height, 1)
    r["eye_z"] = round(eye_z, 1)

    if height < 100.0:
        raise Exception("Kaia measures %.0f cm tall - bounds look wrong, refusing to "
                        "reframe on a bad number." % height)

    look_at = unreal.Vector(origin.x, origin.y, eye_z - EYELINE_DROP)
    cam_loc = cam.get_actor_location()
    r["camera_location_unchanged"] = [round(cam_loc.x, 1), round(cam_loc.y, 1), round(cam_loc.z, 1)]

    dx = look_at.x - cam_loc.x
    dy = look_at.y - cam_loc.y
    dz = look_at.z - cam_loc.z
    dist_cm = (dx * dx + dy * dy + dz * dz) ** 0.5
    r["distance_cm"] = round(dist_cm, 1)

    comp = cam.get_cine_camera_component()
    fb = comp.get_editor_property("filmback")
    sensor_h = fb.get_editor_property("sensor_height")
    r["sensor_height_mm"] = round(sensor_h, 2)

    # focal = distance * sensor_height / subject_height  (similar triangles)
    focal = (dist_cm * 10.0) * sensor_h / (FRAME_HEIGHT_CM * 10.0)
    focal = max(12.0, min(300.0, focal))
    comp.set_editor_property("current_focal_length", focal)
    r["focal_mm"] = round(focal, 1)

    # Aim at her eyeline. The old aim was 6.8 degrees up at her chest.
    rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, look_at)
    cam.set_actor_rotation(rot, False)
    r["camera_rotation"] = [round(rot.pitch, 1), round(rot.yaw, 1), round(rot.roll, 1)]

    fs = comp.get_editor_property("focus_settings")
    fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
    fs.set_editor_property("manual_focus_distance", dist_cm)
    comp.set_editor_property("focus_settings", fs)

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###ZOOM### %s" % json.dumps(r))
