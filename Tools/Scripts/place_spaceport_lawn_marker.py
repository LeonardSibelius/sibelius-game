# place_spaceport_lawn_marker.py - where Nyra waits once the spaceport stands.
#
# *** OPEN /Game/Maps/L_City FIRST, THEN RUN THIS ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_spaceport_lawn_marker.py"
#
# Refuses unless L_City is already open, and never changes levels itself.
#
# ===========================================================================
# WHAT IT MAKES.  A PlayerStart tagged SpaceportLawn - the marker
# UDancerAgentComponent::GuideStage2StartTag looks for. She stands
# GuideStage2Distance (320 cm) along its FORWARD and faces back down it, exactly the way
# stage 1 works off the DeliDoor marker. One marker in the level defines the meeting; drag
# it and the meeting moves.
#
# WHERE, AND WHY IT IS NOT A NUMBER I INVENTED.
#
# The level already knows where the lawn is: AHintVolume is the trigger that says "This
# ground is clear. Press G and generate a spaceport", and Walt sized it (radius 2400) to
# cover the grass past the sidewalk. So the hint IS the lawn, and the marker is derived
# from it rather than guessed - if the lawn ever moves, re-running this follows it.
#
# WHICH WAY SHE FACES, which is the part worth getting right:
#
#   - He generates the spaceport standing INSIDE the hint volume, out on the grass.
#   - So she must end up at the lawn's edge FACING IN, or he turns round to a back.
#   - She stands along the marker's forward and faces BACK down it (yaw + 180).
#   - Therefore the marker's forward must point OUT of the lawn, toward the city.
#
# Get that backwards and she stands correctly and stares at the empty field.

import json
import math
import traceback

import unreal

CITY = "/Game/Maps/L_City"
TAG = "SpaceportLawn"   # must match UDancerAgentComponent::GuideStage2StartTag
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/place_spaceport_lawn_marker.json"

# How far in from the hint volume's rim to sit the marker, as a fraction of its radius.
# 0.95 keeps it at the edge nearest the street rather than out in the middle of the field.
EDGE_FRACTION = 0.95

