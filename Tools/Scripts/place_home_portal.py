# place_home_portal.py — the portals to the agents' virtual homes: in the house (the hall, Elise's
# bedroom, the attic) and the AI temple.
#
# Kaia's (2026-10-02, as place_kaia_portal.py): "when the player starts the game he is looking
# down the hall at BP_Door_Outside_A3. I want one of the portals from my portal pack right there
# with a "C" prompt to go thru it, just like at the spaceport. Might as well make him go upstairs
# and earn Compile from Elise before he can visit Kaia's home." Elise's (2026-10-05): "put her
# portal in her bedroom in front of SM_Window_Floor2_A3_402". See CompilePortal.h: [C] goes
# through once Compile is earned - which Elise grants in that same bedroom.
#
# A PORTAL ALREADY STANDING IS ONLY RE-AIMED, never moved: the first one went in before its home
# existed, and re-measuring would undo anything moved by hand since. A new one is measured from
# its anchor rather than trusted to numbers: it stands on the ROOM side of the anchor (the side
# its agent stands on - for the hall door, the side the player starts on), on the floor, clear
# of the anchor's face by CLEAR - for a window, past its curtains.
#
# SAVES ONLY A CLEAN MAP. If the office already had unsaved changes when this ran, the portal is
# placed but the map is left for Walt to save, so nothing of his is saved without him seeing it.
#
# RUN with the level its portal stands in open (L_Office_v02, L_AI_Temple), PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_home_portal.py"
# Never loads a level (that crashes the editor from a script - open it by hand).

import math
import unreal

TAG = "###HOMEPORTAL###"
PORTALS = [
    {"label": "KaiaPortal", "map": "L_Office_v02", "anchor": "BP_Door_Outside_A3", "side_of": "player",
     "destination": "L_KaiaHome", "arrival": "KaiaHome", "clear": 30.0},
    {"label": "ElisePortal", "map": "L_Office_v02", "anchor": "SM_Window_Floor2_A3_402", "side_of": "Elise",
     "destination": "L_EliseHome", "arrival": "EliseHome", "clear": 90.0},
    # Aisling's (2026-10-05) in the AI temple, "where she is already dancing anyway": no object to
    # stand it in front of, so it stands BEHIND her - she dances in front of her own portal.
    {"label": "AislingPortal", "map": "L_AI_Temple", "behind": "Aisling",
     "destination": "L_AislingHome", "arrival": "AislingHome", "clear": 300.0},
    # Isla's (2026-10-05): "Isla's portal can be in the attic". Among the attic's storage, by the
    # old clock (she is the one who asks what deserves to last), on the one stretch of floor
    # measured clear of boxes all round with 4 m under the sloping roof, 2.5 m from the hatch,
    # the cathedral door and the ladder - and facing the hatch, so he sees it as he climbs in.
    # Re-aimed 2026-10-24 from her station (L_IslaHome) to Fafnir's Keep (make_virtual_home.py).
    # And the way BACK lands here too: [O] in her keep travels to the tag "AtticFromIsla"
    # (ASibeliusGameCharacter::ReturnToOffice) - Walt: "It is a pain in the neck to have to gather
    # those books to get up the ladder" - so a PlayerStart wearing it stands just in front of the
    # portal, between it and the hatch, facing the hatch - 130 cm to its left, since Dee's
    # Freakbook binder lies straight ahead and chairs, boxes and the clock fill the right.
    {"label": "IslaPortal", "map": "L_Office_v02", "at": (-2040.0, 9385.0, 580.0), "face": "HatchLock",
     "destination": "L_IslaKeep", "arrival": "IslaKeep", "return_tag": "AtticFromIsla", "return_side": 130.0},
]
PORTAL_HALF_HEIGHT = 110.0   # its centre this far above the floor - the hall door's centre height
RETURN_AHEAD = 110.0         # cm from the portal to the way-back start, toward what it faces
CAPSULE_HALF_HEIGHT = 92.0   # a PlayerStart stands its capsule on the floor (make_virtual_home.py)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def aim(portal, spec):
    portal.modify()   # or a re-aim of a standing portal may never reach the saved map
    portal.set_editor_property("travel_target_level", spec["destination"])
    portal.set_editor_property("arrival_tag", spec["arrival"])


