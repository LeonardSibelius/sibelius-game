# survey_kart_streets.py — map Transhuman City's streets for the Grand Prix track.
#
# Walt, 2026-10-09: "Transhuman City streets, go ahead and build it." The race's track is a
# loop of street corners (AKartRace.TrackSpline), and the corners have to be where the city
# really has streets. Nobody wrote the city's street plan down - it is the Downtown West
# pack's demo map - so this measures it: a grid of traces straight down over the whole
# city, each recording what it landed on (which mesh, how high), saved as JSON for
# Tools/Scripts/plan_kart_track.py to read the streets out of.
#
# RUN with L_City (or L_Kart) open, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/survey_kart_streets.py"
# Reads only. Writes Saved/KartSurvey/survey.json.

import json
import os
import unreal

TAG = "###KARTSURVEY###"
STEP = 300.0            # cm between traces
OUT = os.path.join(unreal.Paths.project_saved_dir(), "KartSurvey", "survey.json")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def city_bounds():
    """The middle of the city: the 2nd to 98th percentile of the meshes' centres, so a
    far-off sky dome or backdrop does not stretch the grid over empty miles."""
    xs, ys, zs = [], [], []
    for a in eas.get_all_level_actors():
        if a.get_class().get_name() != "StaticMeshActor":
            continue
        o, e = a.get_actor_bounds(False)
        if max(e.x, e.y) > 20000.0:
            continue
        xs.append(o.x)
        ys.append(o.y)
        zs.append(o.z - e.z)
    xs.sort()
    ys.sort()
    zs.sort()

    def pct(v, p):
        return v[min(len(v) - 1, max(0, int(len(v) * p)))]

    return pct(xs, 0.02), pct(xs, 0.98), pct(ys, 0.02), pct(ys, 0.98), pct(zs, 0.05), len(xs)


def main():
    world = ues.get_editor_world()
    x0, x1, y0, y1, ground, n = city_bounds()
    say("map %s: %d meshes; grid x %.0f..%.0f, y %.0f..%.0f, ground ~%.0f, step %.0f" % (
        world.get_path_name(), n, x0, x1, y0, y1, ground, STEP))
    top = ground + 6000.0
    bottom = ground - 3000.0
    cells = []
    meshes = {}
    nx = int((x1 - x0) / STEP) + 1
    ny = int((y1 - y0) / STEP) + 1
    for j in range(ny):
        y = y0 + j * STEP
        for i in range(nx):
            x = x0 + i * STEP
            hit = unreal.SystemLibrary.line_trace_single(
                world, unreal.Vector(x, y, top), unreal.Vector(x, y, bottom),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
            if not hit:
                cells.append([i, j, None, None])
                continue
            # The tuple's order has moved between engine versions: take the first vector after
            # the two flags and two numbers (the location), and the first component.
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
            meshes[name] = meshes.get(name, 0) + 1
            cells.append([i, j, round(loc.z, 1), name])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump({"x0": x0, "y0": y0, "step": STEP, "nx": nx, "ny": ny, "ground": ground, "cells": cells}, f)
    top_names = sorted(meshes.items(), key=lambda kv: -kv[1])[:40]
    say("%d x %d = %d traces, saved %s" % (nx, ny, nx * ny, OUT))
    for name, count in top_names:
        say("  %6d  %s" % (count, name))


main()
