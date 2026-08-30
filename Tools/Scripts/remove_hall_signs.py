# remove_hall_signs.py - take the signs off the Hall machine.
#
# *** RUN FROM THE OPEN EDITOR WITH L_Office_v02 ALREADY OPEN. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/remove_hall_signs.py"
#
# It refuses to run on any other level and never loads or deletes a LEVEL - Python that
# does crashes this editor. See make_city_from_downtown.py.
#
# ===========================================================================
# WHY, IN WALT'S WORDS.
#
# "i want the player to wonder what the Hall machine is, not get a big sign in their
# face."
#
# He is right, and it is the same instinct that made the toll door invisible instead of
# locked. A sign that explains a machine answers a question the player has not asked yet;
# a strange machine with no label makes them walk over to it. The cathedral already
# proves this works - nobody needed telling what the slot cabinet was.
#
# ---------------------------------------------------------------------------
# IT REPORTS EVERYTHING BEFORE IT REMOVES ANYTHING.
#
# The level is not visible from here, so this lists EVERY actor with "sign" in its label
# along with its class and location, and then deletes only exact matches for the two
# names Walt asked for. If those names have drifted, nothing is deleted and the report
# says what is actually there - which is a far better outcome than guessing at a
# substring and taking a light or a door with it.
#
# SAFE TO GET WRONG: L_Office_v02.umap is tracked in git, so
#     git checkout -- Content/L_Office_v02.umap
# puts anything back. That is the real safety net, not this script's caution.

import json
import traceback

import unreal

LEVEL = "/Game/L_Office_v02"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/remove_hall_signs.json"

# Exact labels to remove. Anything else merely gets reported.
TARGETS = ["SignPlate", "SignCard"]

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level
    if open_level != LEVEL:
        raise Exception("open %s first (File > Open Level > L_Office_v02). This script "
                        "never changes levels itself." % LEVEL)

    actors = eas.get_all_level_actors()
    r["actor_count"] = len(actors)

    # ---- what is actually there -------------------------------------------
    found = []
    for a in actors:
        label = a.get_actor_label()
        if "sign" not in label.lower():
            continue
        loc = a.get_actor_location()
        found.append({
            "label": label,
            "class": a.get_class().get_name(),
            "at": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
            "will_remove": label in TARGETS,
        })
    r["signs_in_level"] = found

    # ---- remove only the exact ones ---------------------------------------
    removed, failed = [], []
    for a in actors:
        label = a.get_actor_label()
        if label not in TARGETS:
            continue
        try:
            # A locked level refuses deletion silently-ish; record the outcome either
            # way rather than assuming the call did anything. That lesson cost an
            # evening on the lighting sublevels.
            eas.destroy_actor(a)
            removed.append(label)
        except Exception as e:
            failed.append({"label": label, "why": str(e)})
    r["removed"] = removed
    r["failed"] = failed

    # ---- prove it -----------------------------------------------------------
    still = [a.get_actor_label() for a in eas.get_all_level_actors()
             if a.get_actor_label() in TARGETS]
    r["still_present"] = still
    r["worked"] = len(still) == 0 and len(removed) > 0

    if not removed and not found:
        r["note"] = ("nothing with 'sign' in its label is in this level - the plate may "
                     "be a COMPONENT inside another actor, or named something else. The "
                     "report above lists what is really here.")
    elif not removed and found:
        r["note"] = ("found signs, but none matched %s exactly - check the labels in "
                     "signs_in_level and tell me which to take." % TARGETS)

    les.save_current_level()
    r["saved"] = True

    del actors, found
    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###SIGNS### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
