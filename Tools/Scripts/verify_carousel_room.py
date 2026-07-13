# verify_carousel_room.py — headless check of the Carousel library room wiring.
# Prints one VERIFY line per fact; run editor-closed via -run=pythonscript.

import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_Carousel")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = eas.get_all_level_actors()

gm = "none"
for ws in unreal.GameplayStatics.get_all_actors_of_class(ues.get_editor_world(), unreal.WorldSettings):
    cls = ws.get_editor_property("default_game_mode")
    gm = cls.get_name() if cls else "none"
machine = any(a.get_actor_label() == "CarouselMachine" for a in actors)
start = any(a.get_class().get_name() == "PlayerStart" for a in actors)
print("VERIFY L_Carousel: actors=%d gamemode=%s machine=%s playerstart=%s" %
      (len(actors), gm, machine, start))

unreal.EditorLoadingAndSavingUtils.load_map("/Game/L_Office_v02")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "CarouselDoor_Kitchen":
        print("VERIFY door: TargetLevelName=%s" % a.get_editor_property("TargetLevelName"))
        break
