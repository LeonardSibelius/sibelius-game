# repoint_carousel_door.py — HEADLESS PASS B: point the office kitchen door at
# the library-tower Carousel room. Run editor-closed, in its own process,
# AFTER build_carousel_library_room.py:
#
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript
#       -script="Tools/Scripts/repoint_carousel_door.py" -stdout

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/L_Office_v02")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

door = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "CarouselDoor_Kitchen":
        door = a
        break

if not door:
    print("RESULT: CarouselDoor_Kitchen NOT FOUND")
elif str(door.get_editor_property("TargetLevelName")) == "L_Carousel":
    print("RESULT: door already points at L_Carousel")
else:
    door.set_editor_property("TargetLevelName", "L_Carousel")
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("RESULT: door repointed to L_Carousel and saved")
