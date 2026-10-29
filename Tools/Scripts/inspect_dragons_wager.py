"""Read-only keep layout survey. Run inside the editor with ue_bridge exec-python."""
import unreal
import json
from pathlib import Path

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert "L_IslaKeep" in world.get_path_name(), "Open Isla's keep first"
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
def xyz(v): return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]
result = {"world": world.get_path_name(), "landmarks": [], "floor": []}
for a in actors:
    if any(s in a.get_actor_label().lower() for s in ("dragon_01", "dragon_2", "main_stairs", "magic_mirror", "isla")):
        origin, extent = a.get_actor_bounds(False)
        result["landmarks"].append({"label": a.get_actor_label(), "position": xyz(a.get_actor_location()),
                                    "origin": xyz(origin), "extent": xyz(extent)})
for x,y,z in [(220,-570,200), (0,1150,800), (0,1450,750), (-180,350,450),
              (180,550,600), (-150,750,700), (150,950,800), (-600,-50,200), (600,-50,200)]:
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x,y,z), unreal.Vector(x,y,-100),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    result["floor"].append({"from":[x,y,z], "hit": xyz(hit.to_tuple()[4]) if hit else None})
result["sight"] = []
for eye in [(-310,-205,460),(315,-215,460)]:
    for target in [(0,-500,150),(0,500,370),(0,1150,590)]:
        hit = unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*eye),unreal.Vector(*target),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True)
        result["sight"].append({"eye":eye,"target":target,"blocked_by":hit.to_tuple()[9].get_actor_label() if hit else None})
path = Path(unreal.Paths.project_saved_dir()) / "DragonsWager-layout.json"
path.write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result))
