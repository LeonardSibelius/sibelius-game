# build_carousel_library_room.py — the Carousel of Fates gets a real room.
#
# HEADLESS PASS A (run editor-CLOSED via commandlet python — the live-editor
# attempt crashed on world-teardown leaks, EditorServer.cpp:2524; in a
# throwaway process a teardown grump after the save is harmless):
#
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript
#       -script="Tools/Scripts/build_carousel_library_room.py" -stdout
#
# Duplicates the Modular Library Tower demo (Content/Library, Fab) into
# /Game/Maps/L_Carousel — vendor folder untouched — quiets the pack's
# editor-spawned book tilers (the 4k-actor weight), then wires the Carousel
# game mode, a PlayerStart, and the ACarouselMachine, and SAVES. Pass B
# (repoint_carousel_door.py) points the office door here. Idempotent.

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

# Quiet the pack's in-editor book spawners (listing's own advice: set
# spawnBooksInEditor false; books respawn at play). Property name probed
# defensively — BP-declared names can surface differently.
tilers = 0
for a in actors:
    if "BookTiler" in a.get_class().get_name():
        for prop in ("spawnBooksInEditor", "Spawn Books In Editor", "bSpawnBooksInEditor"):
            try:
                a.set_editor_property(prop, False)
                a.rerun_construction_scripts()
                tilers += 1
                break
            except Exception:
                continue
results.append("book tilers quieted: %d" % tilers)

# GameMode override (WorldSettings is not a "level actor" — fetch by class).
gm_cls = unreal.load_class(None, "/Script/SibeliusGame.CarouselGameMode")
for ws in unreal.GameplayStatics.get_all_actors_of_class(ues.get_editor_world(), unreal.WorldSettings):
    ws.set_editor_property("default_game_mode", gm_cls)
results.append("gamemode: CarouselGameMode")

# PlayerStart: reuse the demo's if present, else spawn near the origin.
start = None
for a in eas.get_all_level_actors():
    if a.get_class().get_name() == "PlayerStart":
        start = a
        break
if start:
    results.append("playerstart: demo's own at %s" % start.get_actor_location())
else:
    start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 150.0),
                                       unreal.Rotator(0.0, 0.0, 0.0))
    start.set_actor_label("PlayerStart")
    results.append("playerstart: spawned at origin")

# The machine, near the start so it can't be lost; Walt composes it by eye.
if any(a.get_actor_label() == "CarouselMachine" for a in eas.get_all_level_actors()):
    results.append("machine: already placed")
else:
    machine_cls = unreal.load_class(None, "/Script/SibeliusGame.CarouselMachine")
    loc = start.get_actor_location() + unreal.Vector(400.0, 0.0, 0.0)
    machine = eas.spawn_actor_from_class(machine_cls, loc, unreal.Rotator(0.0, 0.0, 180.0))
    machine.set_actor_label("CarouselMachine")
    results.append("machine: placed at %s" % loc)

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
results.append("SAVED")

print("RESULT: " + " | ".join(results))
