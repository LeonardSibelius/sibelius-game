# scan_kart_streets.py — measure the edges of Transhuman City's streets for the Grand Prix.
#
# survey_kart_streets.py found the street grid at 3 m resolution: two long east-west
# streets and four north-south ones between them. The track needs better than that - its
# kerbs stand at the road's edge - so this cuts across each street every 10 m and samples
# every 50 cm, recording where the asphalt (the city's road planes, mesh "Plane", at street
# level) starts and stops. Saved as Saved/KartSurvey/streets.json.
#
# RUN with L_City (or L_Kart) open, PIE stopped. Reads only.

import json
import os
import unreal

TAG = "###KARTSCAN###"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "KartSurvey", "streets.json")

# Centre lines from the 3 m survey; each scanned across +-HALF_SPAN, along its length.
STREETS = {
    "top":    {"axis": "x", "at": 6900.0,   "from": -26000.0, "to": 14000.0},
    "bottom": {"axis": "x", "at": -5400.0,  "from": -26000.0, "to": 14000.0},
    "v1":     {"axis": "y", "at": -23830.0, "from": -8000.0,  "to": 9000.0},
    "v2":     {"axis": "y", "at": -11980.0, "from": -8000.0,  "to": 9000.0},
    "v3":     {"axis": "y", "at": 170.0,    "from": -8000.0,  "to": 9000.0},
    "v4":     {"axis": "y", "at": 11870.0,  "from": -8000.0,  "to": 9000.0},
}
HALF_SPAN = 1500.0
ALONG_STEP = 1000.0
ACROSS_STEP = 50.0

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def ground(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 3000.0), unreal.Vector(x, y, -1000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None, None
    t = hit.to_tuple()
    loc = next(v for v in t[4:] if isinstance(v, unreal.Vector))
    comp = next((v for v in t if isinstance(v, unreal.PrimitiveComponent)), None)
    name = "?"
    if comp is not None:
        try:
            sm = comp.get_editor_property("static_mesh")
            name = sm.get_name() if sm else comp.get_class().get_name()
        except Exception:
            name = comp.get_class().get_name()
    return round(loc.z, 1), name


def main():
    world = ues.get_editor_world()
    out = {}
    for key, s in STREETS.items():
        rows = []
        a = s["from"]
        while a <= s["to"]:
            samples = []
            c = s["at"] - HALF_SPAN
            while c <= s["at"] + HALF_SPAN:
                x, y = (a, c) if s["axis"] == "x" else (c, a)
                z, name = ground(world, x, y)
                samples.append([round(c, 1), z, name])
                c += ACROSS_STEP
            rows.append({"along": a, "samples": samples})
            a += ALONG_STEP
        out[key] = {"axis": s["axis"], "at": s["at"], "rows": rows}
        say("%s: %d cross-sections" % (key, len(rows)))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(out, f)
    say("saved %s" % OUT)


main()