def floor_below(world, where, ignore):
    hit = unreal.SystemLibrary.line_trace_single(
        world, where, unreal.Vector(where.x, where.y, where.z - 400.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, ignore, unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def place_behind(spec, actors, world):
    """CLEAR behind the agent, opposite the way she faces, facing the same way she does."""
    agent = next((a for a in actors if spec["behind"].lower() in a.get_actor_label().lower()
                  and a.get_class().get_name() != "CompilePortal"), None)
    if not agent:
        say("%s: no %s in this level to stand behind." % (spec["label"], spec["behind"]))
        return False
    at = agent.get_actor_location()
    facing = agent.get_actor_rotation()
    forward = facing.get_forward_vector()
    where = unreal.Vector(at.x - forward.x * spec["clear"], at.y - forward.y * spec["clear"], at.z + 100.0)
    floor = floor_below(world, where, [agent])
    if floor is not None:
        where.z = floor + PORTAL_HALF_HEIGHT
    # A yaw-0 portal faces along Y (the window's); turn it to face along her forward.
    yaw = facing.yaw - 90.0
    portal = eas.spawn_actor_from_class(unreal.CompilePortal, where, unreal.Rotator(0.0, 0.0, yaw))
    portal.set_actor_label(spec["label"])
    aim(portal, spec)
    say("%s placed %.0f cm behind %s (she faces yaw %.0f) at %s, floor %s, to %s (tag %s)." % (
        spec["label"], spec["clear"], agent.get_actor_label(), facing.yaw, portal.get_actor_location(),
        "%.0f" % floor if floor is not None else "NOT FOUND", spec["destination"], spec["arrival"]))
    return True


def place_at(spec, actors, world):
    """On a measured spot, turned to face an actor (yaw 0 faces along Y, as in place_behind)."""
    x, y, floor_guess = spec["at"]
    where = unreal.Vector(x, y, floor_guess + 100.0)
    floor = floor_below(world, where, [])
    where.z = (floor if floor is not None else floor_guess) + PORTAL_HALF_HEIGHT
    target = next((a for a in actors if a.get_actor_label() == spec["face"]), None)
    yaw = 0.0
    if target:
        t = target.get_actor_location()
        yaw = math.degrees(math.atan2(t.y - y, t.x - x)) - 90.0
    portal = eas.spawn_actor_from_class(unreal.CompilePortal, where, unreal.Rotator(0.0, 0.0, yaw))
    portal.set_actor_label(spec["label"])
    aim(portal, spec)
    say("%s placed at %s, yaw %.0f (facing %s), floor %s, to %s (tag %s)." % (
        spec["label"], portal.get_actor_location(), yaw, spec["face"] if target else "nothing - NO " + spec["face"],
        "%.0f" % floor if floor is not None else "NOT FOUND", spec["destination"], spec["arrival"]))
    return True


def place_return(spec, actors, world):
    """The way back: a PlayerStart tagged spec["return_tag"] in front of the portal, facing what
    the portal faces, kept clear of that thing's own footprint (the hatch's opening). One that
    already stands is left where it is, like a standing portal."""
    label = "PlayerStart_" + spec["return_tag"]
    if any(a.get_actor_label() == label for a in actors):
        say("%s already stands - left where it is." % label)
        return False
    portal = next((a for a in actors if a.get_actor_label() == spec["label"]), None)
    target = next((a for a in actors if a.get_actor_label() == spec.get("face")), None)
    if not portal or not target:
        say("%s: need %s and %s to place it." % (label, spec["label"], spec.get("face")))
        return False
    p = portal.get_actor_location()
    t, ext = target.get_actor_bounds(False)
    dx, dy = t.x - p.x, t.y - p.y
    dist = math.hypot(dx, dy)
    keep_off = max(ext.x, ext.y) + 60.0   # the opening's half-width, and a capsule's radius and some
    ahead = max(60.0, min(RETURN_AHEAD, dist - keep_off))
    side = spec.get("return_side", 0.0)   # to the left of the line from the portal to what it faces
    where = unreal.Vector(p.x + (dx * ahead - dy * side) / dist, p.y + (dy * ahead + dx * side) / dist, p.z)
    hit = unreal.SystemLibrary.line_trace_single(
        world, where, unreal.Vector(where.x, where.y, where.z - 400.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, [portal], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        say("%s: no floor under %s - not placed." % (label, where))
        return False
    floor = hit.to_tuple()[4].z
    on = hit.to_tuple()[9]
    yaw = math.degrees(math.atan2(dy, dx))
    start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(where.x, where.y, floor + CAPSULE_HALF_HEIGHT),
                                       unreal.Rotator(0.0, 0.0, yaw))
    start.set_actor_label(label)
    start.set_editor_property("player_start_tag", spec["return_tag"])
    say("%s placed %.0f cm in front of %s at %s, on %s (floor %.0f), facing %s (yaw %.0f), %.0f cm from its centre." % (
        label, ahead, spec["label"], start.get_actor_location(), on.get_actor_label() if on else "?", floor,
        spec["face"], yaw, dist - ahead))
    return True


def place(spec, actors, world):
    if "behind" in spec:
        return place_behind(spec, actors, world)
    if "at" in spec:
        return place_at(spec, actors, world)
    anchor = next((a for a in actors if a.get_actor_label() == spec["anchor"]), None)
    if spec["side_of"] == "player":
        side = next((a for a in actors if a.get_class().get_name() == "PlayerStart"
                     and not a.get_editor_property("player_start_tag")), None) \
            or next((a for a in actors if a.get_class().get_name() == "PlayerStart"), None)
    else:
        side = next((a for a in actors if spec["side_of"].lower() in a.get_actor_label().lower()
                     and a.get_class().get_name() != "CompilePortal"), None)
    if not anchor or not side:
        say("%s: need %s and %s to stand beside." % (spec["label"], spec["anchor"], spec["side_of"]))
        return False

    origin, ext = anchor.get_actor_bounds(False)
    # The anchor is thin along one horizontal axis: that axis is its facing.
    thin_is_y = ext.y < ext.x
    toward = side.get_actor_location() - origin
    if thin_is_y:
        sign = 1.0 if toward.y > 0 else -1.0
        where = unreal.Vector(origin.x, origin.y + sign * (ext.y + spec["clear"]), origin.z)
        yaw = 0.0
    else:
        sign = 1.0 if toward.x > 0 else -1.0
        where = unreal.Vector(origin.x + sign * (ext.x + spec["clear"]), origin.y, origin.z)
        yaw = 90.0
    floor = floor_below(world, where, [anchor])
    if floor is not None:
        where.z = floor + PORTAL_HALF_HEIGHT
    say("%s: %s bounds centre %s, size %.0f x %.0f x %.0f; standing on the %s side, floor %s" % (
        spec["label"], spec["anchor"], origin, 2 * ext.x, 2 * ext.y, 2 * ext.z, spec["side_of"],
        "%.0f" % floor if floor is not None else "NOT FOUND - left at the anchor's centre height"))

    portal = eas.spawn_actor_from_class(unreal.CompilePortal, where, unreal.Rotator(0.0, 0.0, yaw))
    portal.set_actor_label(spec["label"])
    aim(portal, spec)
    say("%s placed at %s, yaw %.0f, to %s (tag %s)." % (spec["label"], portal.get_actor_location(), yaw,
                                                     spec["destination"], spec["arrival"]))
    return True


def main():
    world = ues.get_editor_world()
    here = world.get_path_name()
    specs = [s for s in PORTALS if "/%s." % s["map"] in here]
    if not specs:
        say("REFUSING: the open map is %s - no portal belongs there. Nothing changed." % here)
        return
    map_name = specs[0]["map"]
    was_dirty = any(map_name in str(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    actors = eas.get_all_level_actors()
    changed = False
    for spec in specs:
        standing = [a for a in actors if a.get_actor_label() == spec["label"]]
        if standing:
            for portal in standing:
                aim(portal, spec)
            say("%s already stands - aimed at %s (tag %s), left where it is." % (spec["label"], spec["destination"], spec["arrival"]))
        else:
            changed = place(spec, actors, world) or changed
        if spec.get("return_tag"):
            changed = place_return(spec, eas.get_all_level_actors(), world) or changed
    if was_dirty:
        say("NOT SAVED: %s already had unsaved changes before this ran - look them over and save it yourself (Ctrl+S)." % map_name)
    else:
        les.save_current_level()
        say("saved %s." % map_name)


main()
