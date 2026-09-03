# make_ufoods_from_showcase.py - the supermarket behind the uFoods door.
#
# *** RUN IT TWICE, FROM THE OPEN EDITOR. It tells you what to do between the runs. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_ufoods_from_showcase.py"
#
#   run 1  (any level open) -> copies L_Showcase to /Game/Maps/L_uFoods, STOPS
#   ...you open L_uFoods by hand (File > Open Level)...
#   run 2  (L_uFoods open)  -> clears the demo GameMode, adds a PlayerStart, saves
#
# ===========================================================================
# WHY TWICE. Python that calls load_level crashes this editor - FPyReferenceCollector
# keeps the outgoing world alive and EditorServer.cpp:2524 fatals the process. It cost
# two crashes to learn while building L_City. Level changes belong to the human.
# This is make_cafe_from_showcase.py's recipe, unchanged where it was already right.
#
# ---------------------------------------------------------------------------
# WHAT THIS IS FOR.  docs/SPACEPORT_PLAN.md Phase E - the supply run.
#
# Walt: "can she appear after the spaceport appears and tell Leonard to go to uFoods to
# buy supplies for his space voyage?"
#
# uFoods has been a painted storefront on the corner since Downtown West shipped. Phase E
# makes it a room, for the same reason the deli became one: a guide who says "go there"
# is writing a cheque the city has to cash.
#
# ---------------------------------------------------------------------------
# THE SCENE. Poly Supermarket 01, bought from Fab 2026-09-03. dump_supermarket_layout.py
# read L_Showcase before a line of this was written, and it is emphatically a ROOM:
#
#     1,956 pack meshes        225 of them shell (53 roof, 56 floor, 43 wall, 6 columns)
#     17.6 m x 20.0 m          5.5 m ceiling
#     bakery and meat sections, fridges, aisles, 286 price tags
#
# Building that from the Modular kit would take days and come out worse. Duplicating a
# dressed, lit showcase is the same move that made L_City out of Downtown West's
# Demo_Environment and L_Cafe out of RestaurantScene's Showcase.
#
# LICENCE: the .umap this creates is OURS and gets committed; it holds transforms and
# asset PATHS, not the vendor's bytes. Content/Poly_Supermarket_01/ stays gitignored.
#
# HOW IT COOKS - and this is the opposite of the reflex this project has earned. The
# usual bug here is gitignored content that never reaches the pak, and the usual fix is
# a DirectoriesToAlwaysCook line. NOT THIS TIME. That fix is for assets named by SOFT
# PATH from C++ (Spaceport.cpp and the rocket pack), because a soft path is not a package
# reference. L_uFoods is a LEVEL and hard-references its 1,956 meshes, so the cooker
# follows them from the map. One +MapsToCook line is the whole story - which is why
# RestaurantScene has no DirectoriesToAlwaysCook entry and the deli ships correctly.

import json
import traceback

import unreal

SRC = "/Game/Poly_Supermarket_01/Levels/L_Showcase"
DST = "/Game/Maps/L_uFoods"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/make_ufoods_from_showcase.json"

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level
    exists = unreal.EditorAssetLibrary.does_asset_exist(DST)

    # ================================================== RUN 2: L_uFoods is open
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
        # The cafe run spawned its PlayerStart at the world origin and told Walt to move
        # it, because "the origin of a dressed interior is probably inside a wall". True
        # then and truer here: this room is 17.6 x 20 m and its origin is not even in it.
        #
        # So work out where the room actually IS, from the room itself - the horizontal
        # middle of the pack geometry, standing on its lowest surface. In a supermarket
        # that is an aisle: open floor, by construction, because aisles are the gaps.
        starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
        if starts:
            p = starts[0].get_actor_location()
            r["player_start"] = "already here"
            r["start_at"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
        else:
            xs, ys, zs = [], [], []
            for a in actors:
                for c in a.get_components_by_class(unreal.StaticMeshComponent):
                    m = c.get_editor_property("static_mesh")
                    if not m or "/Poly_Supermarket_01/" not in m.get_path_name():
                        continue
                    t = c.get_world_transform().translation
                    xs.append(t.x); ys.append(t.y); zs.append(t.z)

            if xs:
                cx = (min(xs) + max(xs)) / 2.0
                cy = (min(ys) + max(ys)) / 2.0
                # +100 clears the floor. A PlayerStart's location is its capsule CENTRE,
                # not its feet - the mistake that left Nyra hanging a metre over the
                # pavement outside the deli on 3 Sep.
                cz = min(zs) + 100.0
                r["room_extent_m"] = [round((max(xs) - min(xs)) / 100.0, 1),
                                      round((max(ys) - min(ys)) / 100.0, 1),
                                      round((max(zs) - min(zs)) / 100.0, 1)]
            else:
                cx, cy, cz = 0.0, 0.0, 120.0
                r["WARNING"] = ("No Poly_Supermarket_01 meshes found - falling back to "
                                "the origin. Is this really the duplicated showcase?")

            s = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(cx, cy, cz))
            s.set_actor_label("uFoods_PlayerStart")
            r["player_start"] = "spawned at the middle of the room"
            r["start_at"] = [round(cx, 1), round(cy, 1), round(cz, 1)]
            r["CHECK_IT"] = ("This is the centre of the geometry, which should be an "
                             "aisle - but it is a GUESS. Play it; if you start inside a "
                             "shelf or a fridge, drag the PlayerStart to the shop door "
                             "and re-save. It wants to be at the entrance anyway, so "
                             "arriving through the uFoods door reads as walking in.")

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Play L_uFoods. You should be Leonard, standing in a supermarket, "
                     "with the HUD. Then tell me and I will wire the uFoods door in "
                     "L_City to it with an ArrivalTag, the same way the deli door works.")

        # Hand the world back before this script's globals outlive it - every UObject a
        # Python name still holds is one the editor cannot collect at the next map change.
        del world, settings, actors, starts
        unreal.SystemLibrary.collect_garbage()

    # ============================================ RUN 1: copy, then stop
    elif not exists:
        r["run"] = 1
        if not unreal.EditorAssetLibrary.does_asset_exist(SRC):
            raise Exception("no %s - is the Poly Supermarket pack added to the project?"
                            % SRC)

        ok = unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
        r["duplicate"] = "created" if ok else "FAILED"
        if not ok:
            raise Exception("duplicate_asset %s -> %s failed" % (SRC, DST))

        # The baked lighting rides in its own package. Say plainly whether it came: a
        # showcase interior without its BuiltData is a flat, wrong-looking room. For this
        # pack the BuiltData is most of the weight - 57.6 MB of the 72 MB pack.
        r["built_data_copied"] = unreal.EditorAssetLibrary.does_asset_exist(DST + "_BuiltData")
        r["next"] = ("NOW OPEN L_uFoods BY HAND: File > Open Level > Maps > L_uFoods. "
                     "Then run this again. Do NOT let a script change the level.")
        unreal.SystemLibrary.collect_garbage()

    # ============================================ exists but not open
    else:
        r["run"] = "waiting"
        r["next"] = ("L_uFoods already exists but is not the open level. Open it by hand "
                     "and run this again. To rebuild it from scratch, delete it in the "
                     "Content Browser first - Python must not delete or load levels.")

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###UFOODS### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
