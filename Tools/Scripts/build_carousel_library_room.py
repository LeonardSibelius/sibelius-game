# build_carousel_library_room.py — the Carousel of Fates gets a real room.
#
# Duplicates the Modular Library Tower demo scene (Content/Library, Fab) into
# /Game/Maps/L_Carousel — vendor folder untouched — then gives it the Carousel
# game mode, a PlayerStart, and the ACarouselMachine, and repoints the office
# kitchen door at it. Idempotent: skips the duplicate if L_Carousel exists;
# ensure-by-label for the actors.

import unreal

SRC = "/Game/Library/Maps/LibraryDemo"
DST = "/Game/Maps/L_Carousel"
results = []

# --- 1) Duplicate the demo (once) ---------------------------------------------
if unreal.EditorAssetLibrary.does_asset_exist(DST):
    results.append("L_Carousel: already exists")
else:
    ok = unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
    results.append("L_Carousel: duplicated=%s" % bool(ok))

# --- 2) Wire the duplicate -----------------------------------------------------
world = unreal.EditorLoadingAndSavingUtils.load_map(DST)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = eas.get_all_level_actors()


def find_by_class(cls_name):
    for a in actors:
        if a.get_class().get_name() == cls_name:
            return a
    return None


def find_by_label(label):
    for a in actors:
        if a.get_actor_label() == label:
            return a
    return None


# GameMode override (the build_carousel_test_map trick: WorldSettings is not a
# "level actor" — fetch it by class).
gm_cls = unreal.load_class(None, "/Script/SibeliusGame.CarouselGameMode")
for ws in unreal.GameplayStatics.get_all_actors_of_class(ues.get_editor_world(), unreal.WorldSettings):
    ws.set_editor_property("default_game_mode", gm_cls)
results.append("gamemode: CarouselGameMode")

# PlayerStart: reuse the demo's if it has one, else spawn near the origin.
start = find_by_class("PlayerStart")
if start:
    results.append("playerstart: demo's own at %s" % start.get_actor_location())
else:
    start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 150.0),
                                       unreal.Rotator(0.0, 0.0, 0.0))
    start.set_actor_label("PlayerStart")
    results.append("playerstart: spawned at origin")

# The machine, near the start so it can't be lost; Walt composes it properly by eye.
if find_by_label("CarouselMachine"):
    results.append("machine: already placed")
else:
    machine_cls = unreal.load_class(None, "/Script/SibeliusGame.CarouselMachine")
    loc = start.get_actor_location() + unreal.Vector(400.0, 0.0, 0.0)
    machine = eas.spawn_actor_from_class(machine_cls, loc, unreal.Rotator(0.0, 0.0, 180.0))
    machine.set_actor_label("CarouselMachine")
    results.append("machine: placed at %s" % loc)

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()

# --- 3) Repoint the office door ------------------------------------------------
unreal.EditorLoadingAndSavingUtils.load_map("/Game/L_Office_v02")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
door = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "CarouselDoor_Kitchen":
        door = a
        break
if door:
    door.set_editor_property("TargetLevelName", "L_Carousel")
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    results.append("door: repointed to L_Carousel")
else:
    results.append("door: NOT FOUND")

print("RESULT: " + " | ".join(results))
