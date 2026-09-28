# place_chess_door.py — the CHESS door in the living room, beside the BOWLING door.
#
# Walt, 2026-09-28: "let's make a chess portal with a chess knight image on it and put that
# portal next to the one for bowling in the living room. Five games in the living room
# without having to do the ai agent routine." Poker, pinball, pool, bowling, chess.
#
# The chess door is the bowling door copied - same frame, same always-visible door, same
# sign placement - moved one door's width plus a gap along the wall, to whichever side has
# nothing standing in the way, and pointed at L_Chess with the knight sign.
#
# RUN with L_Office_v02 open, PIE stopped, after import_chess_set.py (for T_Sign_Chess):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_chess_door.py"
# Safe to re-run: it replaces the ChessDoor it made. SIDE forces a side (+1 / -1).

import unreal

TAG = "###CHESSDOOR###"
SIGN = "/Game/ChessSet/Textures/T_Sign_Chess"
GAP = 40.0          # cm between the two door frames
SIDE = 0            # 0 = pick the clear side
BIG = 300.0         # actors wider than this are walls and floors, not things in the way

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def overlaps(centre, extent, skip):
    """Things (not walls) whose bounds meet a box at centre +- extent."""
    hits = []
    for actor in eas.get_all_level_actors():
        if actor in skip:
            continue
        origin, ext = actor.get_actor_bounds(False)
        if max(ext.x, ext.y, ext.z) * 2.0 > BIG or (ext.x + ext.y + ext.z) < 1.0:
            continue
        if (abs(origin.x - centre.x) < ext.x + extent.x and abs(origin.y - centre.y) < ext.y + extent.y
                and abs(origin.z - centre.z) < ext.z + extent.z):
            hits.append(actor.get_actor_label())
    return hits


def main():
    doors = [a for a in eas.get_all_level_actors() if a.get_class().get_name() == "HiddenDoor"]
    bowling = next((a for a in doors if a.get_actor_label() == "BowlingDoor"), None)
    if not bowling:
        say("no BowlingDoor here - open L_Office_v02.")
        return
    for old in [a for a in doors if a.get_actor_label() == "ChessDoor"]:
        eas.destroy_actor(old)

    sign = unreal.load_asset(SIGN)
    if not sign:
        say("%s not found - run import_chess_set.py first." % SIGN)
        return

    # The door's width runs along the wall: its longer horizontal extent.
    origin, ext = bowling.get_actor_bounds(False)
    along = unreal.Vector(1.0, 0.0, 0.0) if ext.x >= ext.y else unreal.Vector(0.0, 1.0, 0.0)
    width = 2.0 * max(ext.x, ext.y)
    step = width + GAP
    say("bowling door at %s, bounds %.0f x %.0f x %.0f, width along %s = %.0f cm" % (
        bowling.get_actor_location(), 2 * ext.x, 2 * ext.y, 2 * ext.z, "X" if along.x else "Y", width))

    choices = []
    for sign_dir in ((SIDE,) if SIDE else (1, -1)):
        centre = origin + along * (step * sign_dir)
        hits = overlaps(centre, ext, [bowling])
        say("side %+d: centre %s, in the way: %s" % (sign_dir, centre, hits or "nothing"))
        choices.append((len(hits), sign_dir))
    choices.sort()
    side = choices[0][1]

    door = eas.duplicate_actor(bowling, None, along * (step * side))
    door.set_actor_label("ChessDoor")
    door.set_editor_property("travel_target_level", "L_Chess")
    door.set_editor_property("arrival_tag", "ChessPark")
    door.set_editor_property("travel_prompt_text", "Play chess  [E]")
    door.set_editor_property("sign_texture", sign)
    door.set_editor_property("always_visible", True)
    les.save_current_level()
    say("ChessDoor placed on side %+d at %s -> L_Chess (arrival tag ChessPark). Saved." % (side, door.get_actor_location()))
    say("bowling door travels to %s, tag %s" % (bowling.get_editor_property("travel_target_level"),
                                                bowling.get_editor_property("arrival_tag")))


main()
