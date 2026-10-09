# plan_kart_track.py — the Transhuman Grand Prix's circuit, from the measured streets.
#
# Plain Python (no Unreal): writes Saved/KartSurvey/track.json for make_kart_city.py.
#
# WHY THIS LOOP. survey_kart_streets.py and scan_kart_streets.py (2026-10-09) found the city
# is a ladder: two long east-west streets (the top one 14.5 m of asphalt, the bottom one
# 13.5 m) joined by four north-south ones. A loop through a ladder that never runs a street
# twice is a rectangle, and of the rectangles:
#   - the third street (x ~ 300) pinches to 5 m at its pedestrian crossing - single file;
#   - east of it the bottom street stops at the park, whose railings, planters, trees and
#     rock garden would all have to go;
# so the circuit is the block between the first street (x -23830) and the second (x -11630):
# east along the top street from the start line, down the second street (19 m wide - the
# place to overtake), west along the bottom street, and up the first, whose crossing at
# y ~ 0 narrows it to 9 m for 30 m: the bottleneck. About 490 m a lap, so four laps.
#
# Every number below is the middle of the asphalt the scans measured, and every half-width
# is inside it: the kerbs stand at the road's edge, not on the pavement.

import json
import os

PROJECT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(PROJECT, "Saved", "KartSurvey", "track.json")

STREET_Z = -16.0          # the road planes' height, everywhere on the loop
TOP_Y = 6800.0            # asphalt 6100..7500
BOTTOM_Y = -6150.0        # asphalt -6800..-5500
FIRST_X = -23830.0        # asphalt -24730..-22930, 9 m at the crossing (y -1500..1500)
SECOND_X = -11650.0       # asphalt -12600..-10700

# In driving order. [x, y, z, half-width]
CORNERS = [
    [FIRST_X,  TOP_Y,    STREET_Z, 700.0],   # top-left: out of the first street onto the top one
    [SECOND_X, TOP_Y,    STREET_Z, 700.0],   # top-right: down the second street
    [SECOND_X, 5000.0,   STREET_Z, 900.0],   # the second street opens to its full width...
    [SECOND_X, -4300.0,  STREET_Z, 900.0],   # ...and closes again before the bottom
    [SECOND_X, BOTTOM_Y, STREET_Z, 650.0],   # bottom-right: west along the bottom street
    [FIRST_X,  BOTTOM_Y, STREET_Z, 650.0],   # bottom-left: up the first street
    [FIRST_X,  -3000.0,  STREET_Z, 850.0],   # the first street, full width...
    [FIRST_X,  -1700.0,  STREET_Z, 430.0],   # ...squeezed for the crossing...
    [FIRST_X,  1700.0,   STREET_Z, 430.0],
    [FIRST_X,  3000.0,   STREET_Z, 800.0],   # ...and open again before the top
]

plan = {
    "corners": [c[:3] for c in CORNERS],
    "half_widths": [c[3] for c in CORNERS],
    "half_width": 700.0,
    "corner_radius": 900.0,
    "laps": 4,
    "note": "the block between the first and second streets, clockwise from the top street",
}
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as f:
    json.dump(plan, f, indent=1)
print("wrote", OUT)
