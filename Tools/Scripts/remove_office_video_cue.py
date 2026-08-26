# remove_office_video_cue.py — take the abandoned AVideoCue out of the office.
#
# *** RUN FROM THE OPEN EDITOR ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/remove_office_video_cue.py"
#
# The opening cutscene moved from a pre-rendered mp4 (AVideoCue, in the office) to a
# live Level Sequence (ASequenceCue, in L_Cine_KaiaIntro). The office actor was left
# behind.
#
# It is currently harmless ONLY BY ACCIDENT: both cues use the id "KaiaIntro", and
# ASequenceCue marks that played before it travels, so the office one sees the flag and
# stays quiet. Change either id and the office would try to play a video that never
# displays, freezing the player for MaxSeconds on arrival. Luck is not a design.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/office_cue_cleanup.json"
LEVEL = "/Game/L_Office_v02"
LABEL = "VideoCue_KaiaIntro"

r = {}
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    les.load_level(LEVEL)
    removed = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == LABEL:
            eas.destroy_actor(a)
            removed += 1
    r["removed"] = removed

    if removed:
        les.save_current_level()
        r["saved"] = True
    else:
        r["note"] = "nothing to remove - already clean"

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###CLEANUP### %s" % json.dumps(r))
