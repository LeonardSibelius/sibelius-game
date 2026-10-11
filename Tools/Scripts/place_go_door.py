# place_go_door.py — the GO door in the living room, the next door along from BOWLING, in front
# of a window, to the Go table in the Transhuman Chess Park.
#
# Walt, 2026-10-10: "I like the park. please make a door to the park, like the chess door",
# placed "in front of SM_Window_Floor1_A3_402".
#
# The Go door is the CHESS door copied - same frame, same always-visible door - with the GO
# sign (make_go_sign.py). The living room's doors stand in a row along one wall, CHESS then
# BOWLING, a door's width plus 40 cm apart; the next place in that row, past BOWLING, is in
# front of that window. The window's curtains hang there, so the door stands just far enough
# out from the wall to clear them (they hang behind it rather than through it). It travels to
# L_Chess with the arrival tag GoPark: a PlayerStart beside the Go table (place_go_arrival.py,
# run with L_Chess open); until that start exists, the park's default start is used.
#
# RUN with L_Office_v02 open, PIE stopped, after make_go_assets.py (for T_Sign_Go):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_go_door.py"
# DRY_RUN = True (the default) only measures and says where the door would go. Safe to re-run:
# it replaces the GoDoor it made.

import unreal

TAG = "###GODOOR###"
DRY_RUN = True
WINDOW = "SM_Window_Floor1_A3_402"
SIGN = "/Game/Go/T_Sign_Go"
GAP = 40.0          # cm between door frames, as between CHESS and BOWLING
CLEAR = 4.0         # cm between the curtains and the back of the door
BIG = 300.0         # actors wider than this are walls and floors, not things in the way

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def unit(v):
    n = (v.x * v.x + v.y * v.y + v.z * v.z) ** 0.5
    return unreal.Vector(v.x / n, v.y / n, v.z / n)


def main():
    actors = eas.get_all_level_actors()
    doors = [a for a in actors if a.get_class().get_name() == "HiddenDoor"]
    chess = next((a for a in doors if a.get_actor_label() == "ChessDoor"), None)
    bowling = next((a for a in doors if a.get_actor_label() == "BowlingDoor"), None)
    window = next((a for a in actors if a.get_actor_label() == WINDOW), None)
    if not (chess and bowling and window):
        say("need ChessDoor, BowlingDoor and %s in the open level (open L_Office_v02)." % WINDOW)
        return
    sign = unreal.load_asset(SIGN)
    if not sign:
        say("%s not found - run make_go_assets.py first." % SIGN)
        return
    old = [a for a in doors if a.get_actor_label() == "GoDoor"]

    # The row: from CHESS to BOWLING, and one more step the same way.
    b_origin, b_ext = bowling.get_actor_bounds(False)
    c_origin, _ = chess.get_actor_bounds(False)
    along = c_origin - b_origin
    along = unit(unreal.Vector(-along.x, -along.y, 0.0))      # away from CHESS
    width = 2.0 * (abs(along.x) * b_ext.x + abs(along.y) * b_ext.y)
    step = width + GAP
    # Into the room: from the window's plane toward the doors, which stand on the room side.
    w_origin, w_ext = window.get_actor_bounds(False)
    normal = unreal.Vector(1.0, 0.0, 0.0) if w_ext.x < w_ext.y else unreal.Vector(0.0, 1.0, 0.0)
    if (b_origin.x - w_origin.x) * normal.x + (b_origin.y - w_origin.y) * normal.y < 0.0:
        normal = normal * -1.0
    depth = 2.0 * (abs(normal.x) * b_ext.x + abs(normal.y) * b_ext.y)

    centre = b_origin + along * step
    lo_along = (centre.x * along.x + centre.y * along.y) - 0.5 * width
    hi_along = lo_along + width
    back = (centre.x * normal.x + centre.y * normal.y) - 0.5 * depth
    w_lo = (w_origin.x * along.x + w_origin.y * along.y) - (abs(along.x) * w_ext.x + abs(along.y) * w_ext.y)
    w_hi = w_lo + 2.0 * (abs(along.x) * w_ext.x + abs(along.y) * w_ext.y)
    say("bowling door %.0f cm wide; the next place in the row spans %.0f..%.0f along the wall; the window spans %.0f..%.0f" % (
        width, min(lo_along, hi_along), max(lo_along, hi_along), min(w_lo, w_hi), max(w_lo, w_hi)))

    # What stands there, and how far out the door must stand to clear it.
    push = 0.0
    for a in actors:
        if a in doors or a == window:
            continue
        o, e = a.get_actor_bounds(False)
        if max(e.x, e.y, e.z) * 2.0 > BIG or (e.x + e.y + e.z) < 1.0:
            continue
        a_lo = (o.x * along.x + o.y * along.y) - (abs(along.x) * e.x + abs(along.y) * e.y)
        a_hi = a_lo + 2.0 * (abs(along.x) * e.x + abs(along.y) * e.y)
        front = (o.x * normal.x + o.y * normal.y) + (abs(normal.x) * e.x + abs(normal.y) * e.y)
        rear = front - 2.0 * (abs(normal.x) * e.x + abs(normal.y) * e.y)
        overlaps_along = min(a_lo, a_hi) < max(lo_along, hi_along) and max(a_lo, a_hi) > min(lo_along, hi_along)
        overlaps_up = abs(o.z - b_origin.z) < e.z + b_ext.z
        if overlaps_along and overlaps_up and rear < back + depth + 1.0 and front > back - 30.0:
            need = front + CLEAR - back
            say("  in the way: %-24s its front %.0f cm out from the door's back - %s" % (
                a.get_actor_label(), front - back, "clear it by %.0f cm" % need if need > 0 else "no matter"))
            push = max(push, need)
    if push > 60.0:
        say("something big stands there (%.0f cm deep) - nothing placed; place the door by hand." % push)
        return

    offset = along * step + normal * push
    say("the Go door %s %.0f cm along from BOWLING and %.0f cm out from the wall, at %s" % (
        "would stand" if DRY_RUN else "stands", step, push, bowling.get_actor_location() + offset))
    if DRY_RUN:
        return

    for a in old:
        eas.destroy_actor(a)
    door = eas.duplicate_actor(chess, None, bowling.get_actor_location() + offset - chess.get_actor_location())
    door.set_actor_label("GoDoor")
    door.set_editor_property("travel_target_level", "L_Chess")
    door.set_editor_property("arrival_tag", "GoPark")
    door.set_editor_property("travel_prompt_text", "Play Go  [E]")
    door.set_editor_property("sign_texture", sign)
    door.set_editor_property("always_visible", True)
    les.save_current_level()
    say("GoDoor placed at %s -> L_Chess (arrival tag GoPark). Saved." % door.get_actor_location())


main()
