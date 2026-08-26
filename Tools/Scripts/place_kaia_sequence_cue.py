# place_kaia_sequence_cue.py — wire the opening cutscene into L_Cine_KaiaIntro.
#
# *** RUN FROM THE OPEN EDITOR ***
#   Cmd box (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_kaia_sequence_cue.py"
#
# Adds two actors to the cutscene level and saves:
#
#   SequenceCue_KaiaIntro   plays LS_Kaia_Intro, then travels to L_Office_v02
#   PlayerStart             so the pawn spawns 5 m away instead of inside Kaia
#
# The PlayerStart matters more than it looks. Without one the pawn spawns at the world
# origin, which is exactly where Kaia is standing - a first-person camera inside her
# head, and a capsule fighting her collision for the whole shot. The camera cut hides
# it, but "hidden" is not "not happening".
#
# Idempotent: re-running replaces both rather than stacking duplicates.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_seqcue_report.json"
LEVEL = "/Game/Cinematics/L_Cine_KaiaIntro"
SEQUENCE = "/Game/Cinematics/LS_Kaia_Intro"
CUE_LABEL = "SequenceCue_KaiaIntro"
START_LABEL = "CutsceneStart"

r = {}

try:
    eal = unreal.EditorAssetLibrary
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    for p in (LEVEL, SEQUENCE):
        if not eal.does_asset_exist(p):
            raise Exception("missing asset: %s" % p)

    les.load_level(LEVEL)

    removed = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_label() in (CUE_LABEL, START_LABEL):
            eas.destroy_actor(a)
            removed += 1
    r["removed_existing"] = removed

    # Somewhere to stand that is not inside the leading lady.
    start = eas.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-500, 0, 90), unreal.Rotator(0, 0, 0))
    if not start:
        raise Exception("PlayerStart spawn failed")
    start.set_actor_label(START_LABEL)

    cue = eas.spawn_actor_from_class(
        unreal.SequenceCue, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not cue:
        raise Exception("spawn failed - is ASequenceCue compiled?")

    cue.set_actor_label(CUE_LABEL)
    cue.set_editor_property("sequence", unreal.SoftObjectPath(SEQUENCE))
    cue.set_editor_property("next_level", "L_Office_v02")
    cue.set_editor_property("cue_id", "KaiaIntro")
    cue.set_editor_property("play_on_begin_play", True)
    cue.set_editor_property("start_delay", 0.5)
    cue.set_editor_property("skippable", True)
    r["placed"] = [CUE_LABEL, START_LABEL]

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###SEQCUE### %s" % json.dumps(r))
