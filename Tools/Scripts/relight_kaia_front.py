# relight_kaia_front.py — put the light in FRONT of her, where the camera is.
#
# *** RUN FROM THE OPEN EDITOR with L_Cine_KaiaIntro loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/relight_kaia_front.py"
#
# Safe to re-run. Moves and aims the three lights; touches nothing else.
#
# ---------------------------------------------------------------------------
# WHY THE OLD SETUP LIT THE WRONG SIDE OF HER HEAD.
#
# build_kaia_intro_shot.py placed the lights at fixed world coordinates - key at +X,
# rim at -X - written before anyone knew where the camera would end up. Walt then flew
# the camera by hand to -X. So the key finished up BEHIND her from the camera's point of
# view, and what looked like a hot rim light on her hair was the key light itself. Her
# face was lit by spill.
#
# Lights belong in CAMERA space, not world space. This computes the camera-to-subject
# axis and places a standard three-point rig around it, so the key is always in front
# wherever the camera stands. Re-run it after any reframing.
#
#   KEY   in front, one side, above eye level  - shapes the face
#   FILL  in front, other side, eye level, dim - opens the shadow
#   RIM   behind, opposite the key, high       - separates her from the black
# ---------------------------------------------------------------------------
#
# KEY_SIDE_DEG: how far off the camera axis the key sits. 0 is flat and unflattering
# (passport photo); 30-40 gives a cheekbone; past 60 you are back to side-lighting.

import unreal, json, traceback, math

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_relight_report.json"

KEY_INTENSITY = 17.0
FILL_INTENSITY = 5.0
RIM_INTENSITY = 2.0        # was doing the work of a key; now just an edge

KEY_SIDE_DEG = 32.0        # degrees off the camera axis
KEY_DIST = 150.0           # cm from her face
KEY_HEIGHT = 45.0          # cm above her eyeline

FILL_SIDE_DEG = 45.0
FILL_DIST = 175.0

RIM_DIST = 110.0
RIM_HEIGHT = 70.0

EYE_FRACTION = 0.93

r = {}


def place(actor, loc, target, intensity):
    actor.set_actor_location(loc, False, False)
    actor.set_actor_rotation(
        unreal.MathLibrary.find_look_at_rotation(loc, target), False)
    c = actor.rect_light_component
    c.set_editor_property("intensity", intensity)
    return [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]


try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    kaia = cam = key = fill = rim = None
    for a in eas.get_all_level_actors():
        lbl = a.get_actor_label()
        if lbl == "Kaia":      kaia = a
        elif lbl == "CineCam_KaiaIntro": cam = a
        elif lbl == "Key":     key = a
        elif lbl == "Fill":    fill = a
        elif lbl == "Rim":     rim = a

    missing = [n for n, o in (("Kaia", kaia), ("CineCam_KaiaIntro", cam),
                              ("Key", key), ("Fill", fill), ("Rim", rim)) if not o]
    if missing:
        raise Exception("not found in this level: %s" % ", ".join(missing))

    # Her face, from the bounding box - bones read as zero in the editor.
    origin, extent = kaia.get_actor_bounds(False)
    eye_z = (origin.z - extent.z) + (extent.z * 2.0) * EYE_FRACTION
    target = unreal.Vector(origin.x, origin.y, eye_z)
    r["target"] = [round(target.x, 1), round(target.y, 1), round(target.z, 1)]

    # Camera axis, flattened - the direction the LIGHT should broadly come from.
    cam_loc = cam.get_actor_location()
    to_cam = unreal.Vector(cam_loc.x - target.x, cam_loc.y - target.y, 0.0).normal()
    # Perpendicular, for swinging the key off-axis.
    right = unreal.Vector(-to_cam.y, to_cam.x, 0.0)
    r["camera_side"] = [round(to_cam.x, 3), round(to_cam.y, 3)]

    def offset(deg, dist, height, flip=False):
        a = math.radians(deg) * (-1.0 if flip else 1.0)
        dx = to_cam.x * math.cos(a) - to_cam.y * math.sin(a)
        dy = to_cam.x * math.sin(a) + to_cam.y * math.cos(a)
        return unreal.Vector(target.x + dx * dist,
                             target.y + dy * dist,
                             target.z + height)

    r["key"] = place(key, offset(KEY_SIDE_DEG, KEY_DIST, KEY_HEIGHT), target, KEY_INTENSITY)
    r["fill"] = place(fill, offset(FILL_SIDE_DEG, FILL_DIST, 0.0, flip=True), target, FILL_INTENSITY)

    # Rim goes BEHIND her: the camera direction, negated, opposite side from the key.
    behind = unreal.Vector(-to_cam.x, -to_cam.y, 0.0)
    rim_loc = unreal.Vector(target.x + (behind.x * 0.8 - right.x * 0.6) * RIM_DIST,
                            target.y + (behind.y * 0.8 - right.y * 0.6) * RIM_DIST,
                            target.z + RIM_HEIGHT)
    r["rim"] = place(rim, rim_loc, target, RIM_INTENSITY)

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###RELIGHT### %s" % json.dumps(r))
