# place_temple_door.py - the Archaic Language Temple's door, in the poker corner, seen only with V.
#
# Walt, 2026-10-19: "'V' doesn't show anything in the living room, so I suppose I will add a
# hidden door next to poker made visible with 'V' and call it the Archaic Language Temple."
#
# WHERE, measured 2026-10-19 (numbers only - the living room is a bought pack's). The poker
# cabinet is a flat card at the south glass, and neither side of it has wall to spare: to the west
# hang the curtains over the garden's glass door (BP_Door_Outside_B); to the east, a 50 cm strip of
# wall and then the kitchen. Round the corner is the wallpapered box-out the Grand Prix kart backs
# onto, beside the side-game doors' row (CHESS, BOWLING, GO, BASKETBALL). Walt moved the kart 60 cm
# out from it and cleared what stood against it: "can we put the door on the wall behind the kart?"
# So the temple door is the BASKETBALL door copied (its glass, its sign hung straight), centred
# behind the kart on that wall - and NOT always visible. The third secret door, after the office's
# and the attic's (Walt, 2026-09-27, had said two were enough; this one is his own call). Hold V and
# it shimmers in with its sign (make_archaic_temple_sign.py, imported by setup_archaic_temple.py);
# E travels to L_Archaic_Temple, arriving at the PlayerStart tagged ArchaicTemple.
#
# MEASURED BY TRACES, NOT ACTOR BOUNDS: the first try read the cabinet's bounds as 256 cm a side -
# the editor's hidden billboard icon - and would have put the door outside the house.
#
# DRY_RUN = True (the default) only measures and says where the door would go. The real run does
# NOT save: Walt holds V, looks, and saves (Ctrl+S). Safe to re-run: it replaces the door it made.
#
# *** RUN WITH L_Office_v02 OPEN, PIE stopped, after setup_archaic_temple.py (for the sign) ***

import unreal

TAG = "###TEMPLEDOOR###"
DRY_RUN = True
MAP = "/Game/L_Office_v02"
SIGN = "/Game/Signs/T_Sign_ArchaicTemple"
LABEL = "ArchaicTempleDoor"
CLEAR = 4.0      # cm between the wall's face and the back of the door's glass

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(msg):
    print("%s %s" % (TAG, msg))


def trace(world, a, b, ignore):
    hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None or not hit.to_tuple()[0]:
        return None, None
    t = hit.to_tuple()
    return t[4], (t[9].get_actor_label() if t[9] else "?")


def main():
    world = ues.get_editor_world()
    if les.is_in_play_in_editor():
        say("PIE is running - stop it first; nothing done")
        return
    if not world or not world.get_path_name().startswith(MAP + "."):
        say("open L_Office_v02 first - this is %s; nothing done" % (world.get_path_name() if world else None))
        return
    actors = eas.get_all_level_actors()
    doors = {a.get_actor_label(): a for a in actors if a.get_class().get_name() == "HiddenDoor"}
    hoop, go = doors.get("BasketballDoor"), doors.get("GoDoor")
    if not (hoop and go):
        say("need BasketballDoor and GoDoor in the open level; nothing done")
        return
    sign = unreal.load_asset(SIGN)
    if not sign and not DRY_RUN:
        say("%s not found - run setup_archaic_temple.py with L_Archaic_Temple open first" % SIGN)
        return

    kart = next((a for a in actors if a.get_actor_label() == "KartLauncher"), None)
    if not kart:
        say("need the KartLauncher in the open level; nothing done")
        return

    # BASKETBALL's glass panel (DoorMesh), and where its pivot sits relative to it.
    panel = hoop.get_editor_property("door_mesh")
    p_o, p_e, _ = unreal.SystemLibrary.get_component_bounds(panel)
    pivot_off = hoop.get_actor_location() - p_o
    width = 2.0 * p_e.y

    # Centred on the kart, along the wall behind it.
    k_o, k_e = kart.get_actor_bounds(True)
    yc = k_o.y
    lo_y, hi_y = yc - 0.5 * width, yc + 0.5 * width
    say("kart spans x %.0f..%.0f, y %.0f..%.0f; the door, centred on it, spans y %.0f..%.0f" % (
        k_o.x - k_e.x, k_o.x + k_e.x, k_o.y - k_e.y, k_o.y + k_e.y, lo_y, hi_y))

    # The wall's face: traced from between the kart and the wall, across the door's width and height.
    # Every trace must meet the same face - a window recess or a step means the door would hang
    # half in the air or half in the wall.
    start_x = k_o.x - k_e.x - 15.0
    faces = []
    for y in (lo_y + 8.0, yc, hi_y - 8.0):
        for z in (80.0, 160.0, 260.0):
            p, who = trace(world, unreal.Vector(start_x, y, z), unreal.Vector(start_x - 300.0, y, z),
                           [hoop] + [a for a in actors if a.get_actor_label() == LABEL])   # not the one it replaces
            say("  y %5.0f z %3.0f: %s at x=%s" % (y, z, who, "%.0f" % p.x if p else "-"))
            faces.append(p.x if p else None)
    if None in faces or max(faces) - min(faces) > 6.0:
        say("the wall there is not one flat face - nothing placed; place the door by hand (duplicate BasketballDoor).")
        return
    face = max(faces)

    # THE BOX-OUT LESSON (2026-10-19): the first copy kept BASKETBALL's x, which stands in front of a
    # window recess - here the wall stands 28 cm further out, and the panel and its sign were inside
    # it. V revealed the door every time, behind the wallpaper. So: the panel's back just in front
    # of this face.
    panel_x = face + CLEAR + p_e.x
    loc = unreal.Vector(panel_x + pivot_off.x, yc + pivot_off.y, hoop.get_actor_location().z)
    offset = loc - hoop.get_actor_location()
    say("the wall's face is at x=%.0f; the temple door %s at (%.0f, %.0f, %.0f), yaw %.0f - its glass %.0f cm proud of the wall, centred behind the kart" % (
        face, "would stand" if DRY_RUN else "stands", loc.x, loc.y, loc.z, hoop.get_actor_rotation().yaw, CLEAR))
    if DRY_RUN:
        return

    for a in actors:
        if a.get_actor_label() == LABEL:
            eas.destroy_actor(a)
    with unreal.ScopedEditorTransaction("Place the Archaic Language Temple door"):
        door = eas.duplicate_actor(hoop, None, offset)
        door.set_actor_label(LABEL)
        door.set_editor_property("travel_target_level", "L_Archaic_Temple")
        door.set_editor_property("arrival_tag", "ArchaicTemple")
        door.set_editor_property("travel_prompt_text", "Enter the Archaic Language Temple  [E]")
        door.set_editor_property("sign_texture", sign)
        door.set_editor_property("always_visible", False)
    say("%s placed at %s -> L_Archaic_Temple (arrival tag ArchaicTemple), seen only with V. One Ctrl+Z removes it. NOT saved." % (
        LABEL, door.get_actor_location()))


main()
