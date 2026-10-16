# place_office_slot.py - a second Celestial Fortune machine, in the office, where a visitor
# meets it in the first minutes.
#
# Walt, 2026-10-16, with a screenshot of the hall: "how about putting one in front of
# SM_Corner_1Floor_Inside_A3 and if you win you get 10 sauce?" - big wins only, his choice, then
# "let's make it a win of 3x the bet (450) gets you 10 sauce".
# The cathedral's machine is at the end of the game, behind every power; this one is an arcade
# machine (bIsTheCathedralMachine off): it plays for fun and 10 sauce on a spin that pays 3x the
# bet, and touches none of the cathedral's story - see SlotCabinet.h.
#
# It stands with its back to the wall piece's face that looks into the room (measured: the -Y
# face, y = 10010; floor z = 60, ceiling z = 308), at 0.88 scale - the cabinet is 2.5 m and the
# room 2.48 m - so 2.2 m tall and 88 cm wide on a 150 cm wall. The level is NOT saved: Walt
# looks, then saves L_Office_v02 himself.
#
# *** RUN FROM THE OPEN EDITOR, L_Office_v02 open, PIE stopped ***

import unreal

WALL = "SM_Corner_1Floor_Inside_A3"
LABEL = "SlotCabinet_Office"
SCALE = 0.88
BACK = 39.0                 # the cabinet's back is 39 cm behind its origin (mesh bounds x -39..56)
GAP = 2.0                   # off the wallpaper
FLOOR_Z = 60.0
TAG = "###OFFICESLOT###"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def main():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        return "REFUSING: stop Play first"
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_name() != "L_Office_v02":
        return "REFUSING: the open level is %s, not L_Office_v02" % world.get_name()
    actors = eas.get_all_level_actors()
    if any(a.get_actor_label() == LABEL for a in actors):
        return "%s is already in the level - left as it is" % LABEL
    wall = next((a for a in actors if a.get_actor_label() == WALL), None)
    if not wall:
        return "no %s in the level" % WALL
    o, e = wall.get_actor_bounds(False)
    face_y = o.y - e.y                                   # the face looking into the room (-Y)
    loc = unreal.Vector(o.x, face_y - (BACK * SCALE + GAP), FLOOR_Z)
    # Built facing local +X; yaw -90 turns that to world -Y, away from the wall.
    cab = eas.spawn_actor_from_class(unreal.SlotCabinet, loc, unreal.Rotator(roll=0, pitch=0, yaw=-90))
    if not cab:
        return "could not spawn the cabinet"
    cab.set_actor_label(LABEL)
    cab.set_actor_scale3d(unreal.Vector(SCALE, SCALE, SCALE))
    cab.set_editor_property("is_the_cathedral_machine", False)
    cab.set_editor_property("sauce_for_a_big_win", 10)
    cab.set_editor_property("big_win_times_bet", 3)
    comp = cab.get_editor_property("cabinet_mesh")
    mesh = comp.get_editor_property("static_mesh")
    if not mesh:
        comp.set_static_mesh(unreal.load_asset("/Game/SlotFactory/Cabinet/SM_CelestialCabinet"))
    _, ce = cab.get_actor_bounds(False)
    return ("%s placed at (%.0f, %.0f, %.0f), yaw -90, scale %.2f, mesh %s; %0.f x %0.f x %0.f cm; "
            "arcade machine, 10 sauce for a 3x win. L_Office_v02 NOT saved - Walt saves it."
            % (LABEL, loc.x, loc.y, loc.z, SCALE, comp.get_editor_property("static_mesh").get_name(),
               ce.x * 2, ce.y * 2, ce.z * 2))


unreal.log("%s %s" % (TAG, main()))
