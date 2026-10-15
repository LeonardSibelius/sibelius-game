# place_basketball_door.py - the BASKETBALL door in the living room, the next door along from
# GO, to the three-point contest's court in the Transhuman Chess Park.
#
# Walt, 2026-10-15: "let's do the basketball shooting contest - can the door go in front of
# SM_Curtains_A7?" The doors stand in a row along one wall, CHESS, BOWLING, GO, a door's width
# plus 40 cm apart (place_go_door.py); the next place in that row, past GO, is in front of the
# curtains of that window. The GO door already stands out from the wall far enough to clear
# those curtains, so this one stands in line with it, further out only if something there
# sticks out further. It travels to L_Chess with the arrival tag BasketballCourt: a PlayerStart
# at the court, placed with the court; until that start exists, the park's default start is used.
#
# RUN with L_Office_v02 open, PIE stopped, after make_basketball_assets.py (T_Sign_Basketball):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_basketball_door.py"
# DRY_RUN = True (the default) only measures and says where the door would go. Safe to re-run:
# it replaces the BasketballDoor it made.

import unreal

TAG = "###HOOPDOOR###"
DRY_RUN = True
CURTAINS = "SM_Curtains_A7"
SIGN = "/Game/Basketball/T_Sign_Basketball"
# cm between the GO door's frame and this one's. Not the row's usual 40: measured 2026-10-15,
# the wall has only 145 cm between the GO door and SM_Corner_1Floor_Inside_A2 (an inside corner
# standing 50 cm out), and a door is 141 wide - so the BASKETBALL door stands snug beside GO, in
# front of the curtains, rather than 54 cm out into the room in front of the corner.
GAP = 2.0
CLEAR = 4.0         # cm between whatever is behind and the back of the door
BIG = 300.0         # actors wider than this are walls and floors, not things in the way

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def unit(v):
    n = (v.x * v.x + v.y * v.y + v.z * v.z) ** 0.5
    return unreal.Vector(v.x / n, v.y / n, v.z / n)


def dot2(a, b):
    return a.x * b.x + a.y * b.y


def span(o, e, axis):
    """An actor's extent along a horizontal unit axis: (low, high)."""
    c = dot2(o, axis)
    r = abs(axis.x) * e.x + abs(axis.y) * e.y
    return c - r, c + r


def main():
    actors = eas.get_all_level_actors()
    doors = [a for a in actors if a.get_class().get_name() == "HiddenDoor"]
    bowling = next((a for a in doors if a.get_actor_label() == "BowlingDoor"), None)
    go = next((a for a in doors if a.get_actor_label() == "GoDoor"), None)
    curtains = next((a for a in actors if a.get_actor_label() == CURTAINS), None)
    if not (bowling and go and curtains):
        say("need BowlingDoor, GoDoor and %s in the open level (open L_Office_v02)." % CURTAINS)
        return
    sign = unreal.load_asset(SIGN)
    if not sign:
        say("%s not found - run make_basketball_assets.py first." % SIGN)
        return
    old = [a for a in doors if a.get_actor_label() == "BasketballDoor"]

    # The row: from BOWLING to GO, and one more step the same way.
    g_origin, g_ext = go.get_actor_bounds(False)
    b_origin, _ = bowling.get_actor_bounds(False)
    along = unit(unreal.Vector(g_origin.x - b_origin.x, g_origin.y - b_origin.y, 0.0))
    normal = unreal.Vector(-along.y, along.x, 0.0)           # across the row...
    c_origin, c_ext = curtains.get_actor_bounds(False)
    if dot2(g_origin - c_origin, normal) < 0.0:               # ...pointing into the room
        normal = normal * -1.0
    width = 2.0 * (abs(along.x) * g_ext.x + abs(along.y) * g_ext.y)
    depth = 2.0 * (abs(normal.x) * g_ext.x + abs(normal.y) * g_ext.y)
    step = width + GAP
    centre = g_origin + along * step
    lo, hi = dot2(centre, along) - 0.5 * width, dot2(centre, along) + 0.5 * width
    back = dot2(g_origin, normal) - 0.5 * depth
    c_lo, c_hi = span(c_origin, c_ext, along)
    say("GO door %.0f cm wide, %.0f deep; the next place spans %.0f..%.0f along the wall; %s spans %.0f..%.0f" % (
        width, depth, lo, hi, CURTAINS, c_lo, c_hi))

    # The room's end: a wall across the row must leave the whole door's width free.
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    start = unreal.Vector(g_origin.x, g_origin.y, g_origin.z) + normal * (0.5 * depth)
    end = start + along * (step + 0.5 * width + 60.0)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                 False, [go, bowling], unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None      # None when the trace meets nothing at all
    if t and t[0]:
        free = dot2(t[5] - start, along)
        actor = t[9].get_actor_label() if t[9] else "?"
        say("  along the row, the first thing in the way is %s, %.0f cm on from the GO door's middle (need %.0f)" % (
            actor, free, step + 0.5 * width))
        if free < step + 0.5 * width:
            say("no room for a door's width there - nothing placed.")
            return
    else:
        say("  along the row: clear for %.0f cm" % (step + 0.5 * width + 60.0))

    # What stands where the door would, and how far out it must stand to clear it.
    push = 0.0
    for a in actors:
        if a in doors:
            continue
        o, e = a.get_actor_bounds(False)
        if max(e.x, e.y, e.z) * 2.0 > BIG or (e.x + e.y + e.z) < 1.0:
            continue
        a_lo, a_hi = span(o, e, along)
        n_lo, n_hi = span(o, e, normal)
        overlaps_along = a_lo < hi and a_hi > lo
        overlaps_up = abs(o.z - g_origin.z) < e.z + g_ext.z
        if overlaps_along and overlaps_up and n_lo < back + depth + 1.0 and n_hi > back - 30.0:
            need = n_hi + CLEAR - back
            say("  in the way: %-26s its front %.0f cm out from the door's back - %s" % (
                a.get_actor_label(), n_hi - back, "clear it by %.0f cm" % need if need > 0 else "no matter"))
            push = max(push, need)
    if push > 60.0:
        say("something big stands there (%.0f cm deep) - nothing placed; place the door by hand." % push)
        return

    offset = along * step + normal * push
    say("the BASKETBALL door %s %.0f cm along from GO and %.0f cm further out, at %s" % (
        "would stand" if DRY_RUN else "stands", step, push, go.get_actor_location() + offset))
    if DRY_RUN:
        return

    for a in old:
        eas.destroy_actor(a)
    door = eas.duplicate_actor(go, None, offset)
    door.set_actor_label("BasketballDoor")
    door.set_editor_property("travel_target_level", "L_Chess")
    door.set_editor_property("arrival_tag", "BasketballCourt")
    door.set_editor_property("travel_prompt_text", "Three-point contest  [E]")
    door.set_editor_property("sign_texture", sign)
    door.set_editor_property("always_visible", True)
    les.save_current_level()
    say("BasketballDoor placed at %s -> L_Chess (arrival tag BasketballCourt). Saved." % door.get_actor_location())


main()
