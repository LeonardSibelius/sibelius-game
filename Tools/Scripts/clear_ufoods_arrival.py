# clear_ufoods_arrival.py - stop the uFoods door eating the [E] meant for Nyra.
#
# *** OPEN /Game/Maps/L_City FIRST, THEN RUN THIS ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/clear_ufoods_arrival.py"
#
# Refuses unless L_City is already open, and never changes levels itself.
#
# ===========================================================================
# THE BUG, from the shipped 1.2.0 build.
#
# Walt: "when I exited uFoods, I could not talk to Nyra. E only took me back into
# uFoods... I didn't walk far enough away from uFoods. When I put Nyra between myself and
# uFoods, I can talk to her."
#
# She was standing there in plain sight the whole time. The door was winning the trace.
#
# UInteractorComponent::UpdateFocus does a SPHERE SWEEP from the camera. A sweep that
# STARTS INSIDE a collision volume reports a hit at distance zero - so once the player is
# inside the door's trigger box, the door is the focused actor no matter which way he is
# looking. Facing Nyra does not help; there is nothing in front of the hit.
#
# place_ufoods_doors.py put the door trigger 150 cm out from the storefront and the
# arrival marker only 110 cm beyond that. The trigger is a cube scaled 2.2 m tall. The
# player therefore materialises inside it, presses E, and goes straight back in.
#
# WHY THE DELI NEVER SHOWED THIS: its numbers came from a coordinate Walt found by hand
# and then adjusted by hand until it felt right. This one came from arithmetic I wrote,
# and arithmetic does not playtest.
#
# THE FIX IS MEASURED, NOT GUESSED. This reads the door trigger's ACTUAL world bounds -
# whatever SM_Cube's real size turns out to be, times whatever scale it carries - and
# moves the arrival marker clear of it plus a margin. No constant here assumes a cube is
# 100 cm.

import json
import math
import traceback

import unreal

LEVEL = "/Game/Maps/L_City"
DOOR_TAG = "uFoodsDoor"
START_TAG = "uFoodsStreet"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/clear_ufoods_arrival.json"

# Clear of the trigger's own extent, plus room for the interact sphere's radius and a
# pace of ordinary standing-about. Generous on purpose: being a metre too far out costs
# nothing, being 10 cm too close costs the whole interaction.
MARGIN = 150.0

# Must match UDancerAgentComponent::GuideStage3Distance - report only.
GUIDE_DISTANCE = 320.0

r = {}


def find_by_tag(eas, tag):
    for a in eas.get_all_level_actors():
        try:
            if tag in [str(t) for t in a.get_editor_property("tags")]:
                return a
        except Exception:
            pass
    return None


try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level

    if open_level != LEVEL:
        r["refused"] = "Open %s by hand and run this again." % LEVEL
    else:
        door = find_by_tag(eas, DOOR_TAG)
        if not door:
            raise Exception("no actor tagged %s in L_City." % DOOR_TAG)

        marker = None
        for a in eas.get_all_level_actors():
            if isinstance(a, unreal.PlayerStart):
                try:
                    if str(a.get_editor_property("player_start_tag")) == START_TAG:
                        marker = a
                        break
                except Exception:
                    pass
        if not marker:
            raise Exception("no PlayerStart with player_start_tag '%s'." % START_TAG)

        dloc = door.get_actor_location()
        r["door_at"] = [round(dloc.x, 1), round(dloc.y, 1), round(dloc.z, 1)]

        # MEASURE the trigger instead of assuming SM_Cube is 100 cm.
        origin, extent = door.get_actor_bounds(only_colliding_components=False)
        r["door_extent"] = [round(extent.x, 1), round(extent.y, 1), round(extent.z, 1)]
        horiz = max(extent.x, extent.y)
        r["door_horizontal_reach"] = round(horiz, 1)

        mloc = marker.get_actor_location()
        was = math.hypot(mloc.x - dloc.x, mloc.y - dloc.y)
        r["marker_was_at"] = [round(mloc.x, 1), round(mloc.y, 1), round(mloc.z, 1)]
        r["marker_was_from_door_cm"] = round(was, 1)
        r["was_inside_trigger"] = bool(was < horiz)

        # Push out along the marker's own FORWARD, which flip_ufoods_street_start.py
        # already pointed away from the shop. Using the marker's facing rather than
        # recomputing a direction means the two cannot disagree.
        fwd = marker.get_actor_rotation().get_forward_vector()
        need = horiz + MARGIN
        push = need - was
        r["needed_from_door_cm"] = round(need, 1)

        if push <= 0.0:
            r["moved"] = "not needed - already clear"
            new = mloc
        else:
            new = unreal.Vector(mloc.x + fwd.x * push,
                                mloc.y + fwd.y * push,
                                mloc.z)
            marker.set_actor_location(new, False, False)
            r["moved"] = "pushed %.1f cm further from the door" % push

        r["marker_now_at"] = [round(new.x, 1), round(new.y, 1), round(new.z, 1)]
        r["marker_now_from_door_cm"] = round(
            math.hypot(new.x - dloc.x, new.y - dloc.y), 1)

        # And where she ends up, since she is measured off this marker.
        she = [round(new.x + fwd.x * GUIDE_DISTANCE, 1),
               round(new.y + fwd.y * GUIDE_DISTANCE, 1)]
        r["she_will_stand_at"] = she
        r["she_from_door_cm"] = round(math.hypot(she[0] - dloc.x, she[1] - dloc.y), 1)
        # She must stay inside UInteractorComponent::InteractRange (450) of the arrival
        # point, or the fix for one problem creates the other.
        r["she_from_player_cm"] = GUIDE_DISTANCE
        r["within_interact_range"] = bool(GUIDE_DISTANCE <= 450.0)

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Play, leave uFoods, and press E without walking first. You should "
                     "get Nyra, not the door. If she is now standing in the road, drag "
                     "her marker back a little - hand placement wins.")

    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###CLEARARRIVAL### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
