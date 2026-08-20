# agents_grant_powers.py — the AI agents hand over the powers you meet by ACCIDENT.
#
# Kaia already gives REFACTOR (agent_grants_refactor.py). This does the other two that a
# player walks into rather than goes looking for:
#
#     Isla -> DEPLOY       the staircase ambush: 496 units from the PlayerStart, the very
#                          first shrine on the way out of the living room, and Chapter 5's
#                          power. Walt met it climbing the stairs and got a staked slot
#                          machine plus a Refuser wave, unasked.
#     Nyra -> TEST-DRIVE   next nearest at 683, the one you meet immediately after.
#
# DELIBERATELY LEFT AS SPHERES:
#
#     COMPILE    lives in the library and is the payoff of the book hunt -- you went
#                looking for it. Walt: "Compile is given by that bright little sphere in
#                the library after you collect 12 books. Can that one stay there?" It can.
#                A reward you walked to is the opposite of an ambush.
#     GENERATE   1227 units out, deep enough to be a destination rather than an accident.
#
# The distinction is the whole point: this is not "move every sphere to a dancer", it is
# "nothing should take the screen off a player who was doing something else".
#
# Distances are measured, not assumed -- see Saved/power_grants.json from
# dump_power_grants.py. An earlier guess put Refactor on the staircase on the reasoning
# that Refactor is Chapter 2 and the stairs lead to the second floor. It was Deploy.
# Chapter order is not level layout.
#
# Idempotent. Writes Saved/agents_grant_powers.json as well as printing, because print()
# lands in the Output Log panel which is not open by default. Does NOT save the level.
#
# RUN (editor Python console):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/agents_grant_powers.py"

import json
import unreal

# verb-substring -> dancer-label-substring. Case-insensitive, both sides.
PAIRINGS = {
    "deploy": "isla",
    "test_drive": "nyra",
}

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

grants = []
actors_by_label = {}

for a in eas.get_all_level_actors():
    try:
        cls = a.get_class().get_name()
        label = a.get_actor_label()
    except Exception:
        continue
    actors_by_label[label.lower()] = a
    if cls == "PowerGrant":
        verb = ""
        try:
            verb = str(a.get_editor_property("power"))
        except Exception:
            pass
        grants.append((a, label, verb))

results = []

for verb_hint, agent_hint in PAIRINGS.items():
    grant = None
    for a, label, verb in grants:
        hay = (verb + " " + label).lower().replace("-", "_")
        if verb_hint in hay:
            grant = a
            grant_label = label
            break

    agent = None
    for label_low, a in actors_by_label.items():
        if agent_hint in label_low:
            agent = a
            break

    if grant is None:
        results.append({"pairing": "%s -> %s" % (verb_hint, agent_hint),
                        "ok": False, "error": "no PowerGrant matching '%s'" % verb_hint})
        continue
    if agent is None:
        results.append({"pairing": "%s -> %s" % (verb_hint, agent_hint),
                        "ok": False, "error": "no actor whose label contains '%s'" % agent_hint,
                        "labels_seen": sorted(k for k in actors_by_label
                                              if any(h in k for h in ("kaia", "nyra", "isla",
                                                                     "dancer", "mhc_")))})
        continue

    try:
        grant.set_editor_property("granted_by_agent", agent)
        results.append({"pairing": "%s -> %s" % (verb_hint, agent_hint), "ok": True,
                        "grant": grant_label, "agent": agent.get_actor_label()})
    except Exception as e:
        results.append({"pairing": "%s -> %s" % (verb_hint, agent_hint),
                        "ok": False, "error": str(e)})

# Report EVERY grant afterwards, bound or not, so what is still live is visible.
final = []
for a, label, verb in grants:
    try:
        g = a.get_editor_property("granted_by_agent")
        bound = g.get_actor_label() if g else None
    except Exception:
        bound = "?"
    final.append({"grant": label, "power": verb, "granted_by_agent": bound,
                  "still_a_live_sphere": bound in (None, "None")})

payload = {
    "results": results,
    "all_grants_after": final,
    "expected_to_stay_spheres": ["PowerGrant_Compile (library, earned by the book hunt)",
                                 "PowerGrant_Generate (deep, a destination)"],
    "REMEMBER": "save the level -- this script does not",
}

text = json.dumps(payload, indent=2)
out_path = unreal.Paths.project_saved_dir() + "agents_grant_powers.json"
try:
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[agents_grant_powers] wrote " + out_path)
except Exception as e:
    unreal.log_error("[agents_grant_powers] could not write %s: %s" % (out_path, e))

print(text)
