# forest_verify_flat.py — read-only: line-trace down at several points; report the terrain Z at
# each + the spread. Flat == all Z within a few cm. (No heightmap writes.)
import unreal, json
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
pts = [(0, 0), (20000, 20000), (-20000, -20000), (20000, -20000), (-20000, 20000)]
zs = []
for (x, y) in pts:
    try:
        res = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, 8000.0), unreal.Vector(x, y, -8000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
        hit = res[1] if isinstance(res, (tuple, list)) else res
        zs.append(round(hit.impact_point.z, 1))   # -8000 if the trace missed
    except Exception as e:
        zs.append("ERR:%r" % e)
valid = [z for z in zs if isinstance(z, (int, float))]
spread = (max(valid) - min(valid)) if len(valid) > 1 else None
out = {"z_at_points": zs, "spread_uu": spread, "spread_cm": spread}
open(r"C:/Users/wpark/projects/sibelius-game/forest-flat-check.json", "w").write(json.dumps(out, indent=1))
unreal.log("###FLATCHK### zs=%s spread=%s uu" % (zs, spread))
