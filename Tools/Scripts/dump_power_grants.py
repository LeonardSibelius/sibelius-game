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
        for prop, key in (("power", "power"),
                          ("b_grants_power", "grants_power"),
                          ("b_slot_trial", "slot_trial"),
                          ("b_show_beacon", "show_beacon")):
            try:
                rec[key] = str(a.get_editor_property(prop))
            except Exception:
                rec[key] = "?"
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
    # A grant that still has a live trigger and no agent is one that can still ambush.
    g["CAN_STILL_AMBUSH"] = (g.get("granted_by_agent") in (None, "None")
                             and g.get("grants_power", "").lower().endswith("true"))

payload = {
    "player_starts": starts,
    "power_grants": sorted(grants, key=lambda g: g.get("dist_to_playerstart", 1e9)),
    "dancers": dancers,
    "note": "power_grants are sorted NEAREST-FIRST to the PlayerStart. The one the player "
            "meets on the way out of the living room is at the top.",
}

print(json.dumps(payload, indent=2))
