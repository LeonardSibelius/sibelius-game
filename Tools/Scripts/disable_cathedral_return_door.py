# disable_cathedral_return_door.py — switch off the cathedral's old return door.
#
# Walt, 2026-08-19: "long ago, there used to be a wooden door on that cathedral and you
# pressed 'E' to open it. I use [o] now to get home so drop that prompt and that action
# there, [E] is saved for playing the slot machine when you get past the altar."
#
# The wooden door is gone; the ACathedralDoor that served it survives in L_Cathedral with
# its PromptText overridden to "Return to the office [E]". O has been the way home from
# any world for a long time, and the HUD draws its own "[O] Back to Office" hint there, so
# the surviving prompt offered a second undocumented exit that contradicted it -- and it
# spent E, which in that room belongs to the slot machine past the altar.
#
# Sets bInteractive = False on any ACathedralDoor in the OPEN level whose prompt mentions
# returning/office. Leaves the attic -> cathedral door alone: that one still says "Enter
# the cathedral [E]" and must keep working.
#
# OPEN L_Cathedral FIRST. This edits the level that is currently loaded.
#
# Idempotent, read-and-report, writes Saved/cathedral_doors.json. Does NOT save the level.
#
# RUN (editor Python console, with L_Cathedral open):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/disable_cathedral_return_door.py"

import json
import unreal

RETURN_WORDS = ("return", "office", "home")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

try:
    level_name = ues.get_editor_world().get_name()
except Exception:
    level_name = "?"

doors = []
changed = []

for a in eas.get_all_level_actors():
    try:
        if a.get_class().get_name() != "CathedralDoor":
            continue
        label = a.get_actor_label()
    except Exception:
        continue

    prompt = ""
    for candidate in ("prompt_text", "PromptText"):
        try:
            prompt = str(a.get_editor_property(candidate))
            break
        except Exception:
            continue

    interactive = None
    for candidate in ("interactive", "bInteractive"):
        try:
            interactive = a.get_editor_property(candidate)
            break
        except Exception:
            continue

    is_return = any(w in prompt.lower() for w in RETURN_WORDS)
    rec = {"label": label, "prompt": prompt, "interactive_before": interactive,
           "looks_like_the_return_door": is_return}

    if is_return:
        done = False
        for candidate in ("interactive", "bInteractive"):
            try:
                a.set_editor_property(candidate, False)
                rec["set_via"] = candidate
                done = True
                break
            except Exception:
                continue
        rec["disabled"] = done
        if not done:
            rec["error"] = ("could not set bInteractive -- is the editor running a build "
                            "that has the property? rebuild if this is an old DLL")
        else:
            changed.append(label)

    doors.append(rec)

payload = {
    "level": level_name,
    "doors": doors,
    "disabled": changed,
    "note": "Only doors whose prompt mentions return/office/home are touched. The attic -> "
            "cathedral door says 'Enter the cathedral [E]' and is left working.",
    "REMEMBER": "save the level -- this script does not",
}

if not doors:
    payload["error"] = ("no ACathedralDoor in the open level -- open L_Cathedral and run "
                        "this again (it edits whatever level is currently loaded)")

text = json.dumps(payload, indent=2, default=str)
path = unreal.Paths.project_saved_dir() + "cathedral_doors.json"
try:
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[disable_cathedral_return_door] wrote " + path)
except Exception as e:
    unreal.log_error("[disable_cathedral_return_door] could not write %s: %s" % (path, e))

print(text)
