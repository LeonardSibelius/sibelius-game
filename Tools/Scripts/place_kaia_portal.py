# place_kaia_portal.py — the portal at the end of the hall, the way to Kaia's home.
#
# Walt, 2026-10-02: "when the player starts the game he is looking down the hall at
# BP_Door_Outside_A3. I want one of the portals from my portal pack right there with a "C"
# prompt to go thru it, just like at the spaceport. Might as well make him go upstairs and
# earn Compile from Elise before he can visit Kaia's home." See CompilePortal.h.
#
# Measures the door and the PlayerStart rather than trusting numbers: the portal stands
# centred on the door, on the side the player comes from, just clear of the door's face.
#
# RUN with L_Office_v02 open, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_kaia_portal.py"
# Safe to re-run: a KaiaPortal already there is only re-aimed. Never loads a level (that
# crashes the editor from a script - open L_Office_v02 by hand).

import unreal

TAG = "###KAIAPORTAL###"
DOOR = "BP_Door_Outside_A3"
LABEL = "KaiaPortal"
CLEAR = 30.0        # cm between the door's face and the portal's centre
DESTINATION = "L_KaiaHome"      # Tools/Scripts/make_kaia_home.py
ARRIVAL_TAG = "KaiaHome"        # its PlayerStart

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def aim(portal):
    portal.set_editor_property("travel_target_level", DESTINATION)
    portal.set_editor_property("arrival_tag", ARRIVAL_TAG)


def main():
    actors = eas.get_all_level_actors()

    # ONE ALREADY STANDING IS AIMED WHERE IT STANDS, not re-placed: the first one went in
    # before L_KaiaHome existed, pointing nowhere ("THE PORTAL HUMS..." - Walt, 2026-10-03),
    # and re-measuring would undo anything moved by hand since.
    standing = [a for a in actors if a.get_actor_label() == LABEL]
    if standing:
        for portal in standing:
            aim(portal)
        les.save_current_level()
        say("%d %s aimed at %s (tag %s), left where it stands. Saved." % (len(standing), LABEL, DESTINATION, ARRIVAL_TAG))
        return

    door = next((a for a in actors if a.get_actor_label() == DOOR), None)
    start = next((a for a in actors if a.get_class().get_name() == "PlayerStart"
                  and not a.get_editor_property("player_start_tag")), None) \
        or next((a for a in actors if a.get_class().get_name() == "PlayerStart"), None)
    if not door or not start:
        say("need %s and a PlayerStart - open L_Office_v02." % DOOR)
        return

    origin, ext = door.get_actor_bounds(False)
    # The door is thin along one horizontal axis: that axis is its facing.
    thin_is_y = ext.y < ext.x
    to_player = start.get_actor_location() - origin
    if thin_is_y:
        side = 1.0 if to_player.y > 0 else -1.0
        where = unreal.Vector(origin.x, origin.y + side * (ext.y + CLEAR), origin.z)
        yaw = 0.0
    else:
        side = 1.0 if to_player.x > 0 else -1.0
        where = unreal.Vector(origin.x + side * (ext.x + CLEAR), origin.y, origin.z)
        yaw = 90.0
    say("door bounds centre %s, size %.0f x %.0f x %.0f; player start %s" % (
        origin, 2 * ext.x, 2 * ext.y, 2 * ext.z, start.get_actor_location()))

    portal = eas.spawn_actor_from_class(unreal.CompilePortal, where, unreal.Rotator(0.0, 0.0, yaw))
    portal.set_actor_label(LABEL)
    aim(portal)
    les.save_current_level()
    say("%s placed at %s, yaw %.0f, effect %s. Saved." % (
        LABEL, portal.get_actor_location(), yaw, portal.get_editor_property("portal_system")))


main()
