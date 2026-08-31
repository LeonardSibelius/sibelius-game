# make_cafe_from_showcase.py - the cafe behind the deli door.
#
# *** RUN IT TWICE, FROM THE OPEN EDITOR. It tells you what to do between the runs. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_cafe_from_showcase.py"
#
#   run 1  (any level open) -> copies the Showcase scene to /Game/Maps/L_Cafe, STOPS
#   ...you open L_Cafe by hand (File > Open Level)...
#   run 2  (L_Cafe open)    -> clears the demo GameMode, adds a PlayerStart, saves
#
# ===========================================================================
# WHY TWICE. Python that calls load_level crashes this editor - FPyReferenceCollector
# keeps the outgoing world alive and EditorServer.cpp:2524 fatals the process. It cost
# two crashes to learn while building L_City. Level changes belong to the human.
# See make_city_from_downtown.py for the whole story.
#
# ---------------------------------------------------------------------------
# WHAT THIS IS FOR.
#
# Walt, having populated the city with AI ghosts: "Right next to the city dancers is
# Jacob's Downtown Deli. Maybe Nyra could invite the player to go in there."
#
# Nyra's guide line promises "ask me where to go, and I will take you there" - which is
# currently a cheque the city cannot cash, because there is nowhere to go. One door and
# one room makes it true. Not a city of content: a room.
#
# JACOB'S, incidentally, is Jacob Norris - PurePolygons, who made Downtown West and whom
# Walt emailed the night before. His name is on the storefront in his own pack, and it is
# about to become the one place in this game a guide takes you.
#
# ---------------------------------------------------------------------------
# THE SCENE. Leartes Studios' Coffee Shop Environment, free, 197 MB, 4.7 from 32
# ratings, and - the thing that actually mattered in choosing it - "Includes Showcased
# Preassembled Scene". Content/RestaurantScene/Maps/Showcase.umap ships dressed AND lit
# (19 MB of BuiltData), so duplicating it is a finished room in one step, exactly the way
# Downtown West's Demo_Environment became L_City.
#
# Four packs were weighed and rejected first: two were retro/abandoned diners when the
# scene wants somewhere OPEN FOR BUSINESS, one listed 5.0-5.4 against this project's 5.7,
# and the free one was props rather than a room. The criterion that settled it was never
# price - it was "does it come as a room, and is it realistic".

import json
import traceback

import unreal

SRC = "/Game/RestaurantScene/Maps/Showcase"
DST = "/Game/Maps/L_Cafe"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/make_cafe_from_showcase.json"

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level
    exists = unreal.EditorAssetLibrary.does_asset_exist(DST)

    # ================================================== RUN 2: L_Cafe is open
    if open_level == DST:
        r["run"] = 2

        world = ues.get_editor_world()
        settings = world.get_world_settings()

        # A vendor demo map usually pins its own GameMode so the pack is playable
        # standalone. Left alone that spawns the pack's pawn instead of Leonard - no HUD,
        # no [O], no [>], no journal. Cleared so it falls through to the project default.
        before = settings.get_editor_property("default_game_mode")
        r["game_mode_was"] = str(before) if before else None
        try:
            settings.set_editor_property("default_game_mode", None)
        except Exception as e:
            r["game_mode_clear_error"] = str(e)
        after = settings.get_editor_property("default_game_mode")
        r["game_mode_cleared"] = after is None

        actors = eas.get_all_level_actors()
        r["actor_count"] = len(actors)

        # ---- somewhere to stand ------------------------------------------
        # A showcase scene is built for a CAMERA, not a player, so it often has no
        # PlayerStart at all - and a level without one drops the player at the origin,
        # which in a dressed interior is usually inside a wall or under the floor.
        starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
        if starts:
            p = starts[0].get_actor_location()
            r["player_start"] = "already here"
            r["start_at"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
        else:
            s = eas.spawn_actor_from_class(unreal.PlayerStart,
                                           unreal.Vector(0.0, 0.0, 120.0))
            s.set_actor_label("Cafe_PlayerStart")
            r["player_start"] = "spawned at the origin"
            r["start_at"] = [0.0, 0.0, 120.0]
            r["MOVE_IT"] = ("Spawned at the world origin, which in a dressed interior is "
                            "probably inside a wall. Drag it to the doorway and re-save.")

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Play L_Cafe. You should be Leonard, standing in the cafe, with the "
                     "HUD. Then tell me and I will wire the deli door in L_City to it.")

        # Hand the world back before this script's globals outlive it - every UObject a
        # Python name still holds is one the editor cannot collect at the next map change.
        del world, settings, actors, starts
        unreal.SystemLibrary.collect_garbage()

    # ============================================ RUN 1: copy, then stop
    elif not exists:
        r["run"] = 1
        if not unreal.EditorAssetLibrary.does_asset_exist(SRC):
            raise Exception("no %s - is the Coffee Shop pack added to the project?" % SRC)

        ok = unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
        r["duplicate"] = "created" if ok else "FAILED"
        if not ok:
            raise Exception("duplicate_asset %s -> %s failed" % (SRC, DST))

        # The baked lighting rides in its own package. Say plainly whether it came: a
        # showcase interior without its BuiltData is a flat, wrong-looking room.
        r["built_data_copied"] = unreal.EditorAssetLibrary.does_asset_exist(DST + "_BuiltData")
        r["next"] = ("NOW OPEN L_Cafe BY HAND: File > Open Level > Maps > L_Cafe. Then "
                     "run this again. Do NOT let a script change the level.")
        unreal.SystemLibrary.collect_garbage()

    # ============================================ exists but not open
    else:
        r["run"] = "waiting"
        r["next"] = ("L_Cafe already exists but is not the open level. Open it by hand "
                     "and run this again. To rebuild it from scratch, delete it in the "
                     "Content Browser first - Python must not delete or load levels.")

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###CAFE### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