# A PlayerStart's location is its capsule CENTRE, not its feet - and the component
# subtracts that half-height again when it places her, so this only has to be sane.
STAND_H = 100.0

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level

    if open_level != CITY:
        r["refused"] = "Open %s by hand and run this again." % CITY
    else:
        actors = eas.get_all_level_actors()

        # Already there? Leave it exactly alone - it may have been dragged into place.
        existing = None
        for a in actors:
            if isinstance(a, unreal.PlayerStart):
                try:
                    if str(a.get_editor_property("player_start_tag")) == TAG:
                        existing = a
                        break
                except Exception:
                    pass

        if existing:
            p = existing.get_actor_location()
            rot = existing.get_actor_rotation()
            r["marker"] = "ALREADY EXISTS - not moved"
            r["marker_at"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
            r["marker_yaw"] = round(rot.yaw, 1)
            r["note"] = ("Hand placement wins. To re-derive it from the hint volume, "
                         "delete it and run this again.")
        else:
            hint_cls = unreal.load_class(None, "/Script/SibeliusGame.HintVolume")
            if not hint_cls:
                raise Exception("no AHintVolume class - is the editor running our module?")

            # unreal.Class has NO is_child_of method (that was the first draft, and it
            # threw AttributeError). Exact class match covers the real case - AHintVolume
            # is not subclassed - and class_is_child_of on the Kismet math library is the
            # correct spelling if it ever is.
            def is_hint(a):
                c = a.get_class()
                if c == hint_cls:
                    return True
                try:
                    return bool(unreal.MathLibrary.class_is_child_of(c, hint_cls))
                except Exception:
                    return False

            hints = [a for a in actors if is_hint(a)]
            r["hint_volumes"] = len(hints)

            if not hints:
                raise Exception("no AHintVolume in L_City - it is what defines the lawn. "
                                "Place the marker by hand, tagged %s." % TAG)

            hint = hints[0]
            hp = hint.get_actor_location()
            radius = 900.0
            for prop in ("Radius", "radius"):
                try:
                    radius = float(hint.get_editor_property(prop))
                    break
                except Exception:
                    continue
            r["hint_at"] = [round(hp.x, 1), round(hp.y, 1), round(hp.z, 1)]
            r["hint_radius"] = round(radius, 1)

            # OUT of the lawn = toward the middle of the city, the same heuristic the
            # uFoods and deli doors use: the built city is at the origin, the field is not.
            dx, dy = -hp.x, -hp.y
            length = math.hypot(dx, dy)
            if length < 1.0:
                raise Exception("the hint volume is at the world origin; cannot tell "
                                "which way is out of the lawn. Place the marker by hand.")
            ux, uy = dx / length, dy / length

            mx = hp.x + ux * radius * EDGE_FRACTION
            my = hp.y + uy * radius * EDGE_FRACTION

            # FIND THE GROUND. DO NOT INHERIT THE HINT VOLUME'S Z.
            #
            # The first draft used hp.z + STAND_H, and hp.z is the CENTRE of a 2400-radius
            # trigger sphere (115.3), not a floor. Nyra ended up hovering a metre over the
            # pavement - the same mistake as reading a PlayerStart's capsule centre as its
            # feet, which cost a round trip earlier the same day.
            #
            # A thing's centre is not the ground under it. Trace for the ground.
            ground = None
            how = "not attempted"
            try:
                world = ues.get_editor_world()
                hit = unreal.SystemLibrary.line_trace_single(
                    world,
                    unreal.Vector(mx, my, hp.z + 2000.0),
                    unreal.Vector(mx, my, hp.z - 10000.0),
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,   # Visibility
                    False, [], unreal.DrawDebugTrace.NONE, True)
                # Depending on binding version this is a HitResult or (bool, HitResult).
                if isinstance(hit, tuple):
                    hit = hit[1] if len(hit) > 1 and hit[0] else None
                if hit:
                    ground = hit.impact_point.z
                    how = "traced"
            except Exception as e:
                how = "trace failed: %s" % e

            if ground is None:
                ground = hp.z
                how += " - FELL BACK to the hint volume's Z, which may be a volume centre"
            r["ground_z"] = round(ground, 1)
            r["ground_how"] = how

            spot = unreal.Vector(mx, my, ground + STAND_H)
            # Forward points further OUT of the lawn, so she steps that way and turns to
            # face back in at him.
            yaw = math.degrees(math.atan2(uy, ux))
            # unreal.Rotator is (ROLL, PITCH, YAW) - not C++'s (Pitch, Yaw, Roll).
            ps = eas.spawn_actor_from_class(unreal.PlayerStart, spot,
                                            unreal.Rotator(0.0, 0.0, yaw))
            ps.set_actor_label("SpaceportLawn_Marker")
            ps.set_editor_property("tags", [unreal.Name(TAG)])
            ps.set_editor_property("player_start_tag", unreal.Name(TAG))

            r["marker"] = "created"
            r["marker_at"] = [round(spot.x, 1), round(spot.y, 1), round(spot.z, 1)]
            r["marker_yaw"] = round(yaw, 1)
            # Read back rather than assume: a tag that silently failed to set is a guide
            # who never moves, and the only symptom is a warning in the log.
            r["marker_tag"] = str(ps.get_editor_property("player_start_tag"))
            r["she_will_stand_at"] = [round(spot.x + ux * 320.0, 1),
                                      round(spot.y + uy * 320.0, 1),
                                      round(spot.z, 1)]
            r["CHECK_IT"] = ("Derived from the hint volume, so it is only as right as the "
                             "hint volume is. If she ends up in the road or inside a "
                             "hedge, DRAG THE MARKER - re-running will not touch it.")

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Close the editor so the C++ can build, then generate a spaceport "
                     "and turn round.")

    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###LAWNMARKER### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
