# build_carousel_test_map.py — SIB-46 grey-box presentation slice (June 2026).
#
# Builds /Game/Maps/L_Carousel_Test: a flat floor, a sun + skylight, a PlayerStart, the
# ACarouselMachine, and the ACarouselGameMode override (Canvas HUD + flying DefaultPawn). Placeholder
# cubes only — no temple art — so the map stays small and portable to the lean fork.
#
# ORDER: Build.bat the editor target FIRST (the C++ classes must exist), then open the editor and run:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_carousel_test_map.py"
# Then PIE L_Carousel_Test: fly to the machine, press E to pull the lever.

import unreal

MAP = "/Game/Maps/L_Carousel_Test"
TAG = "###CAROUSEL_TEST###"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

machine_cls = unreal.load_class(None, "/Script/SibeliusGame.CarouselMachine")
gm_cls = unreal.load_class(None, "/Script/SibeliusGame.CarouselGameMode")
if machine_cls is None or gm_cls is None:
    unreal.log_error("%s C++ classes not found — Build.bat the SibeliusGameEditor target first." % TAG)
else:
    les.new_level(MAP)

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    floor = eas.spawn_actor_from_object(cube, unreal.Vector(0, 0, 0)) if cube else None
    if floor:
        floor.set_actor_scale3d(unreal.Vector(24.0, 24.0, 0.5))
        floor.set_actor_label("Floor")

    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 600), unreal.Rotator(-50.0, -30.0, 0.0))
    sun.set_actor_label("Sun")
    sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600))
    sky.set_actor_label("SkyLight")

    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-450.0, 0.0, 150.0), unreal.Rotator(0.0, 0.0, 0.0))
    ps.set_actor_label("PlayerStart")

    machine = eas.spawn_actor_from_class(machine_cls, unreal.Vector(0.0, 0.0, 100.0), unreal.Rotator(0.0, 180.0, 0.0))
    machine.set_actor_label("CarouselMachine")

    # GameMode override on the level's World Settings. NB: get_all_level_actors() does NOT return the
    # WorldSettings actor — fetch it by class.
    ok_gm = False
    world = ues.get_editor_world()
    for ws in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings):
        ws.set_editor_property("default_game_mode", gm_cls)
        ok_gm = True

    les.save_current_level()
    unreal.log("%s built %s (gamemode override set=%s). PIE it: fly to the machine, press E." % (TAG, MAP, ok_gm))
