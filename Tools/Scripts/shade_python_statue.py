# shade_python_statue.py - an invisible shadow over the one statue that stands in the sun.
#
# Walt, 2026-10-19, in L_Archaic_Temple, of Python's Laocoon: "python sign is still barely
# readable. can the sunlight be blocked on the opposite window?" The other four statues stand in
# shade; Python's, on the -Y aisle, takes the sun through the opening across the nave, and its
# bronze tablet washes out however the material is set (tune_statue_tablet.py).
#
# So: a panel in the sun's path that is invisible but casts a shadow (bCastHiddenShadow) - no
# change to the pack's lighting, nothing anyone can see. Placed along the line from Python's tablet
# toward the sun, where that line crosses the far side of the nave, square to the light, a little
# larger than the statue and its tablet so the whole group falls in shade. No collision.
#
# One transaction: a single Ctrl+Z removes it. Does NOT save - Walt looks, then Ctrl+S.
#
# *** RUN WITH L_Archaic_Temple OPEN, PIE stopped ***

import math

import unreal

TAG = "###SHADE###"
MAP = "/Game/Maps/L_Archaic_Temple"
LABEL = "SunShade_Python"
STATUE = "Statue_PYTHON"
CROSS_Y = 800.0          # the far side of the nave from Python (it stands at y -540, facing +Y)
SIZE = (450.0, 500.0)    # cm across and up, square to the light: the Laocoon is 180 x 242, tablet in front

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def say(msg):
    print("%s %s" % (TAG, msg))


def main():
    if not world.get_path_name().startswith(MAP + "."):
        say("open %s first - this is %s; nothing done" % (MAP, world.get_path_name()))
        return
    actors = eas.get_all_level_actors()
    suns = [a for a in actors if a.get_class().get_name() == "DirectionalLight"]
    statue = next((a for a in actors if a.get_actor_label() == STATUE), None)
    if len(suns) != 1 or not statue:
        say("need exactly one DirectionalLight (found %d) and %s; nothing done" % (len(suns), STATUE))
        return
    light = suns[0].get_actor_rotation().get_forward_vector()       # the way the light travels
    to_sun = light * -1.0
    say("sun: light travels (%.2f, %.2f, %.2f), %.0f degrees above the horizon" % (
        light.x, light.y, light.z, math.degrees(math.asin(max(-1.0, min(1.0, to_sun.z))))))

    tablet = statue.get_editor_property("tablet")
    o, e = statue.get_actor_bounds(True)
    target = unreal.Vector(o.x, o.y + 0.5 * e.y, o.z)   # the group's middle, toward its tablet
    if to_sun.y <= 0.05:
        say("the sun does not come from the far side of the nave (to-sun y %.2f) - nothing placed" % to_sun.y)
        return
    t = (CROSS_Y - target.y) / to_sun.y
    at = target + to_sun * t
    say("from the Laocoon's middle (%.0f, %.0f, %.0f) toward the sun, the line crosses y %.0f at (%.0f, %.0f, %.0f), %.0f cm away" % (
        target.x, target.y, target.z, CROSS_Y, at.x, at.y, at.z, t))

    # Is the sun really reaching the tablet? Trace from the tablet toward the sun.
    tw = tablet.get_world_location()
    hit = unreal.SystemLibrary.line_trace_single(world, tw, tw + to_sun * 20000.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                 False, [statue], unreal.DrawDebugTrace.NONE, True)
    first = hit.to_tuple() if hit else None
    say("from the tablet toward the sun, the first thing met: %s" % (
        "%s at %.0f cm (glass lets light through)" % (first[9].get_actor_label() if first[9] else "?", first[3])
        if first and first[0] else "open sky - it is in the sun"))

    rot = unreal.MathLibrary.make_rot_from_x(light)    # the panel's thin X along the light: square to it
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    with unreal.ScopedEditorTransaction("Invisible sun shade over Python's statue"):
        for a in actors:
            if a.get_actor_label() == LABEL:
                eas.destroy_actor(a)
        shade = eas.spawn_actor_from_object(cube, at, rot)
        shade.set_actor_label(LABEL)
        shade.set_actor_scale3d(unreal.Vector(0.05, SIZE[0] / 100.0, SIZE[1] / 100.0))
        c = shade.get_component_by_class(unreal.StaticMeshComponent)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property("cast_shadow", True)
        c.set_editor_property("cast_hidden_shadow", True)    # unseen, but its shadow falls
        c.set_editor_property("affect_distance_field_lighting", False)
        c.set_visibility(False)
    say("%s placed at (%.0f, %.0f, %.0f), %.0f x %.0f cm, invisible, casting shadow. One Ctrl+Z removes it. Not saved." % (
        LABEL, at.x, at.y, at.z, SIZE[0], SIZE[1]))


main()
