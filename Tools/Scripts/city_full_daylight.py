# city_full_daylight.py - make L_City always arrive in daylight.
#
# *** RUN FROM THE OPEN EDITOR WITH L_City ALREADY OPEN. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/city_full_daylight.py"
#
# It never loads or deletes a level - Python that does crashes this editor
# (FPyReferenceCollector / EditorServer.cpp:2524). See make_city_from_downtown.py.
#
# ===========================================================================
# THE PROBLEM.
#
# Downtown West's demo map streams TWO lighting sublevels as alternatives -
# Sub-Levels/Daytime_Lighting and Sub-Levels/Nightime_Lighting - and L_City inherited
# both when it was duplicated. Which one you get at RUNTIME is decided by each streaming
# level's own load/visible flags, NOT by what is ticked in the editor's Levels panel.
# That is why the street looks like noon in the viewport and like dusk after travelling
# there with [>]: the editor was showing one and the game was loading the other.
#
# Walt, arriving at his own ending: "the city is in dusk now and the dancers are in
# shadow." The last thing the game shows him should not be a coin flip.
#
# ---------------------------------------------------------------------------
# WHAT THIS DOES: day ON and loaded, night OFF and not loaded. Both are set through
# whichever property names this engine build actually exposes, and every one is READ
# BACK - ULevelStreaming's flags are private with meta-accessors and the Python names
# have moved between versions. A setter that reports success without a readback has cost
# this project an evening already.
#
# IF THE READBACK SAYS false, do it by hand instead - it is three clicks and it is
# guaranteed:
#     Window > Levels, right-click Daytime_Lighting  > Change Streaming Method > Always
#     Loaded, then right-click Nightime_Lighting > Remove Selected Levels. Save.

import json
import traceback

import unreal

LEVEL = "/Game/Maps/L_City"
DAY = "/Game/Downtown_West/Maps/Sub-Levels/Daytime_Lighting"
NIGHT = "/Game/Downtown_West/Maps/Sub-Levels/Nightime_Lighting"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/city_full_daylight.json"

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    world = ues.get_editor_world()
    open_level = world.get_path_name().split(".")[0]
    r["open_level"] = open_level
    if open_level != LEVEL:
        raise Exception("open %s first (File > Open Level). This script never changes "
                        "levels itself." % LEVEL)

    def tune(package, want_on):
        """Set one streaming level's load+visible flags, and report what actually stuck."""
        out = {"package": package, "wanted": want_on}
        try:
            sl = unreal.GameplayStatics.get_streaming_level(world, unreal.Name(package))
        except Exception as e:
            out["error"] = "get_streaming_level failed: %s" % e
            return out
        if not sl:
            out["error"] = "not a streaming level of L_City"
            return out

        def put(names, value):
            for n in names:
                try:
                    sl.set_editor_property(n, value)
                    return n
                except Exception:
                    continue
            return None

        out["visible_set_via"] = put(
            ["should_be_visible", "initially_visible", "b_should_be_visible"], want_on)
        out["loaded_set_via"] = put(
            ["should_be_loaded", "initially_loaded", "b_should_be_loaded"], want_on)

        def get(names):
            for n in names:
                try:
                    return bool(sl.get_editor_property(n))
                except Exception:
                    continue
            return None

        out["visible_now"] = get(["should_be_visible", "initially_visible", "b_should_be_visible"])
        out["loaded_now"] = get(["should_be_loaded", "initially_loaded", "b_should_be_loaded"])
        out["ok"] = (out["visible_now"] == want_on and out["loaded_now"] == want_on)
        return out

    r["daytime"] = tune(DAY, True)
    r["nighttime"] = tune(NIGHT, False)

    # ---- how many suns are actually in this world right now? --------------
    # Two directional lights fighting is the other way a street ends up wrong, and it
    # is worth being able to tell the two causes apart without another round trip.
    suns = []
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.DirectionalLight):
            c = a.get_component_by_class(unreal.DirectionalLightComponent)
            suns.append({
                "label": a.get_actor_label(),
                "pitch": round(a.get_actor_rotation().pitch, 1),
                "intensity": round(float(c.get_editor_property("intensity")), 2) if c else None,
                "visible": bool(c.is_visible()) if c else None,
            })
    r["directional_lights"] = suns
    r["sun_count"] = len(suns)

    les.save_current_level()
    r["saved"] = True
    r["worked"] = bool(r["daytime"].get("ok") and r["nighttime"].get("ok"))
    r["next"] = ("If worked is false, do it by hand: Window > Levels, set "
                 "Daytime_Lighting to Always Loaded and remove Nightime_Lighting, "
                 "then save. Then travel to the city with [>] and check the light.")

    del world, suns
    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###DAYLIGHT### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
