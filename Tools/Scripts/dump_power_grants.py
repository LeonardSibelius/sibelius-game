# dump_power_grants.py — where every power shrine is, and how it is obtained.
#
# READ-ONLY. Changes nothing, saves nothing.
#
# Written because a guess was wrong. The Refactor grant was assumed to be the one
# ambushing the player on the staircase, on the reasoning that Refactor is Chapter 2 and
# the stairs lead to the second floor. Binding it to Kaia worked -- and the staircase
# still produced a slot machine, because the shrine there is one of the OTHER four.
#
# Chapter order is not level layout. This prints the layout.
#
# For each APowerGrant: label, verb, world location, whether an AI agent now grants it,
# whether its overlap trigger is still live, and its distance to the PlayerStart and to
# every dancer in the level -- so "which one is on the stairs" is answered by numbers.
#
# RUN (editor Python console):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/dump_power_grants.py"

import json
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

grants = []
dancers = []
starts = []

DANCER_HINTS = ("kaia", "nyra", "isla", "aisling", "elise", "dancer", "mhc_")


def loc_of(a):
    v = a.get_actor_location()
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def dist(a, b):
    return round(unreal.Vector(a[0] - b[0], a[1] - b[1], a[2] - b[2]).length(), 1)


for a in eas.get_all_level_actors():
    try:
        cls = a.get_class().get_name()
        label = a.get_actor_label()
    except Exception:
        continue

    low = label.lower()

    if cls == "PowerGrant":
        rec = {"label": label, "loc": loc_of(a)}
        # Unreal's Python bindings STRIP the leading 'b' from boolean UPROPERTYs and
        # snake_case the rest: bGrantsPower -> grants_power. Asking for "b_grants_power"
        # throws, the field comes back "?", and anything derived from it is silently
        # wrong -- which is exactly what happened to CAN_STILL_AMBUSH on the first run.
        # Try both spellings and record which one answered.
        for cpp_name, key in (("power", "power"),
                              ("bGrantsPower", "grants_power"),
                              ("bSlotTrial", "slot_trial"),
                              ("bShowBeacon", "show_beacon")):
            snake = key if cpp_name.startswith("b") else cpp_name
            rec[key] = "?"
            for candidate in (snake, cpp_name):
                try:
                    rec[key] = str(a.get_editor_property(candidate))
                    break
                except Exception:
                    continue
        try:
            g = a.get_editor_property("granted_by_agent")
            rec["granted_by_agent"] = g.get_actor_label() if g else None
        except Exception:
            rec["granted_by_agent"] = "?"
        grants.append(rec)

    elif cls == "PlayerStart":
        starts.append({"label": label, "loc": loc_of(a)})

    elif any(h in low for h in DANCER_HINTS):
        dancers.append({"label": label, "loc": loc_of(a)})

# Distances make the layout answerable without flying the viewport around.
for g in grants:
    if starts:
        g["dist_to_playerstart"] = dist(g["loc"], starts[0]["loc"])
    near = sorted(
        ({"dancer": d["label"], "dist": dist(g["loc"], d["loc"])} for d in dancers),
        key=lambda r: r["dist"],
    )
    g["nearest_dancers"] = near[:3]
    # A grant with no agent still has a live overlap trigger, so it can still take the
    # screen off a player who merely walks past it. Keyed on granted_by_agent ALONE:
    # the first version also required grants_power == True, which was unreadable and
    # made the flag read false for every shrine in the level, including the four that
    # could very much still ambush.
    g["CAN_STILL_AMBUSH"] = g.get("granted_by_agent") in (None, "None")

payload = {
    "player_starts": starts,
    "power_grants": sorted(grants, key=lambda g: g.get("dist_to_playerstart", 1e9)),
    "dancers": dancers,
    "note": "power_grants are sorted NEAREST-FIRST to the PlayerStart. The one the player "
            "meets on the way out of the living room is at the top.",
}

text = json.dumps(payload, indent=2)

# WRITE TO A FILE, do not rely on print().
#
# print() from the editor's Cmd box lands in the OUTPUT LOG panel, which is not open by
# default -- Walt ran this and saw nothing at all, which looked like the script failing
# when it had actually worked. A file can be read afterwards by anyone, including me,
# and it survives closing the editor.
out_path = unreal.Paths.project_saved_dir() + "power_grants.json"
try:
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[dump_power_grants] wrote " + out_path)
except Exception as e:
    unreal.log_error("[dump_power_grants] could not write %s: %s" % (out_path, e))

print(text)
