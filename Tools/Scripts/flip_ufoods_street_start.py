# flip_ufoods_street_start.py - turn Leonard round when he leaves uFoods.
#
# *** OPEN /Game/Maps/L_City FIRST, THEN RUN THIS ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/flip_ufoods_street_start.py"
#
# Refuses unless L_City is already open, and never changes levels itself.
#
# ===========================================================================
# THE BUG THIS FIXES.
#
# Walt bought his supplies, came out of uFoods, and Nyra was not there. The log said she
# was: "[Dancer] Nyra is guiding at stage 3, moved to her marker". Both were true - she
# had been moved INSIDE THE SHOPFRONT.
#
# place_ufoods_doors.py built the uFoodsStreet PlayerStart facing TOWARD the shop, so the
# player would arrive looking at the door he came through. Stage 3 then stands her along
# that marker's FORWARD, which is the direction of the building:
#
#     marker sits 110 cm out from the door, pointing back at it
#     she stands 320 cm along that forward  ->  about 2 m PAST the door, in the wall
#
# Stage 1 works because DeliDoor faces AWAY from the deli - forward is out into the
# street, which is both where the player looks on arrival and where there is room for
# somebody to stand.
#
# So the marker was wrong for BOTH of its jobs, and one flip fixes both: he now steps out
# of a shop facing the street like a person leaving a shop, and the space in front of him
# is open pavement for his guide to be standing in.
#
# ONE MARKER, TWO JOBS - that is the whole trick of this arrangement (see the stage-1
# note in DancerAgentComponent.h). It also means a mistake in it shows up twice, which is
# how this one was found.

import json
import math
import traceback

import unreal

LEVEL = "/Game/Maps/L_City"
TAG = "uFoodsStreet"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/flip_ufoods_street_start.json"

# Must match UDancerAgentComponent::GuideStage3Distance, for the report only.
GUIDE_DISTANCE = 320.0

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level

    if open_level != LEVEL:
        r["refused"] = "Open %s by hand and run this again." % LEVEL
    else:
        marker = None
        for a in eas.get_all_level_actors():
            if isinstance(a, unreal.PlayerStart):
                try:
                    if str(a.get_editor_property("player_start_tag")) == TAG:
                        marker = a
                        break
                except Exception:
                    pass

        if not marker:
            raise Exception("no PlayerStart with player_start_tag '%s' in L_City - run "
                            "place_ufoods_doors.py with L_City open first." % TAG)

        rot = marker.get_actor_rotation()
        loc = marker.get_actor_location()
        r["was_yaw"] = round(rot.yaw, 1)
        r["marker_at"] = [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)]

        # Where she stands TODAY, so the before/after is a fact rather than a claim.
        def stand_at(yaw_deg):
            rad = math.radians(yaw_deg)
            return [round(loc.x + math.cos(rad) * GUIDE_DISTANCE, 1),
                    round(loc.y + math.sin(rad) * GUIDE_DISTANCE, 1)]

        r["she_stood_at"] = stand_at(rot.yaw)

        new_yaw = rot.yaw + 180.0
        while new_yaw > 180.0:
            new_yaw -= 360.0
        # unreal.Rotator is (ROLL, PITCH, YAW) - not C++'s (Pitch, Yaw, Roll).
        marker.set_actor_rotation(unreal.Rotator(0.0, 0.0, new_yaw), False)

        r["now_yaw"] = round(new_yaw, 1)
        r["she_will_stand_at"] = stand_at(new_yaw)
        # Read it back rather than trust the setter.
        r["read_back_yaw"] = round(marker.get_actor_rotation().yaw, 1)

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Play, buy supplies if this save has not, and leave uFoods. You "
                     "should step out facing the street with Nyra in front of you. If she "
                     "is now in the road instead, drag the marker - hand placement wins "
                     "and re-running this would only flip it back.")

    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###FLIPSTART### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
