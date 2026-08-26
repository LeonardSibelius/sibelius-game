# tune_kaia_lights.py — cutscene lighting, in the units the engine actually uses.
#
# *** RUN FROM THE OPEN EDITOR with L_Cine_KaiaIntro loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/tune_kaia_lights.py"
#
# ---------------------------------------------------------------------------
# THE ROOT MISTAKE, RECORDED SO NOBODY REPEATS IT.
#
# build_kaia_intro_shot.py lit her with rect lights at 18000 / 4500 / 9000 and never
# asked what unit that was. An Unreal rect light defaults to an intensity of about 8.
# If those are candelas, she was lit somewhere in the region of a thousand times too
# hard, and every symptom followed from it:
#
#   glowing nostrils   -> subsurface scatter through over-lit tissue
#   glowing chin/beard -> same, after cutting the rim let auto-exposure open further
#   barely visible     -> same again, after locking exposure removed the last
#                         compensation that had been hiding the real level
#
# Three rounds of tuning chased the symptom. Auto-exposure had been quietly absorbing a
# gross error, and each "fix" removed a bit more of the absorption. The lights were
# always the problem.
#
# So: this run PRINTS the intensity units to Saved/kaia_lights_report.json, and sets
# values near the engine's own default. Expect it dark rather than blinding - dark is
# easy to judge and easy to correct.
# ---------------------------------------------------------------------------
#
# TUNING, once the report confirms the units:
#   too dark  -> raise KEY_INTENSITY (double it; light is multiplicative, not additive)
#   too hot   -> halve it
#   flat      -> lower FILL_INTENSITY for deeper shadow on one side
#   lost in the black -> raise RIM_INTENSITY a little, and only a little

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_lights_report.json"

LOCK_EXPOSURE = True
EXPOSURE_VALUE = 1.0       # min == max. Lower = brighter image.

KEY_INTENSITY = 30.0       # ~4x the engine default for a rect light
FILL_INTENSITY = 8.0
RIM_INTENSITY = 5.0

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    wanted = {"Key": KEY_INTENSITY, "Fill": FILL_INTENSITY, "Rim": RIM_INTENSITY}
    found = {}
    cam = None

    for a in eas.get_all_level_actors():
        lbl = a.get_actor_label()
        if lbl in wanted:
            comp = a.rect_light_component
            before = comp.get_editor_property("intensity")
            units = comp.get_editor_property("intensity_units")
            comp.set_editor_property("intensity", wanted[lbl])
            found[lbl] = {
                "before": round(before, 1),
                "after": wanted[lbl],
                "units": str(units),
                "source_w": comp.get_editor_property("source_width"),
                "source_h": comp.get_editor_property("source_height"),
                "attenuation": comp.get_editor_property("attenuation_radius"),
            }
        elif lbl == "CineCam_KaiaIntro":
            cam = a

    r["lights"] = found

    if cam:
        comp = cam.get_cine_camera_component()
        pp = comp.get_editor_property("post_process_settings")
        if LOCK_EXPOSURE:
            pp.set_editor_property("auto_exposure_min_brightness", EXPOSURE_VALUE)
            pp.set_editor_property("override_auto_exposure_min_brightness", True)
            pp.set_editor_property("auto_exposure_max_brightness", EXPOSURE_VALUE)
            pp.set_editor_property("override_auto_exposure_max_brightness", True)
            r["exposure_locked_at"] = EXPOSURE_VALUE
        pp.set_editor_property("auto_exposure_bias", 0.0)
        pp.set_editor_property("override_auto_exposure_bias", True)
        comp.set_editor_property("post_process_settings", pp)

        r["camera"] = {
            "focal_mm": comp.get_editor_property("current_focal_length"),
            "aperture": comp.get_editor_property("current_aperture"),
        }
    else:
        r["missing_camera"] = True

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###LIGHTS### %s" % json.dumps(r))
