# place_home_portal.py — the portals in Mrs. Hall's house to the agents' virtual homes.
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
# RUN with L_Office_v02 open, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_home_portal.py"
# Never loads a level (that crashes the editor from a script - open L_Office_v02 by hand).

import unreal

TAG = "###HOMEPORTAL###"
PORTALS = [
    {"label": "KaiaPortal", "anchor": "BP_Door_Outside_A3", "side_of": "player",
     "destination": "L_KaiaHome", "arrival": "KaiaHome", "clear": 30.0},
    {"label": "ElisePortal", "anchor": "SM_Window_Floor2_A3_402", "side_of": "Elise",
     "destination": "L_EliseHome", "arrival": "EliseHome", "clear": 90.0},
]
PORTAL_HALF_HEIGHT = 110.0   # its centre this far above the floor - the hall door's centre height

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def aim(portal, spec):
    portal.set_editor_property("travel_target_level", spec["destination"])
    portal.set_editor_property("arrival_tag", spec["arrival"])


def floor_below(world, where, ignore):
    hit = unreal.SystemLibrary.line_trace_single(
        world, where, unreal.Vector(where.x, where.y, where.z - 400.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False, ignore, unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def place(spec, actors, world):
    anchor = next((a for a in actors if a.get_actor_label() == spec["anchor"]), None)
    if spec["side_of"] == "player":
        side = next((a for a in actors if a.get_class().get_name() == "PlayerStart"
                     and not a.get_editor_property("player_start_tag")), None) \
            or next((a for a in actors if a.get_class().get_name() == "PlayerStart"), None)
    else:
        side = next((a for a in actors if spec["side_of"].lower() in a.get_actor_label().lower()
                     and a.get_class().get_name() != "CompilePortal"), None)
    if not anchor or not side:
        say("%s: need %s and %s to stand beside - open L_Office_v02." % (spec["label"], spec["anchor"], spec["side_of"]))
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
    if "L_Office_v02" not in world.get_path_name():
        say("REFUSING: the open map is %s, not L_Office_v02. Nothing changed." % world.get_path_name())
        return
    was_dirty = any("L_Office_v02" in str(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    actors = eas.get_all_level_actors()
    changed = False
    for spec in PORTALS:
        standing = [a for a in actors if a.get_actor_label() == spec["label"]]
        if standing:
            for portal in standing:
                aim(portal, spec)
            say("%s already stands - aimed at %s (tag %s), left where it is." % (spec["label"], spec["destination"], spec["arrival"]))
        else:
            changed = place(spec, actors, world) or changed
    if was_dirty:
        say("NOT SAVED: the office already had unsaved changes before this ran - look them over and save it yourself (Ctrl+S).")
    else:
        les.save_current_level()
        say("saved L_Office_v02.")


main()
