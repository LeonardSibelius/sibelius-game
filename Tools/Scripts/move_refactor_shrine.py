# move_refactor_shrine.py — take the REFACTOR power grant off the staircase.
#
# WHY. APowerGrant fires on OVERLAP, not on E: walking within its 110cm trigger opens
# Celestial Fortune with a staked trial and rings the hall alarm. That was tolerable when
# the stairs were somewhere you arrived deliberately. v0.9.3 moved the spawn to the living
# room, which made the staircase the way OUT of the opening -- so the second thing a brand
# new player met was a forced gamble plus a Refuser wave, unasked and unavoidable on a
# narrow flight of stairs.
#
# Walt, 2026-08-19: "move the shrine off the stairs ... put the refactor one next to Kaia
# I suppose."
#
# WHERE IT GOES. Beside Kaia in the hallway, offset along HER RIGHT so it does not sit in
# whatever she faces, and far enough out that greeting her does not trip it -- that would
# just move the same bug. The trigger is 110cm and the player stands ~100cm from her to
# press E, so anything under ~220cm reproduces the problem. Default is 260cm.
#
# Height is preserved as height-ABOVE-FLOOR, not as a raw Z: the stairs and the hallway
# are not necessarily the same storey, and copying the Z would bury it or float it.
#
# Idempotent: re-running moves it to the same computed spot. Read the printed payload --
# it reports the before/after and the distance to Kaia so the result is checkable without
# hunting through the viewport.
#
# RUN (editor Python console):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/move_refactor_shrine.py"
# Then SAVE THE LEVEL. Nothing here saves for you.

import json
import unreal

SIDE_OFFSET = 260.0          # cm from Kaia, along her right vector
DANCER_HINT = "kaia"
TRIGGER_RADIUS = 110.0       # APowerGrant's sphere, for the sanity check below

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

notes = []


def floor_under(x, y, fallback):
    """Z of the floor beneath (x, y), or fallback when nothing is hit."""
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(x, y, fallback + 400.0),
        unreal.Vector(x, y, fallback - 600.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True, [],
        unreal.DrawDebugTrace.NONE,
        True, unreal.LinearColor.RED, unreal.LinearColor.GREEN, 1.0,
    )
    t = hit.to_tuple()
    if t and bool(t[0]):
        return t[4].z
    return None


shrine = None
kaia = None
all_grants = []

for a in eas.get_all_level_actors():
    try:
        cls = a.get_class().get_name()
        label = a.get_actor_label()
    except Exception:
        continue

    if cls == "PowerGrant":
        verb = None
        try:
            verb = a.get_editor_property("power")
        except Exception:
            pass
        all_grants.append({"label": label, "power": str(verb),
                           "loc": [round(v, 1) for v in a.get_actor_location().to_tuple()]})
        # Match on the verb where we can read it, else fall back to the label.
        if (verb is not None and "REFACTOR" in str(verb).upper()) or "refactor" in label.lower():
            shrine = a
    elif DANCER_HINT in label.lower():
        kaia = a

if shrine is None:
    payload = {"ok": False, "error": "no Refactor PowerGrant found", "grants": all_grants}
elif kaia is None:
    payload = {"ok": False,
               "error": "no actor whose label contains '%s' -- check her label in the "
                        "World Outliner and set DANCER_HINT" % DANCER_HINT,
               "grants": all_grants}
else:
    before = shrine.get_actor_location()

    # Height above the floor it currently stands on -- preserved, not the raw Z.
    old_floor = floor_under(before.x, before.y, before.z)
    height_above_floor = (before.z - old_floor) if old_floor is not None else 90.0
    if old_floor is None:
        notes.append("no floor found under the old spot; assuming 90cm above floor")

    k_loc = kaia.get_actor_location()
    k_rot = kaia.get_actor_rotation()
    right = k_rot.get_right_vector()

    x = k_loc.x + right.x * SIDE_OFFSET
    y = k_loc.y + right.y * SIDE_OFFSET

    new_floor = floor_under(x, y, k_loc.z)
    if new_floor is None:
        new_floor = k_loc.z
        notes.append("no floor found beside Kaia; using her own Z as the floor")

    target = unreal.Vector(x, y, new_floor + height_above_floor)
    shrine.set_actor_location(target, False, False)

    gap = unreal.Vector(target.x - k_loc.x, target.y - k_loc.y, 0.0).length()
    if gap < TRIGGER_RADIUS + 100.0:
        notes.append("WARNING: only %.0fcm from Kaia -- the %.0fcm trigger may fire while "
                     "the player stands to greet her. Raise SIDE_OFFSET." % (gap, TRIGGER_RADIUS))
    else:
        notes.append("%.0fcm from Kaia -- clear of the %.0fcm trigger plus standing room"
                     % (gap, TRIGGER_RADIUS))

    payload = {
        "ok": True,
        "shrine": shrine.get_actor_label(),
        "kaia": kaia.get_actor_label(),
        "before": [round(v, 1) for v in before.to_tuple()],
        "after": [round(v, 1) for v in target.to_tuple()],
        "height_above_floor": round(height_above_floor, 1),
        "notes": notes,
        "REMEMBER": "save the level -- this script does not",
        "all_power_grants": all_grants,
    }

print(json.dumps(payload, indent=2))
