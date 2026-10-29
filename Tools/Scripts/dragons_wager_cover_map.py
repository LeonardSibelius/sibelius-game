"""Read-only cover map of Isla's keep hall for The Dragon's Wager. Run in PIE with ue_bridge exec-python.

For each 50 cm floor cell, can each dragon eye see the pawn's centre or eyes? Same rule as
ADragonsWager::UpdateWatch: Visibility traces that ignore only the pawn, the wager actor and
that dragon itself. Writes Saved/DragonsWager-cover-map.txt:
  '.' seen by both   'L' only the -X dragon   'R' only the +X dragon   ' ' hidden   '#' no floor
"""
import unreal
from pathlib import Path

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert w and "L_IslaKeep" in w.get_name(), "Start PIE in L_IslaKeep first"
pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
game = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DragonsWager)[0]
vis = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1

def trace(a, b, ignore):
    return unreal.SystemLibrary.line_trace_single(w, a, b, vis, True, ignore, unreal.DrawDebugTrace.NONE, True)

dragons = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
           if any(c.static_mesh and c.static_mesh.get_name() == "SM_Dragon_a_01"
                  for c in a.get_components_by_class(unreal.StaticMeshComponent))]
eyes = [unreal.Vector(-310, -205, 460), unreal.Vector(315, -215, 460)]
owners = [min(dragons, key=lambda a: (a.get_actor_location() - e).length()) for e in eyes]
rows = []
for y in range(1300, -851, -50):
    row = ""
    for x in range(-750, 751, 50):
        h = trace(unreal.Vector(x, y, 900), unreal.Vector(x, y, -50), [pawn, game])
        if not h or h.to_tuple()[7].z < 0.7:
            row += "#"; continue
        floor = h.to_tuple()[4].z
        # the capsule must fit above the floor
        if trace(unreal.Vector(x, y, floor + 20), unreal.Vector(x, y, floor + 180), [pawn, game]):
            row += "#"; continue
        seen = [any(not trace(e, unreal.Vector(x, y, floor + dz), [pawn, game, owners[i]]) for dz in (98, 158))
                for i, e in enumerate(eyes)]
        row += "." if all(seen) else ("L" if seen[0] else ("R" if seen[1] else " "))
    rows.append("%5d %s" % (y, row))
path = Path(unreal.Paths.project_saved_dir()) / "DragonsWager-cover-map.txt"
path.write_text("x: -750 .. 750 step 50 (left column = -X)\n" + "\n".join(rows), encoding="utf-8")
print(path)
