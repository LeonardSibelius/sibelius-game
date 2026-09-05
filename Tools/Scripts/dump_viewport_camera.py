# dump_viewport_camera.py - where is the editor camera standing right now?
#
# *** RUN FROM THE OPEN EDITOR ***
#   Fly the viewport to the shot you want, then in the Cmd box:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/dump_viewport_camera.py"
#
# Output: Saved/viewport_camera.json, and the same line in the Output Log.
#
# ===========================================================================
# WHY THIS EXISTS.
#
# Walt, framing Nyra's spot on Grok: "I am standing at the spot, but see no coordinates
# in the details..."
#
# He is right that there are none. The Details panel describes the SELECTED ACTOR, and
# flying the viewport selects nothing - so the one number he wants, the place he is
# looking from, is the one number Unreal never puts on screen.
#
# Fly to the shot, run this, and the position and rotation come back. That is the whole
# tool, and it is reusable: every future "put something HERE" starts this way.
#
# ROTATION MATTERS AS MUCH AS POSITION. Framing a shot is a direction, not a point - the
# yaw says which way Nyra should face and where the camera should look. Both are
# reported, and the yaw is called out separately because it is the number usually needed.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/viewport_camera.json"

r = {}


def camera_info():
    # The 5.x way first; EditorLevelLibrary is the deprecated fallback and is kept
    # because a deprecation that removes it should degrade to a message, not a crash.
    subsystem = getattr(unreal, "UnrealEditorSubsystem", None)
    if subsystem is not None:
        try:
            return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_level_viewport_camera_info()
        except Exception as e:
            r.setdefault("errors", {})["UnrealEditorSubsystem"] = str(e)

    try:
        return unreal.EditorLevelLibrary.get_level_viewport_camera_info()
    except Exception as e:
        r.setdefault("errors", {})["EditorLevelLibrary"] = str(e)

    return None


try:
    info = camera_info()
    if not info:
        r["error"] = "could not read the viewport camera"
    else:
        loc, rot = info
        r["location"] = [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]
        r["rotation_pitch_yaw_roll"] = [round(rot.pitch, 1), round(rot.yaw, 1), round(rot.roll, 1)]
        r["yaw"] = round(rot.yaw, 1)

        # Paste-ready, because the next thing anyone does with this is put it in a script.
        r["python_vector"] = "unreal.Vector(%.1f, %.1f, %.1f)" % (loc.x, loc.y, loc.z)
        # unreal.Rotator is (roll, pitch, yaw) in Python - NOT the C++ order. Writing it
        # out correctly here stops the next script from spawning something facing the floor.
        r["python_rotator"] = "unreal.Rotator(%.1f, %.1f, %.1f)  # roll, pitch, yaw" % (
            rot.roll, rot.pitch, rot.yaw)
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###CAMERA### %s" % json.dumps(r))
