# dump_live_spheres.py — what is standing next to the two shrines that can still fire.
#
# READ-ONLY. Changes nothing, saves nothing.
#
# Three shrines are now bound to dancers and inert. Only PowerGrant_Compile and
# PowerGrant_Generate still have live overlap triggers, so the machine Walt keeps meeting
# on a staircase is one of those two -- but their positions alone do not say WHICH
# staircase, and I have already guessed wrong twice (Refactor, then Deploy).
#
# So stop guessing: print the twelve nearest actors to each live shrine. Stairs, banisters
# and landings are placed geometry with names, and whichever shrine has a staircase in its
# neighbour list is the one.
#
# Also prints each shrine's distance to the PlayerStart and to every dancer, and its
# height above the floor directly beneath it -- a shrine on a landing and a shrine at the
# bottom of a flight look identical in raw Z.
#
# RUN (editor Python console):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/dump_live_spheres.py"

import json
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

STAIR_WORDS = ("stair", "step", "banister", "railing", "rail", "landing", "attic",
               "floor", "hall", "library", "shelf", "book")

all_actors = []
grants = []
start = None
dancers = []

for a in eas.get_all_level_actors():
    try:
        label = a.get_actor_label()
        cls = a.get_class().get_name()
        loc = a.get_actor_location()
    except Exception:
        continue

    rec = {"label": label, "cls": cls, "loc": loc}
    all_actors.append(rec)

    if cls == "PowerGrant":
        bound = None
        try:
            g = a.get_editor_property("granted_by_agent")
            bound = g.get_actor_label() if g else None
        except Exception:
            bound = "?"
        if bound in (None, "None"):
            grants.append((a, label, loc))
    elif cls == "PlayerStart":
        start = rec
    elif any(h in label.lower() for h in ("kaia", "nyra", "isla", "aisling", "elise")):
        dancers.append(rec)


def d(a, b):
    return round(unreal.Vector(a.x - b.x, a.y - b.y, a.z - b.z).length(), 1)


def floor_under(loc):
    """Height above the floor beneath loc, and what that floor is.

    Uses break_hit_result rather than indexing HitResult.to_tuple() by position: the
    first version read t[4] as the impact point (right) and t[5] as the actor (wrong --
    it is the impact NORMAL), and died with 'Vector object has no attribute
    get_actor_label'. Named fields do not drift when a struct changes.
    """
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(loc.x, loc.y, loc.z + 50.0),
        unreal.Vector(loc.x, loc.y, loc.z - 800.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True, [], unreal.DrawDebugTrace.NONE,
        True, unreal.LinearColor.RED, unreal.LinearColor.GREEN, 1.0,
    )
    try:
        broken = unreal.GameplayStatics.break_hit_result(hit)
        blocking, _initial, _time, _dist, _location, impact, _n, _in, _phys, actor = broken[:10]
        if not blocking:
            return None, None
        name = None
        try:
            name = actor.get_actor_label() if actor else None
        except Exception:
            name = str(actor) if actor else None
        return round(loc.z - impact.z, 1), name
    except Exception as e:
        unreal.log_warning("[dump_live_spheres] floor trace unreadable: %s" % e)
        return None, None


out = []
for a, label, loc in grants:
    neighbours = sorted(
        ({"label": r["label"], "cls": r["cls"], "dist": d(loc, r["loc"])}
         for r in all_actors if r["label"] != label),
        key=lambda r: r["dist"],
    )[:12]

    # Never let a diagnostic die on its own nice-to-have: the neighbour list is the
    # answer, the floor trace is garnish. The first run threw here and produced NO FILE
    # AT ALL, which cost a round trip for a field nobody needed.
    try:
        height, floor_actor = floor_under(loc)
    except Exception as e:
        height, floor_actor = None, "trace failed: %s" % e

    out.append({
        "grant": label,
        "loc": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
        "height_above_floor": height,
        "standing_over": floor_actor,
        "dist_to_playerstart": d(loc, start["loc"]) if start else None,
        "dist_to_dancers": sorted(
            ({"dancer": dr["label"], "dist": d(loc, dr["loc"])} for dr in dancers),
            key=lambda r: r["dist"]),
        "twelve_nearest_actors": neighbours,
        "stairish_neighbours": [n for n in neighbours
                                if any(w in n["label"].lower() for w in STAIR_WORDS)],
    })

payload = {
    "live_spheres": out,
    "note": "These are the ONLY shrines that can still open a trial on overlap. "
            "Whichever has stairs in its neighbour list is the one on the staircase.",
}

text = json.dumps(payload, indent=2, default=str)
path = unreal.Paths.project_saved_dir() + "live_spheres.json"
try:
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[dump_live_spheres] wrote " + path)
except Exception as e:
    unreal.log_error("[dump_live_spheres] could not write %s: %s" % (path, e))

print(text)
