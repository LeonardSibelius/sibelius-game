# place_not_human_terminal.py — the PROVE YOU'RE NOT HUMAN terminal, over the laptop on the
# living-room desk, under the sign (Walt, 2026-10-11: "I want an 'E' trigger at the desk with
# the laptop to launch the idea").
#
# *** RUN FROM THE OPEN EDITOR with L_Office_v02 loaded, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_not_human_terminal.py"
#
# Finds the laptop mesh nearest the sign (Sign_NotHuman_Text), fits ANotHumanTerminal's shell
# to that laptop's bounds (a little bigger, so it is easy to look at) and saves the level. Safe
# to re-run: it moves the terminal it made rather than adding a second. Never loads a level.

import unreal

TAG = "###NOTHUMAN###"
SIGN = "Sign_NotHuman_Text"
LAPTOP_MESH = "SM_Laptop_A1"
PAD = 4.0   # cm added round the laptop's bounds

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def main():
    world = ues.get_editor_world()
    if "L_Office_v02" not in world.get_path_name() or ues.get_game_world():
        say("REFUSING: open L_Office_v02 with Play stopped. Nothing changed.")
        return
    actors = eas.get_all_level_actors()
    sign = next((a for a in actors if a.get_actor_label() == SIGN), None)
    if not sign:
        say("no %s in this level - nothing to stand under." % SIGN)
        return
    at = sign.get_actor_location()
    laptops = [a for a in actors if a.get_class().get_name() == "StaticMeshActor"
               and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == LAPTOP_MESH]
    if not laptops:
        say("no %s in this level." % LAPTOP_MESH)
        return
    laptop = min(laptops, key=lambda a: (a.get_actor_location() - at).length())
    distance = (laptop.get_actor_location() - at).length()
    if distance > 200.0:
        say("the nearest laptop (%s) is %.0f cm from the sign - refusing to guess." % (laptop.get_actor_label(), distance))
        return
    origin, ext = laptop.get_actor_bounds(False)

    cls = unreal.load_class(None, "/Script/SibeliusGame.NotHumanTerminal")
    if not cls:
        say("NotHumanTerminal is not built yet - build the editor first.")
        return
    existing = [a for a in actors if a.get_class().get_name() == "NotHumanTerminal"]
    terminal = existing[0] if existing else eas.spawn_actor_from_class(cls, origin, unreal.Rotator(0.0, 0.0, 0.0))
    for extra in existing[1:]:
        eas.destroy_actor(extra)
    terminal.set_actor_location(origin, False, False)
    terminal.set_actor_label("NotHumanTerminal")
    shell = terminal.get_editor_property("shell")
    shell.set_editor_property("box_extent", unreal.Vector(ext.x + PAD, ext.y + PAD, ext.z + PAD))
    les.save_current_level()
    say("NotHumanTerminal over %s at (%.0f, %.0f, %.0f), shell half-size (%.0f, %.0f, %.0f). Saved." % (
        laptop.get_actor_label(), origin.x, origin.y, origin.z, ext.x + PAD, ext.y + PAD, ext.z + PAD))


main()
