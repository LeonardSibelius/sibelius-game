# place_go_table.py — the Go table, on one of the Transhuman Chess Park's cafe tables, near
# the chess table (Walt, 2026-10-10: "yes, build Go on a 9x9 board").
#
# *** RUN FROM THE OPEN EDITOR with L_Chess loaded, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_go_table.py"
#
# DRY_RUN = True (the default) lists the free cafe tables, nearest the chess table first, and
# where the Go table would go. False places it and saves the level. Re-running with a Go
# table already there moves that one instead of adding a second.
#
# Never loads a level (the UnrealClaude plugin walks the level's actors after every script,
# and a level swapped out mid-script crashed the editor, 2026-09-28): it refuses unless the
# open map is already L_Chess.

import unreal

TAG = "###GOTABLE###"
DRY_RUN = True
PARK_MAP = "/Game/Maps/L_Chess"
TABLE_MESH = "SM_prop_dining_table_a"
SURFACE = "/Game/Go/M_GoSurface"
PICK = 0          # which free table, nearest first; change after a dry run if the first is wrong

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def flat_distance(a, b):
    d = a - b
    return (d.x * d.x + d.y * d.y) ** 0.5


def main():
    here = ues.get_editor_world().get_path_name()
    if PARK_MAP not in here:
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (here, PARK_MAP))
        return
    actors = eas.get_all_level_actors()
    chess = [a for a in actors if a.get_class().get_name() == "ChessTable"]
    go_class = unreal.load_class(None, "/Script/SibeliusGame.GoTable")
    existing = [a for a in actors if a.get_class().get_name() == "GoTable"]
    if not chess:
        say("no ChessTable in this level - nothing to stand beside.")
        return
    chess_at = chess[0].get_actor_location()

    tables = [a for a in actors if a.get_class().get_name() == "StaticMeshActor"
              and a.static_mesh_component.static_mesh
              and a.static_mesh_component.static_mesh.get_name() == TABLE_MESH]
    # Not the chess table's own, and not one so close that the two players would collide.
    free = [t for t in tables if flat_distance(t.get_actor_location(), chess_at) > 150.0]
    others = [a for a in actors if a.get_class().get_name() == "StaticMeshActor"]

    def crowding(table):
        at = table.get_actor_location()
        n = 0
        for o in others:
            if o != table:
                d = o.get_actor_location() - at
                n += 1 if abs(d.z) < 200 and (d.x * d.x + d.y * d.y) < 180.0 * 180.0 else 0
        return n

    free.sort(key=lambda t: flat_distance(t.get_actor_location(), chess_at))
    for i, t in enumerate(free[:8]):
        loc = t.get_actor_location()
        say("  [%d] %-28s %5.0f cm from chess, %d meshes within 1.8 m, at (%.0f, %.0f, %.0f)" % (
            i, t.get_actor_label(), flat_distance(loc, chess_at), crowding(t), loc.x, loc.y, loc.z))
    if not free:
        say("no free %s - the Go table needs placing by hand." % TABLE_MESH)
        return
    table = free[min(PICK, len(free) - 1)]
    origin, ext = table.get_actor_bounds(False)
    top = unreal.Vector(origin.x, origin.y, origin.z + ext.z)
    say("the Go table %s on [%d] %s, its surface at (%.1f, %.1f, %.1f); %s" % (
        "would go" if DRY_RUN else "goes", min(PICK, len(free) - 1), table.get_actor_label(), top.x, top.y, top.z,
        "a Go table is already here and would move" if existing else "none yet"))
    if DRY_RUN:
        return
    if not go_class:
        say("GoTable class not built yet - rebuild the editor first.")
        return

    go = existing[0] if existing else eas.spawn_actor_from_class(go_class, top, unreal.Rotator(0.0, 0.0, 0.0))
    # The board's block hangs below the origin, so the origin goes up by its thickness.
    thickness = go.get_editor_property("board_thickness")
    go.set_actor_location(unreal.Vector(top.x, top.y, top.z + thickness), False, False)
    go.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    go.set_actor_label("GoTable")
    surface = unreal.EditorAssetLibrary.load_asset(SURFACE)
    if surface:
        go.set_editor_property("surface_material", surface)
    else:
        say("no %s yet - run make_go_assets.py; the board wears the engine's basic material until then." % SURFACE)
    les.save_current_level()
    say("placed GoTable at %s and saved %s." % (go.get_actor_location(), PARK_MAP))


main()
