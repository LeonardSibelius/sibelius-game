# agent_grants_refactor.py — Kaia hands over REFACTOR.
#
# Walt, 2026-08-19: "those sliding spheres on poles look weird. Can the dancing girls be
# the ones that give you powers when you approach them? they are supposed to be AI agents
# after all. Can we make Kaia the first one who launches the slot machine to get Refactor?"
#
# WHAT THIS DOES. Sets GrantedByAgent on PowerGrant_Refactor to Kaia. At BeginPlay the
# grant then hides its pole, its beacon and its glow, switches its overlap trigger OFF,
# and hands its interface to her: her prompt becomes "[E] ask Kaia for REFACTOR", and that
# press opens the same trial it always did.
#
# NOTHING ELSE MOVES. The grant keeps the stake, the 2250 target, the claim key, the sauce
# and the Refuser alarm. Only the way it is ASKED FOR changes -- from walking within 110cm
# of a floating sphere to pressing E on a person who introduces herself as an AI agent.
#
# WHY IT IS BETTER THAN MOVING THE SPHERE. The powers ARE AI assistance made literal
# (docs/NARRATIVE.md). Mrs. Hall's whole position is that a senior developer should not
# need a machine's help. Taking a forbidden capability from an AI agent is the premise of
# the game happening in front of the player, and it cannot ambush anyone on a staircase.
#
# The shrine does NOT need to be moved first -- it is invisible and inert once bound, so
# it can stay exactly where it is. Move it later if you want it out of the way in the
# editor viewport.
#
# Idempotent. Read the printed payload. Does NOT save the level.
#
# RUN (editor Python console):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/agent_grants_refactor.py"

import json
import unreal

AGENT_HINT = "kaia"       # matched against the actor label, case-insensitive
VERB_HINT = "refactor"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

grant = None
agent = None
grants = []
dancer_like = []

for a in eas.get_all_level_actors():
    try:
        cls = a.get_class().get_name()
        label = a.get_actor_label()
    except Exception:
        continue

    if cls == "PowerGrant":
        verb = None
        try:
            verb = str(a.get_editor_property("power"))
        except Exception:
            pass
        bound = None
        try:
            b = a.get_editor_property("granted_by_agent")
            bound = b.get_actor_label() if b else None
        except Exception:
            pass
        grants.append({"label": label, "power": verb, "granted_by": bound})
        if (verb and VERB_HINT in verb.lower()) or VERB_HINT in label.lower():
            grant = a

    # Dancers are MetaHuman actors we do not own; they are recognised at runtime by a
    # scan, so in the EDITOR they are just actors with a name. Match on the label.
    if AGENT_HINT in label.lower():
        agent = a
    if any(k in label.lower() for k in ("dancer", "mhc_", "kaia", "nyra", "isla", "aisling", "elise")):
        dancer_like.append(label)

if grant is None:
    payload = {"ok": False, "error": "no Refactor PowerGrant in this level", "grants": grants}
elif agent is None:
    payload = {"ok": False,
               "error": "no actor whose label contains '%s'" % AGENT_HINT,
               "hint": "set AGENT_HINT to one of the labels below",
               "dancer_like_actors": sorted(set(dancer_like)),
               "grants": grants}
else:
    try:
        grant.set_editor_property("granted_by_agent", agent)
        ok = True
        err = None
    except Exception as e:
        ok = False
        err = str(e)

    payload = {
        "ok": ok,
        "error": err,
        "grant": grant.get_actor_label(),
        "agent": agent.get_actor_label(),
        "agent_class": agent.get_class().get_name(),
        "effect": "Kaia's prompt becomes '[E] ask Kaia for REFACTOR'; the pole hides "
                  "itself and its trigger stands down at BeginPlay",
        "REMEMBER": "save the level -- this script does not",
        "all_power_grants": grants,
    }

print(json.dumps(payload, indent=2))
