# place_freakbook_binder.py — Dee's binder, in the attic (docs/FREAKBOOK.md: she left her 1983
# listing "in somebody's attic"; Freakbook's profile button mails it to her once he has it).
#
# *** RUN FROM THE OPEN EDITOR with L_Office_v02 loaded, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_freakbook_binder.py"
#
# The attic is measured, not looked at (its kit is purchased): the binder goes near Isla's portal,
# which place_home_portal.py stood on the one measured-clear stretch of attic floor. Around the
# portal, on rings 1.1 to 2.6 m out, every spot is traced: five traces down (the middle and the
# four corners of the binder) must land on one flat surface, nothing may hang within 60 cm above
# it, and four short traces sideways must find room. A spot on top of a storage box is preferred -
# a binder left on a box reads as left - then clear floor; nearest the hatch-to-portal line, so he
# sees it as he climbs in. Safe to re-run: it moves the binder it placed rather than adding one.
# Saves the level. Never loads a level.

import math
import unreal

TAG = "###BINDER###"
PORTAL = "IslaPortal"
HATCH = "HatchLock"
LABEL = "FreakbookBinder_Dee"
HALF = unreal.Vector(14.0, 16.0, 3.0)   # the binder lying flat: 28 x 32 x 6 cm
YAW = 17.0                              # left at an angle, not squared up

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def trace(world, start, end, ignore):
    """(impact point, hit actor) or None. line_trace_single's return differs between versions
    (dress_meadow.py paid for assuming), and GameplayStatics.break_hit_result is not exposed in
    5.7: read the HitResult's own tuple (fix_playerstart_capsule.py's way) - [0] blocking,
    [5] impact point, [9] the actor."""
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
        unreal.DrawDebugTrace.NONE, True, unreal.LinearColor.RED, unreal.LinearColor.GREEN, 1.0)
    if hit is None:
        return None
    if isinstance(hit, tuple):
        hit = next((h for h in hit if isinstance(h, unreal.HitResult)), None)
        if hit is None:
            return None
    t = hit.to_tuple()
    return (t[5], t[9]) if t and t[0] else None


def spot(world, x, y, floor_z, ignore):
    """The support under (x, y) if the binder fits there flat and clear, else None."""
    top = floor_z + 130.0
    centre = trace(world, unreal.Vector(x, y, top), unreal.Vector(x, y, floor_z - 40.0), ignore)
    if not centre:
        return None
    z = centre[0].z
    if z < floor_z - 15.0 or z > floor_z + 115.0:
        return None
    a = math.radians(YAW)
    for sx, sy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        lx, ly = sx * (HALF.x + 2.0), sy * (HALF.y + 2.0)
        cx, cy = x + lx * math.cos(a) - ly * math.sin(a), y + lx * math.sin(a) + ly * math.cos(a)
        corner = trace(world, unreal.Vector(cx, cy, top), unreal.Vector(cx, cy, floor_z - 40.0), ignore)
        if not corner or abs(corner[0].z - z) > 1.5:
            return None   # an edge, a lip or a gap under one corner
    if trace(world, unreal.Vector(x, y, z + 1.0), unreal.Vector(x, y, z + 60.0), ignore):
        return None       # something hangs right over it
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if trace(world, unreal.Vector(x, y, z + 4.0), unreal.Vector(x + dx * 24.0, y + dy * 24.0, z + 4.0), ignore):
            return None   # boxed in
    return z, centre[1]


def main():
    world = ues.get_editor_world()
    if "L_Office_v02" not in world.get_path_name() or ues.get_game_world():
        say("REFUSING: open L_Office_v02 with Play stopped. Nothing changed.")
        return
    cls = unreal.load_class(None, "/Script/SibeliusGame.FreakbookItem")
    if not cls:
        say("FreakbookItem is not built yet - build the editor first. Nothing changed.")
        return
    actors = eas.get_all_level_actors()
    by_label = {a.get_actor_label(): a for a in actors}
    portal, hatch = by_label.get(PORTAL), by_label.get(HATCH)
    if not portal:
        say("no %s in this level - nothing to find the attic by. Nothing changed." % PORTAL)
        return
    p = portal.get_actor_location()
    h = hatch.get_actor_location() if hatch else p
    floor = trace(world, unreal.Vector(p.x, p.y, p.z + 100.0), unreal.Vector(p.x, p.y, p.z - 200.0), [portal])
    floor_z = floor[0].z if floor else p.z
    existing = [a for a in actors if a.get_class().get_name() == "FreakbookItem" and str(a.get_editor_property("person")) == "dee"]
    ignore = [portal] + existing

    best = None
    for radius in range(110, 261, 30):
        for k in range(24):
            ang = 2.0 * math.pi * k / 24.0
            x, y = p.x + radius * math.cos(ang), p.y + radius * math.sin(ang)
            if hatch and math.hypot(x - h.x, y - h.y) < 80.0:
                continue      # not on the way up the ladder
            found = spot(world, x, y, floor_z, ignore)
            if not found:
                continue
            z, support = found
            on_box = z > floor_z + 20.0
            # Nearest the line from the hatch to the portal: what he sees as he climbs in.
            vx, vy = p.x - h.x, p.y - h.y
            length = max(1.0, math.hypot(vx, vy))
            off_line = abs((x - h.x) * vy - (y - h.y) * vx) / length
            score = (0 if on_box else 1, off_line + 0.2 * radius)
            if best is None or score < best[0]:
                best = (score, x, y, z, support, on_box, radius)
    if not best:
        say("no flat, clear spot within 2.6 m of %s. Nothing changed." % PORTAL)
        return
    _, x, y, z, support, on_box, radius = best
    at = unreal.Vector(x, y, z + HALF.z + 0.2)
    binder = existing[0] if existing else eas.spawn_actor_from_class(cls, at, unreal.Rotator(0.0, 0.0, YAW))
    for extra in existing[1:]:
        eas.destroy_actor(extra)
    binder.set_actor_location(at, False, False)
    binder.set_actor_rotation(unreal.Rotator(0.0, 0.0, YAW), False)
    binder.set_editor_property("person", "dee")
    binder.set_actor_label(LABEL)
    les.save_current_level()
    say("%s at (%.0f, %.0f, %.0f): %s, %.0f cm up, %.0f cm from %s and %.0f from the hatch. Saved." % (
        LABEL, at.x, at.y, at.z, ("on " + support.get_actor_label()) if on_box and support else "on the attic floor",
        z - floor_z, radius, PORTAL, math.hypot(x - h.x, y - h.y)))


main()
