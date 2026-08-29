# make_city_from_downtown.py - turn the Downtown West demo street into L_City.
#
# *** RUN IT TWICE, FROM THE OPEN EDITOR. It tells you what to do between the runs. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_city_from_downtown.py"
#
#   run 1  (any level open) -> copies the demo street to /Game/Maps/L_City, then STOPS
#   ...you open L_City by hand (File > Open Level)...
#   run 2  (L_City open)    -> clears the demo GameMode, checks it, saves
#
# ===========================================================================
# WHY IT IS TWO RUNS INSTEAD OF ONE, AND WHY THAT IS NOT LAZINESS.
#
# The one-run version called les.load_level() after duplicating, and it took the editor
# down twice. The log said exactly why:
#
#   Old world /Game/Maps/L_City not cleaned up by garbage collection while loading new
#   map!  Referenced by: -> FPyReferenceCollector::AddReferencedObjects(World ...)
#
# FPyReferenceCollector is Unreal's Python GC bridge. Any UObject a Python variable is
# holding is kept alive - so a script that touches a world and then changes maps stops
# the editor collecting the old one, and EditorServer.cpp:2524 fatals the process. Not a
# race, not the disk: deterministic, and it fired at the same file and line both times.
#
# THE RULE THIS PROJECT ALREADY KNEW. place_meadow_door.py says "RUN TWICE, ONCE PER
# LEVEL, FROM THE OPEN EDITOR" and never calls load_level. That was not a style
# preference, it was this crash being avoided. This script now follows it.
#
# Level changes belong to the human, who makes them through the editor's own map-change
# path with nothing of Python's holding the door open.
#
# ---------------------------------------------------------------------------
# WHY DUPLICATE THE DEMO MAP AT ALL.
#
# Downtown West ships Maps/Demo_Environment: a full shopping street, composed and lit by
# the six artists who made the pack. That IS a real city block, and better than anything
# this project would assemble from the same parts in a week.
#
# TAKE, not point at. CLAUDE.md's rule for vendor packs is duplicate-out-and-rename,
# never author into the pack - so L_City is a copy in /Game/Maps beside every other
# level, and /Game/Downtown_West stays as Fab delivered it. A pack update can then never
# quietly redecorate the ending of the game.
#
# WHAT RUN 2 FIXES THAT A PLAIN DUPLICATE DOES NOT: a vendor demo map usually pins its
# own GameMode in World Settings so the pack is playable standalone. Left alone that
# spawns the pack's mannequin instead of Leonard Sibelius - no HUD, no [O], no [>], no
# journal. Cleared, so the map falls through to the project default like every other
# level in the game.

import json
import traceback

import unreal

SRC = "/Game/Downtown_West/Maps/Demo_Environment"
DST = "/Game/Maps/L_City"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/make_city_from_downtown.json"

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    # Which level is open right now? Asked as a STRING and nothing is kept, so the
    # question itself costs no reference. Everything below branches on it.
    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level
    exists = unreal.EditorAssetLibrary.does_asset_exist(DST)

    # ================================================== RUN 2: L_City is open
    if open_level == DST:
        r["run"] = 2

        world = ues.get_editor_world()
        settings = world.get_world_settings()

        before = settings.get_editor_property("default_game_mode")
        r["game_mode_was"] = str(before) if before else None
        try:
            settings.set_editor_property("default_game_mode", None)
        except Exception as e:
            r["game_mode_clear_error"] = str(e)
        after = settings.get_editor_property("default_game_mode")
        r["game_mode_now"] = str(after) if after else None
        r["game_mode_cleared"] = after is None

        # PROVE it is the street and not the grey boxes. The placeholder was 42 actors;
        # a dressed shopping street is hundreds. This script has already once reported a
        # success it had not achieved, so the last thing it does is count what is really
        # in front of it.
        actors = eas.get_all_level_actors()
        labels = [a.get_actor_label() for a in actors]
        starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
        r["actor_count"] = len(labels)
        r["player_starts"] = len(starts)
        r["placeholder_gone"] = "City_Ground" not in labels
        r["is_the_street"] = len(labels) > 100 and "City_Ground" not in labels
        if starts:
            p = starts[0].get_actor_location()
            r["first_start"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
        else:
            s = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 200))
            s.set_actor_label("City_PlayerStart")
            r["note"] = "no PlayerStart - spawned one at the origin, MOVE IT by hand"

        les.save_current_level()
        r["saved"] = True
        r["next"] = "Play L_City. You should be Leonard, on a street, with the HUD."

        # HAND THE WORLD BACK before this script's globals outlive it. Every UObject a
        # Python name is still bound to is one the editor cannot collect at the next map
        # change - which is precisely how this script crashed the editor twice. Dropping
        # the names and collecting here makes opening another level afterwards safe.
        del world, settings, before, after, actors, labels, starts
        unreal.SystemLibrary.collect_garbage()

    # ============================================ RUN 1: make the copy, then stop
    elif not exists:
        r["run"] = 1
        if not unreal.EditorAssetLibrary.does_asset_exist(SRC):
            raise Exception("no %s - is Downtown West installed into the project?" % SRC)

        ok = unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
        r["duplicate"] = "created" if ok else "FAILED"
        if not ok:
            raise Exception("duplicate_asset %s -> %s failed" % (SRC, DST))

        r["built_data_copied"] = unreal.EditorAssetLibrary.does_asset_exist(DST + "_BuiltData")
        r["next"] = ("NOW OPEN L_City BY HAND: File > Open Level > Maps > L_City. "
                     "Then run this script again to finish it. Do NOT let a script "
                     "change the level - that is what crashed the editor.")

        # Nothing of the new world is held - the duplicate is an asset operation, not a
        # world one. Sweep anyway so the map change he is about to make starts clean.
        unreal.SystemLibrary.collect_garbage()

    # ============================================ L_City exists but is not open
    else:
        r["run"] = "waiting"
        r["next"] = ("L_City already exists but is not the open level. Open it by hand "
                     "(File > Open Level > Maps > L_City) and run this again. If you "
                     "want it rebuilt from scratch, delete it in the Content Browser "
                     "first - Python must not delete or load levels in this project.")

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###CITY### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
