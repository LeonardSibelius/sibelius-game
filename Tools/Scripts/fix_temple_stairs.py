# fix_temple_stairs.py - invisible ramps over the temple's two staircases, which cannot be climbed.
#
# Walt, 2026-10-19, in L_Archaic_Temple: "i can only get halfway up the stairs, is that by design?" -
# and then, with the east stairs fixed: "The other set of stairs won't let me go up." Not by design.
# Measured (numbers only - the level is a bought pack's): the temple is symmetric, its lower hall
# between two staircases - east beyond the statues, rising toward +X; west behind the entrance,
# rising toward -X - each climbing from the hall floor (40) to an upper floor (~510) in four flights
# with flat landings. The stair pieces' collision is not stepped: each flight is one smooth slope of
# about 50 degrees, past the character's walkable 44.8. Olympus was built to be looked at, not walked.
#
# The usual fix, and no change to how it looks: a thin slab with collision and no visibility, along
# the line through the landings' front edges - 31 degrees, walkable - so it lies on or above every
# flight, from where that line meets the hall floor up to the last landing. He walks up it as up the
# stairs. Placed only if the line clears every step; the character's step height and walkable angle
# are not touched: that would change every level.
#
# One transaction per run: a single Ctrl+Z removes what it placed. Does NOT save - Walt walks it,
# then Ctrl+S. Safe to re-run: it replaces its own ramps.
#
# *** RUN WITH L_Archaic_Temple OPEN, PIE stopped ***

import math

import unreal

TAG = "###STAIRS###"
MAP = "/Game/Maps/L_Archaic_Temple"
STEP_LIMIT = 45.0        # BP_FirstPersonCharacter's MaxStepHeight
MAX_SLOPE = 40.0         # degrees; walkable is 44.8
LANES = (-150.0, 0.0, 150.0)
# label, x at the foot (in the hall), x past the top; the stairs run along X, centred on y 0.
STAIRS = [
    ("StairsRamp_East", 3100.0, 4100.0),
    ("StairsRamp_West", -950.0, -1950.0),
]
OLD_LABELS = ("StairsRamp_TallRiser",)   # the east ramp's first name

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_editor_world()


def say(msg):
    print("%s %s" % (TAG, msg))


def floor(x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 800.0), unreal.Vector(x, y, -300.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None or not hit.to_tuple()[0]:
        return None
    return hit.to_tuple()[4].z


def plan(label, x_foot, x_top, actors, ignore):
    """The ramp for one staircase, as (centre, rotator, scale, report), or None with the reason said."""
    d = 1.0 if x_top > x_foot else -1.0
    run = abs(x_top - x_foot)
    # s runs from the foot up the stairs; the floor a walker meets is the highest of the lanes.
    prof = []
    for i in range(int(run / 5.0) + 1):
        s = 5.0 * i
        zs = [z for z in (floor(x_foot + d * s, y, ignore) for y in LANES) if z is not None]
        prof.append((s, max(zs) if zs else None))
    rises = []
    for (sa, za), (sb, zb) in zip(prof, prof[1:]):
        if za is not None and zb is not None and zb - za > 5.0:
            if rises and sb - rises[-1][0] <= 10.0:
                rises[-1] = (sb, rises[-1][1], zb)
            else:
                rises.append((sb, za, zb))
    for s, za, zb in rises:
        say("%s: rise at x %.0f: %.0f -> %.0f (%.0f cm)%s" % (label, x_foot + d * s, za, zb, zb - za,
            "  <-- too tall" if zb - za > STEP_LIMIT else ""))
    tall = [r for r in rises if r[2] - r[1] > STEP_LIMIT]
    if not tall:
        say("%s: no rise over %.0f cm - climbable; nothing placed" % (label, STEP_LIMIT))
        return None
    (s0, z0), (s1, z1) = (tall[0][0], tall[0][2]), (tall[-1][0], tall[-1][2])
    if s1 == s0:
        say("%s: only one flight found - nothing placed; this needs a look" % label)
        return None
    k = (z1 - z0) / (s1 - s0)
    slope = math.degrees(math.atan(k))
    floor_z = next(z for s, z in prof if z is not None)
    sa, za = s0 - (z0 - floor_z) / k, floor_z
    sb, zb = s1, z1
    worst = max((z - (za + (s - sa) * k + 1.0), s) for s, z in prof if z is not None and sa <= s <= sb)
    say("%s: the flights' tops line up at %.1f degrees; highest step above that line: %.1f cm at x %.0f" % (
        label, slope, worst[0], x_foot + d * worst[1]))
    if slope > MAX_SLOPE or worst[0] > 2.0:
        say("%s: no walkable ramp clears every step - nothing placed" % label)
        return None
    za, zb = za + 1.0, zb + 1.0

    xa, xb = x_foot + d * sa, x_foot + d * sb
    near = []
    for a in actors:
        if a.get_actor_label().startswith("Stairs_modular_part"):
            o, e = a.get_actor_bounds(True)
            if o.x - e.x < max(xa, xb) and o.x + e.x > min(xa, xb):
                near.append((o.y - e.y, o.y + e.y))
    y0, y1 = (min(p[0] for p in near), max(p[1] for p in near)) if near else (-250.0, 250.0)

    length = math.hypot(sb - sa, zb - za)
    t = math.radians(slope)
    thick = 10.0
    # Rising toward +X: pitch up, yaw 0. Rising toward -X: the same pitch, turned round (yaw 180).
    normal = unreal.Vector(-d * math.sin(t), 0.0, math.cos(t))
    mid = unreal.Vector(0.5 * (xa + xb), 0.5 * (y0 + y1), 0.5 * (za + zb)) - normal * (0.5 * thick)
    rot = unreal.Rotator(0.0, slope, 0.0 if d > 0 else 180.0)
    scale = unreal.Vector(length / 100.0, (y1 - y0) / 100.0, thick / 100.0)
    report = "%s: ramp from (%.0f, %.0f) to (%.0f, %.0f), %.0f cm long, %.1f degrees, %.0f cm wide" % (
        label, xa, za, xb, zb, length, slope, y1 - y0)
    return mid, rot, scale, report


def main():
    if not world.get_path_name().startswith(MAP + "."):
        say("open %s first - this is %s; nothing done" % (MAP, world.get_path_name()))
        return
    actors = eas.get_all_level_actors()
    labels = {s[0] for s in STAIRS} | set(OLD_LABELS)
    old = [a for a in actors if a.get_actor_label() in labels]
    ignore = old + [a for a in actors if "Chandellier" in a.get_actor_label()]

    plans = []
    for label, x_foot, x_top in STAIRS:
        p = plan(label, x_foot, x_top, actors, ignore)
        if p:
            plans.append((label,) + p)
    if not plans:
        return

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    with unreal.ScopedEditorTransaction("Invisible ramps over the temple's stairs"):
        for a in old:
            eas.destroy_actor(a)
        for label, mid, rot, scale, report in plans:
            ramp = eas.spawn_actor_from_object(cube, mid, rot)
            ramp.set_actor_label(label)
            ramp.set_actor_scale3d(scale)
            c = ramp.get_component_by_class(unreal.StaticMeshComponent)
            c.set_collision_profile_name("BlockAll")
            c.set_editor_property("cast_shadow", False)
            c.set_editor_property("affect_distance_field_lighting", False)
            c.set_visibility(False)   # invisible in the editor and the game; collision does not care
            say(report + " - placed")
    say("%d ramp(s) placed (invisible, collision only). One Ctrl+Z removes them. Not saved - walk them, then Ctrl+S." % len(plans))


main()
