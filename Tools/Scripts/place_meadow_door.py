# place_meadow_door.py — the way to the battle, and the way back.
#
# *** RUN TWICE, ONCE PER LEVEL, FROM THE OPEN EDITOR ***
#   open L_Cathedral, run it   -> places the toll door beside the slot machine
#   open L_Meadow,    run it   -> places the return door beside the PlayerStart
#
# It looks at whichever level is loaded and does the right half. Safe to re-run.
#
# ===========================================================================
# WHAT THIS FINISHES.
#
# Everything for the battle already exists and nothing connects it to the game: the toll
# is metered on the cathedral machine, Mrs. Hall has her line, the Journal counts down
# the credits still owed, the meadow has navmesh and an army - and the only way in is a
# console command. This is the door.
#
# THE TOLL DOOR IS INVISIBLE UNTIL IT IS PAID, not locked. bRequireBattleToll hides it
# and polls FProgressionState::IsBattleQualified twice a second, the same shape
# bRequireGenerateUse already uses for the Generate gate. A locked door tells the player
# there is somewhere they are not allowed; a wall that becomes a door tells them the
# machine did something. Mrs. Hall's Battle.Open line fires on the same crossing, so she
# names the Architects in the same breath the way opens.
#
# BESIDE THE MACHINE, deliberately. The toll is paid at the cathedral machine and the way
# opens at the cathedral machine - the player should not have to be told where to look.
# So the script finds ASlotCabinet and puts the door next to it rather than at a
# hand-typed coordinate that would drift the first time the room is redressed.

import json
import traceback

import unreal

MEADOW = "/Game/Cinematics/L_Meadow"
CATHEDRAL = "/Game/Maps/L_Cathedral"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/place_meadow_door.json"

r = {}

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    world = ues.get_editor_world()
    level_path = world.get_path_name().split(".")[0]
    r["level"] = level_path

    door_cls = unreal.load_class(None, "/Script/SibeliusGame.CathedralDoor")
    if not door_cls:
        raise Exception("CathedralDoor class not found - is the editor on a current build?")

    actors = eas.get_all_level_actors()

    def door_labelled(label):
        for a in actors:
            if a.get_actor_label() == label:
                return a
        return None

    # ------------------------------------------------ the cathedral half
    if level_path.endswith("L_Cathedral"):
        cab_cls = unreal.load_class(None, "/Script/SibeliusGame.SlotCabinet")
        cab = next((a for a in actors if cab_cls and a.get_class() == cab_cls), None)
        if not cab:
            raise Exception("no ASlotCabinet in L_Cathedral - the toll door belongs beside it")

        # Three metres to the machine's left, facing the way the machine faces, so the
        # player standing at the reels only has to turn their head.
        fwd = cab.get_actor_forward_vector()
        right = cab.get_actor_right_vector()
        loc = cab.get_actor_location() + right * -300.0 + fwd * 100.0
        r["slot_cabinet"] = [round(cab.get_actor_location().x, 1),
                             round(cab.get_actor_location().y, 1),
                             round(cab.get_actor_location().z, 1)]

        door = door_labelled("Door_ToMeadow")
        if not door:
            door = eas.spawn_actor_from_class(door_cls, loc, cab.get_actor_rotation())
            door.set_actor_label("Door_ToMeadow")
            r["cathedral_door"] = "spawned"
        else:
            door.set_actor_location(loc, False, False)
            r["cathedral_door"] = "moved"

        door.set_editor_property("TargetLevelName", "L_Meadow")
        door.set_editor_property("bRequireBattleToll", True)
        door.set_editor_property("bInteractive", True)
        door.set_editor_property("PromptText",
            unreal.Text("The Architects are waiting [E]"))

        # Read it back. Setters that report success and do nothing cost this project a
        # whole evening on 2026-08-27.
        r["readback"] = {
            "target": str(door.get_editor_property("TargetLevelName")),
            "toll_gated": bool(door.get_editor_property("bRequireBattleToll")),
            "location": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
        }

    # ------------------------------------------------ the meadow half
    elif level_path.endswith("L_Meadow"):
        start = next((a for a in actors if isinstance(a, unreal.PlayerStart)), None)
        if not start:
            raise Exception("no PlayerStart in L_Meadow")

        # Behind where the player arrives, so it is not the first thing they walk into
        # and not something they have to hunt for when they have had enough.
        loc = start.get_actor_location() + unreal.Vector(-400.0, 0.0, 0.0)

        door = door_labelled("Door_MeadowReturn")
        if not door:
            door = eas.spawn_actor_from_class(door_cls, loc)
            door.set_actor_label("Door_MeadowReturn")
            r["meadow_door"] = "spawned"
        else:
            door.set_actor_location(loc, False, False)
            r["meadow_door"] = "moved"

        door.set_editor_property("TargetLevelName", "L_Cathedral")
        door.set_editor_property("bRequireBattleToll", False)
        door.set_editor_property("bInteractive", True)
        door.set_editor_property("PromptText",
            unreal.Text("Back to the cathedral [E]"))

        r["readback"] = {
            "target": str(door.get_editor_property("TargetLevelName")),
            "location": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
        }

        # ---- and the thing that makes it a battle instead of a field --------
        #
        # Walt, having played to the meadow: "why must I press the 5 and 4?" He should
        # not - those are debug keys, made so the fight could be iterated on without
        # playing the whole game to reach it. ABattleArrival runs the whole beat on
        # BeginPlay: the army appears and is LOOKED at, the agents speak, the body is
        # granted, and only then do they come. See BattleArrival.h for why the order is
        # slow on purpose.
        arr_cls = unreal.load_class(None, "/Script/SibeliusGame.BattleArrival")
        if not arr_cls:
            r["arrival"] = "BattleArrival class not found - editor on an old build?"
        else:
            arr = door_labelled("TheArrival")
            if not arr:
                arr = eas.spawn_actor_from_class(arr_cls, start.get_actor_location())
                arr.set_actor_label("TheArrival")
                r["arrival"] = "spawned"
            else:
                r["arrival"] = "already here"
            r["arrival_readback"] = {
                "army": int(arr.get_editor_property("ArmyCount")),
                "metres": float(arr.get_editor_property("ArmyDistanceMetres")),
                "arc": float(arr.get_editor_property("ArmyArcDegrees")),
                "ranks": int(arr.get_editor_property("ArmyRanks")),
            }
    else:
        raise Exception("open L_Cathedral or L_Meadow first - this level is %s" % level_path)

    les.save_current_level()
    r["saved"] = True

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###DOOR### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
