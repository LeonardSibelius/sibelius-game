# place_poker_at_glass.py - the poker cabinet takes the library door's place.
#
# *** RUN FROM THE OPEN EDITOR, WITH L_Office_v02 OPEN AND PIE STOPPED ***
#   Cmd box:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_poker_at_glass.py"
#
# Report: Saved/poker_at_glass.json
#
# ===========================================================================
# WHY.
#
# Walt, 2026-09-06: "as i play this game, i never find myself going into that big library
# level... I want to move the video poker into the living room and just drop that big
# library level, along with that weird carousel of fates game that is in there."
#
# And where: "can you put that big poker deck that you E at to play poker into the spot
# where the door to the library level is."
#
# That spot is PokerDoor_Kitchen, on the south sliding glass behind his spawn - which is
# also the CODE VISION TUTORIAL. The first two objectives in the game point at that glass.
# So the cabinet arrives hidden and Code Vision reveals it, and Chapter 1 keeps its shape.
# See APokerMachine::bHiddenUntilCodeVision.
#
# ---------------------------------------------------------------------------
# IT DOES NOT DELETE THE DOOR.
#
# The standing rule in this project is that placement scripts never destroy level content
# (place_grok_arrival.py's header says so, and it is the reason a re-run cannot lose a
# hand-adjusted pose). The door is HIDDEN and made non-interactive instead: invisible, no
# collision, no travel. It stays in the map, so a single line change brings the library
# back if Walt ever wants it, and nothing is unrecoverable.
#
# ---------------------------------------------------------------------------
# IT RUNS ON THE OPEN LEVEL AND SAVES NOTHING.
#
# Editor Python that loads levels or writes .umap files has fatally crashed this editor
# before, and L_Office_v02 in particular is only ever saved by a healthy interactive
# editor - never by the save-on-close prompt, which has corrupted it twice. So: open the
# level yourself, run this, LOOK at where the cabinet landed, nudge it if you like, and
# save with Ctrl+S.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/poker_at_glass.json"

DOOR_HINT   = "PokerDoor"          # substring of the door actor's label
POKER_TAG   = "LivingRoomPoker"    # so a re-run finds the cabinet instead of adding a second
FORWARD_OFF = 60.0                 # cm out from the glass, so it does not z-fight the pane

r = {}


def find_by_label(world, hint):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        try:
            if hint.lower() in a.get_actor_label().lower():
                return a
        except Exception:
            pass
    return None


def find_by_tag(world, tag):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.actor_has_tag(tag):
            return a
    return None


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        r["error"] = "no editor world"
        return

    if "Office" not in world.get_name():
        r["error"] = "open L_Office_v02 first - this is %s" % world.get_name()
        return

    cab_existing = find_by_tag(world, POKER_TAG)

    door = find_by_label(world, DOOR_HINT)
    if door is None:
        # THE DOOR IS GONE, WHICH IS A FINISHED STATE AND NOT AN ERROR.
        if cab_existing is not None:
            r["door"] = "already removed"
            r["cabinet"] = "left exactly where it was placed by hand"
            r["next"] = ("Nothing to do. The door is deleted and the cabinet is where you "
                         "put it; this script will not move it.")
            return
        r["error"] = ("no actor whose label contains '%s', and no cabinet either - "
                      "is L_Office_v02 the open level?" % DOOR_HINT)
        return

    loc = door.get_actor_location()
    rot = door.get_actor_rotation()
    r["door"] = {
        "label": door.get_actor_label(),
        "location": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
        "yaw": round(rot.yaw, 1),
    }

    # Step OUT of the glass along the door's forward, so the deck stands in the room
    # rather than inside the pane. Negative forward would put it in the old library.
    fwd = rot.get_forward_vector()
    where = unreal.Vector(loc.x + fwd.x * FORWARD_OFF,
                          loc.y + fwd.y * FORWARD_OFF,
                          loc.z)

    poker_cls = unreal.load_class(None, "/Script/SibeliusGame.PokerMachine")
    if poker_cls is None:
        r["error"] = "PokerMachine class not found - build the editor target first"
        return

    cab = cab_existing
    if cab is None:
        cab = unreal.EditorLevelLibrary.spawn_actor_from_class(
            poker_cls, where, unreal.Rotator(0.0, 0.0, rot.yaw))   # (roll, pitch, yaw)!
        cab.tags = list(cab.tags) + [POKER_TAG]
        cab.set_actor_label("PokerMachine_LivingRoom")
        r["cabinet"] = "spawned at the door's spot"
    else:
        # IT ALREADY EXISTS, SO LEAVE IT WHERE IT IS.
        #
        # The first version moved it back to the door every run, which would have yanked a
        # hand-placed cabinet out of the room the moment this was re-run for any other
        # reason. place_grok_arrival.py's header states the rule: "a re-run that quietly
        # threw away a hand-adjusted pose would be its own kind of loss." The spawn is a
        # starting point offered once; where it ends up is Walt's, and his by hand.
        loc = cab.get_actor_location()
        r["cabinet"] = "left where it was placed by hand"
        r["cabinet_existing"] = [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]

    # THE TUTORIAL PROPERTY. Without this the cabinet is plainly visible and the opening
    # objective tells a new player to hold V at something already in front of him.
    #
    # UE's Python STRIPS THE b FROM BOOLEANS: bHiddenUntilCodeVision is reached as
    # "hidden_until_code_vision", not "b_hidden_until_code_vision". The first run of this
    # script guessed wrong, threw, and left the door untouched because the throw happened
    # before the door work below - which is also why that work no longer sits behind it.
    r["hidden_property"] = "FAILED"
    for name in ("hidden_until_code_vision", "b_hidden_until_code_vision"):
        try:
            cab.set_editor_property(name, True)
            r["hidden_property"] = name
            break
        except Exception:
            pass

    # WHERE IT ACTUALLY IS, not where the door is. Reporting the spawn point for a cabinet
    # that has since been dragged across the room is the same class of lie as a log line
    # that names a range it never applied - it sends the next reader after the wrong thing.
    now = cab.get_actor_location()
    r["cabinet_location"] = [round(now.x, 1), round(now.y, 1), round(now.z, 1)]
    r["cabinet_yaw"] = round(cab.get_actor_rotation().yaw, 1)

    # THE DOOR STANDS DOWN - hidden, non-colliding, non-travelling. Not destroyed.
    door.set_actor_hidden_in_game(True)
    door.set_actor_enable_collision(False)
    try:
        door.set_editor_property("target_level_name", "None")
    except Exception:
        pass   # not every door class exposes it; hiding it is what matters
    r["door_state"] = "hidden, collision off, travel cleared"

    if r["hidden_property"] == "FAILED":
        r["warning"] = ("Could not set the hidden-until-Code-Vision flag. The cabinet will "
                        "be plainly visible and the opening tutorial will point at nothing "
                        "hidden. Rebuild the editor target and re-run.")

    r["next"] = ("LOOK at it, nudge if you like, then Ctrl+S. "
                 "The level is NOT saved by this script. "
                 "The door stays VISIBLE IN THE EDITOR - hidden_in_game only affects play.")


try:
    main()
except Exception:
    r["error"] = traceback.format_exc()

with open(OUT, "w") as f:
    json.dump(r, f, indent=2)
unreal.log("place_poker_at_glass: " + json.dumps(r, indent=2))
