# fix_kaia_cue_link.py — point SequenceCue_KaiaIntro at the actual sequence.
#
# *** RUN FROM THE OPEN EDITOR with L_Cine_KaiaIntro loaded ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/fix_kaia_cue_link.py"
#
# THE BUG THIS FIXES. place_kaia_sequence_cue.py set the soft reference from
# "/Game/Cinematics/LS_Kaia_Intro" - a PACKAGE path. A soft object reference needs the
# OBJECT path, which repeats the asset name: "/Game/Cinematics/LS_Kaia_Intro.LS_Kaia_Intro".
# The package form silently resolves to nothing, so the cue logged "has no sequence to
# play" and travelled straight to the office - which on screen looked exactly like the
# cutscene not existing.
#
# Assigning the LOADED ASSET instead of a path sidesteps the whole question: Python
# converts a UObject to the right soft pointer itself.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_link_report.json"
SEQUENCE = "/Game/Cinematics/LS_Kaia_Intro"
CUE_LABEL = "SequenceCue_KaiaIntro"

r = {}

try:
    eal = unreal.EditorAssetLibrary
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    if not eal.does_asset_exist(SEQUENCE):
        raise Exception("sequence missing: %s" % SEQUENCE)
    seq = eal.load_asset(SEQUENCE)
    r["sequence_loaded"] = seq is not None

    cue = None
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == CUE_LABEL:
            cue = a
            break
    if not cue:
        raise Exception("%s not in this level - open L_Cine_KaiaIntro." % CUE_LABEL)

    cue.set_editor_property("sequence", seq)

    # Read it straight back. "I set it" is not evidence; "it is set" is.
    back = cue.get_editor_property("sequence")
    r["assigned"] = str(back) if back else None
    r["ok"] = back is not None

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###LINK### %s" % json.dumps(r))
