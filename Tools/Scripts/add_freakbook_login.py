# add_freakbook_login.py — FREAKBOOK LOGIN under PROVE YOU'RE NOT HUMAN on the living-room sign
# (Walt, 2026-10-12: "Perhaps write 'Freakbook Login' on the sign underneath 'Prove You're Not
# Human'"). Freakbook itself is designed in docs/FREAKBOOK.md and not built yet.
#
# *** RUN FROM THE OPEN EDITOR with L_Office_v02 loaded, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/add_freakbook_login.py"
#
# Adds a second Text Render actor, Sign_NotHuman_Login: a smaller line in the same font, colour
# and alignment, under Sign_NotHuman_Text. The plate grows UPWARD to hold both lines: its bottom
# edge stays where Walt put it, just above the desk lamp and the laptop, which would stand in
# front of anything lower. The main line moves up with it. Margins stay as Walt's sign had them.
# Every position is worked out from the plate's bottom edge, so re-running gives the same sign.
# Saves the level. Never loads a level.
#
# Text Render actors report a placeholder 128 cm box as their bounds; the text's real size comes
# from GetTextWorldSize.

import unreal

TAG = "###LOGIN###"
SIGN = "Sign_NotHuman_Text"
PLATE = "Sign_NotHuman_Plate"
LOGIN = "Sign_NotHuman_Login"
WORDS = "FREAKBOOK LOGIN"
SIZE_RATIO = 0.6   # the login line's world size, against the main line's
GAP_RATIO = 0.35   # the space between the lines, against the login line's height
MARGIN = 9.0       # cm of plate above and below the words: what Walt's one-line sign had
CUBE_CM = 100.0    # /Engine/BasicShapes/Cube is 100 cm a side, pivot in the middle

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def upright(actor):
    rot = actor.get_actor_rotation()
    return abs(rot.pitch) < 1.0 and abs(rot.roll) < 1.0


def main():
    world = ues.get_editor_world()
    if "L_Office_v02" not in world.get_path_name() or ues.get_game_world():
        say("REFUSING: open L_Office_v02 with Play stopped. Nothing changed.")
        return
    by_label = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    sign, plate = by_label.get(SIGN), by_label.get(PLATE)
    if not sign or not plate:
        say("need both %s and %s in this level. Nothing changed." % (SIGN, PLATE))
        return
    if not upright(sign) or not upright(plate):
        say("the sign is tilted (text %s, plate %s) - refusing to guess which way is up. Nothing changed." % (
            sign.get_actor_rotation(), plate.get_actor_rotation()))
        return
    main_text = sign.text_render
    if main_text.get_editor_property("vertical_alignment") != unreal.VerticalTextAligment.EVRTA_TEXT_CENTER:
        say("the main line is not centred vertically - this script places by centres. Nothing changed.")
        return

    login = by_label.get(LOGIN) or eas.spawn_actor_from_class(
        unreal.TextRenderActor, sign.get_actor_location(), sign.get_actor_rotation())
    login.set_actor_label(LOGIN)
    login.set_actor_rotation(sign.get_actor_rotation(), False)
    login.set_actor_scale3d(sign.get_actor_scale3d())
    text = login.text_render
    text.set_text(WORDS)
    for prop in ("font", "text_material", "text_render_color", "horizontal_alignment", "vertical_alignment",
                 "x_scale", "y_scale"):
        text.set_editor_property(prop, main_text.get_editor_property(prop))
    text.set_editor_property("world_size", main_text.get_editor_property("world_size") * SIZE_RATIO)

    main_h = main_text.get_text_world_size().z
    login_h = text.get_text_world_size().z
    gap = GAP_RATIO * login_h

    _, plate_ext = plate.get_actor_bounds(False)
    bottom = plate.get_actor_location().z - plate_ext.z
    login_z = bottom + MARGIN + login_h / 2.0
    main_z = login_z + login_h / 2.0 + gap + main_h / 2.0
    top = main_z + main_h / 2.0 + MARGIN

    at = sign.get_actor_location()
    sign.set_actor_location(unreal.Vector(at.x, at.y, main_z), False, False)
    login.set_actor_location(unreal.Vector(at.x, at.y, login_z), False, False)
    scale = plate.get_actor_scale3d()
    plate.set_actor_scale3d(unreal.Vector(scale.x, scale.y, (top - bottom) / CUBE_CM))
    ploc = plate.get_actor_location()
    plate.set_actor_location(unreal.Vector(ploc.x, ploc.y, (top + bottom) / 2.0), False, False)

    les.save_current_level()
    say("plate %.1f to %.1f cm (was %.1f tall, now %.1f)." % (bottom, top, 2.0 * plate_ext.z, top - bottom))
    say("PROVE YOU'RE NOT HUMAN centred at %.1f (was %.1f); %s centred at %.1f, %.1f wide x %.1f tall." % (
        main_z, at.z, WORDS, login_z, text.get_text_world_size().y, login_h))
    say("Saved L_Office_v02.")


main()
