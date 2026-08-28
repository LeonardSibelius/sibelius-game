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

# HOW HIGH THE DOOR SITS. Walt, looking at the first one: "can I nudge it down a bit?"
#
# The door took its Z from the SLOT CABINET, which stands on a plinth at Z=100 - so the
# door floated a metre off the floor. It now takes its Z from a door already in the
# cathedral, which is by definition standing on the floor correctly, and this is the
# hand-tuning knob on top of that. Negative is down.
#
# Editing this and re-running is the reproducible way to move it. Dragging it with the
# gizmo works too and looks identical, right up until somebody re-runs the script.
DOOR_Z_NUDGE = 0.0

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


    # ------------------------------------------------------------------
    # A DOOR WITH NO MESH IS NOT A DOOR.
    #
    # ACathedralDoor creates its DoorMesh component in C++ but assigns no asset - every
    # door already in a level got its mesh by hand in the editor. So the one this script
    # spawned revealed itself correctly on the toll (the log even said so:
    # "[CathedralDoor] the toll is paid - the way to the meadow is open") and was an
    # invisible, non-collidable nothing. Walt: "i see no door, even when i press V."
    #
    # Copied from a door already in the level rather than hard-coded, so it matches
    # whatever the cathedral actually uses and keeps matching if that is ever changed.
    def door_mesh_comp(a):
        # DoorMesh is not exposed to Python under that name - ask for the component by
        # class instead, which does not depend on how a UPROPERTY happens to be named.
        return a.get_component_by_class(unreal.StaticMeshComponent)

    def give_it_a_body(new_door, all_actors):
        mine = door_mesh_comp(new_door)
        if not mine:
            return {"worked": False, "why": "the spawned door has no StaticMeshComponent"}

        donor_mesh, src = None, None
        for a in all_actors:
            if a is new_door or a.get_class() != new_door.get_class():
                continue
            c = door_mesh_comp(a)
            got = c.get_editor_property("static_mesh") if c else None
            if got:
                donor_mesh, src = got, a.get_actor_label()
                new_door.set_actor_scale3d(a.get_actor_scale3d())
                break

        if not donor_mesh:
            donor_mesh = unreal.EditorAssetLibrary.load_asset(
                "/Game/UltimateGothicCathedralChurch/Mesh/SM_Door_Cathedral_Huge_00001__6336")
            src = "the gothic pack (no donor door in this level)"

        if donor_mesh:
            mine.set_editor_property("static_mesh", donor_mesh)

        back = mine.get_editor_property("static_mesh")
        return {"mesh": back.get_name() if back else None,
                "copied_from": src,
                "worked": back is not None}

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

        # FLOOR HEIGHT COMES FROM A DOOR, NOT THE CABINET. The cabinet is on a plinth at
        # Z=100, so taking its Z left this one hanging a metre in the air.
        floor_z = None
        for a in actors:
            if a.get_class() == door_cls and a.get_actor_label() != "Door_ToMeadow":
                floor_z = a.get_actor_location().z
                break
        loc.z = (floor_z if floor_z is not None else 0.0) + DOOR_Z_NUDGE
        r["floor_z_from"] = "an existing door" if floor_z is not None else "level zero"
        r["door_z"] = round(loc.z, 1)
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

        r["body"] = give_it_a_body(door, actors)

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

        r["body"] = give_it_a_body(door, actors)

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
