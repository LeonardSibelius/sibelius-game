# place_go_arrival.py — where the living room's GO door sets Leonard down in the Transhuman Chess
# Park: standing on Black's side of the Go table, a step back, facing the board.
#
# Walt, 2026-10-10: "please make a door to the park, like the chess door". The GO door
# (place_go_door.py) travels to L_Chess with arrival tag GoPark; this is that PlayerStart,
# made the way make_chess_park.py made the chess table's ChessPark start.
#
# RUN with L_Chess open, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_go_arrival.py"
# Safe to re-run: it replaces the start it made. Refuses unless the open map is L_Chess.

import unreal

TAG = "###GOARRIVAL###"
PARK_MAP = "/Game/Maps/L_Chess"
ARRIVAL_TAG = "GoPark"
BACK = 160.0        # cm behind the board's centre, on Black's side (-X), as the chess start

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def main():
    here = ues.get_editor_world().get_path_name()
    if PARK_MAP not in here:
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (here, PARK_MAP))
        return
    actors = eas.get_all_level_actors()
    go = next((a for a in actors if a.get_class().get_name() == "GoTable"), None)
    if not go:
        say("no GoTable here - run place_go_table.py first.")
        return
    for old in [a for a in actors if a.get_actor_label() == "PlayerStart_GoPark"]:
        eas.destroy_actor(old)
    board = go.get_actor_location()
    table_top = board.z - go.get_editor_property("board_thickness")
    yaw = go.get_actor_rotation().yaw
    back = unreal.MathLibrary.get_forward_vector(unreal.Rotator(0.0, 0.0, yaw)) * -BACK
    start = unreal.Vector(board.x + back.x, board.y + back.y, table_top + 20.0)
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, start, unreal.Rotator(0.0, 0.0, yaw))
    ps.set_editor_property("player_start_tag", ARRIVAL_TAG)
    ps.set_actor_label("PlayerStart_GoPark")
    les.save_current_level()
    say("PlayerStart_GoPark (tag %s) at %s facing yaw %.0f, the Go board %.0f cm ahead. Saved." % (
        ARRIVAL_TAG, start, yaw, BACK))


main()
