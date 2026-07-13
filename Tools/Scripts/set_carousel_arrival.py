# set_carousel_arrival.py — Walt's arrival shot for the Carousel of Fates.
#
# Captured live from his PIE pawn (2026-07-12): stand at (6.4, 227.9, 208.7)
# facing yaw 90 — bookshelves either side, the golden gallery dead ahead.
# PlayerStart goes exactly there; the machine plants ~700 units down the view
# axis, turned to face the arriving player. Machine keeps its own Z (floor).

import unreal

ARRIVE = unreal.Vector(6.4, 227.9, 208.7)
ARRIVE_YAW = 90.0
MACHINE_AHEAD = 700.0   # units down the +Y view axis

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_Carousel")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

start = None
machine = None
for a in eas.get_all_level_actors():
    if a.get_class().get_name() == "PlayerStart":
        start = a
    elif a.get_actor_label() == "CarouselMachine":
        machine = a

results = []
if start:
    start.set_actor_location(ARRIVE, False, False)
    start.set_actor_rotation(unreal.Rotator(0.0, 0.0, ARRIVE_YAW), False)
    results.append("PlayerStart at Walt's spot, facing the gallery")
else:
    results.append("PlayerStart NOT FOUND")

if machine:
    old_z = machine.get_actor_location().z
    machine.set_actor_location(unreal.Vector(ARRIVE.x, ARRIVE.y + MACHINE_AHEAD, old_z), False, False)
    machine.set_actor_rotation(unreal.Rotator(0.0, 0.0, ARRIVE_YAW + 180.0), False)
    results.append("machine %d units ahead, facing the player (z kept at %.0f)" % (int(MACHINE_AHEAD), old_z))
else:
    results.append("CarouselMachine NOT FOUND")

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print("RESULT: " + " | ".join(results))
