# place_kaia_intro_cue.py — drop the opening cutscene into the office.
#
# *** RUN FROM THE OPEN EDITOR ***
#   Cmd box (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_kaia_intro_cue.py"
#
# Opens L_Office_v02, places one AVideoCue, saves. Idempotent - re-running replaces
# the existing cue rather than stacking a second one.
#
# WHERE IT SITS DOES NOT MATTER. AVideoCue draws through the HUD and its audio is
# non-spatialised (bAllowSpatialization false), so the actor is a trigger, not a
# speaker. It goes at the origin because that is the least surprising place to find it.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_cue_report.json"
LEVEL = "/Game/L_Office_v02"
LABEL = "VideoCue_KaiaIntro"

r = {}

try:
    eal = unreal.EditorAssetLibrary
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not eal.does_asset_exist(LEVEL):
        raise Exception("Level not found: %s" % LEVEL)

    les.load_level(LEVEL)
    r["level"] = LEVEL

    removed = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == LABEL:
            eas.destroy_actor(a)
            removed += 1
    r["removed_existing"] = removed

    cue = eas.spawn_actor_from_class(
        unreal.VideoCue, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not cue:
        raise Exception("spawn_actor_from_class returned None - is AVideoCue compiled?")

    cue.set_actor_label(LABEL)
    cue.set_editor_property("video_file_name", "kaia_intro.mp4")
    cue.set_editor_property("cue_id", "KaiaIntro")
    cue.set_editor_property("play_on_begin_play", True)
    cue.set_editor_property("start_delay", 0.75)
    cue.set_editor_property("skippable", True)
    r["placed"] = LABEL

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###CUE### %s" % json.dumps(r))
