# place_basketball_court.py - the three-point contest's half-court in the Transhuman Chess Park
# (L_Chess), and the PlayerStart the living room's BASKETBALL door sets Leonard down at.
#
# Walt, 2026-10-15: the court goes "in the chess park". The park is bought scenery and is not
# rendered, so the site was found by measuring (survey_court_site.py): the only footprint near the
# chess table that is flat and empty is the patch of TEMP_Road about 31 m from it, ground within
# 20 cm across the slab. Nearer, the curb ramps and walkways rise a metre or more.
#
# *** RUN FROM THE OPEN EDITOR with L_Chess loaded, PIE stopped ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_basketball_court.py"
#
# DRY_RUN = True (the default) measures the ground under the slab every 50 cm (finer than the
# survey, for posts and bollards a metre grid steps over) and lists every actor standing in the
# court's space, then says where the court and the start would go. False places both and saves.
# Re-running moves the court already there instead of adding a second, and replaces the start.
# Refuses unless the open map is already L_Chess (never loads a level, place_go_table.py).

import math
import unreal

TAG = "###HOOPCOURT###"
DRY_RUN = True
PARK_MAP = "/Game/Maps/L_Chess"
ARRIVAL_TAG = "BasketballCourt"      # place_basketball_door.py's arrival tag
START_LABEL = "PlayerStart_BasketballCourt"

# The site (survey_court_site.py, 2026-10-15): the floor under the middle of the rim, and the yaw
# the court's +X (out from the basket toward half court) points along.
SITE_X, SITE_Y = 341.0, 3097.0
YAW = 90.0

# The slab's footprint in the court's frame (BasketballCourt.cpp: the lines plus the apron), and
# how high the game's space over it goes: the ball's arc tops out under six metres.
SLAB = (-360.0, 1373.0, -912.0, 912.0)
CLEAR_HEIGHT = 650.0
GRID = 50.0
# The slab top sits this far over the highest ground under it; BasketballCourt's slab is deep
# enough to reach the lowest.
OVER_GROUND = 1.0
SLAB_DEPTH = 40.0                    # BasketballCourtDetail::SlabDepth

# Where he arrives: just inside the free-throw line, facing the basket, within the interactor's
# 450 cm of the hoop so E starts the contest at once.
START_X = 400.0

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def to_world(u, v):
    c, s = math.cos(math.radians(YAW)), math.sin(math.radians(YAW))
    return SITE_X + u * c - v * s, SITE_Y + u * s + v * c


def main():
    world = ues.get_editor_world()
    here = world.get_path_name()
    if PARK_MAP not in here:
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (here, PARK_MAP))
        return
    actors = eas.get_all_level_actors()
    existing = [a for a in actors if a.get_class().get_name() == "BasketballCourt"]
    court_class = unreal.load_class(None, "/Script/SibeliusGame.BasketballCourt")

    # The ground under the slab, every GRID cm, ignoring the court itself if it is already here.
    ignore = list(existing)
    zs = []
    ground = {}
    x0, x1, y0, y1 = SLAB
    u = x0
    while u <= x1 + 0.1:
        v = y0
        while v <= y1 + 0.1:
            wx, wy = to_world(u, v)
            hit = unreal.SystemLibrary.line_trace_single(
                world, unreal.Vector(wx, wy, 3000.0), unreal.Vector(wx, wy, -3000.0),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore, unreal.DrawDebugTrace.NONE, True)
            t = hit.to_tuple() if hit else None
            if t and t[0]:
                zs.append(t[5].z)
                name = t[9].get_actor_label() if t[9] else "?"
                ground[name] = ground.get(name, 0) + 1
            v += GRID
        u += GRID
    if not zs:
        say("nothing under the site at all - wrong SITE_X/SITE_Y?")
        return
    zs.sort()
    # A ray that stops well over the rest stopped in a tree's crown, not on the ground: the two
    # narrowleaf trees past the half-court corners hang over the slab 6.7 m up (2026-10-15).
    median = zs[len(zs) // 2]
    overhead = [z for z in zs if z > median + 150.0]
    zs = [z for z in zs if z <= median + 150.0]
    low, high = zs[0], zs[-1]
    say("ground under the slab: %d points, %.1f .. %.1f (varies %.1f cm); %d rays stopped overhead (%s); %s" % (
        len(zs), low, high, high - low, len(overhead),
        ", ".join("%.0f" % z for z in overhead[:6]) or "none",
        ", ".join("%s x%d" % kv for kv in sorted(ground.items(), key=lambda kv: -kv[1]))))
    top = high + OVER_GROUND
    if top - SLAB_DEPTH > low:
        say("WARNING: the slab's bottom (%.1f) would float over the lowest ground (%.1f)." % (top - SLAB_DEPTH, low))

    # Anything standing in the court's space: bounds overlapping the slab, from just over the
    # ground up to CLEAR_HEIGHT. The ground's own pieces are listed apart.
    corners = [to_world(a, b) for a in (x0, x1) for b in (y0, y1)]
    bx0, bx1 = min(c[0] for c in corners), max(c[0] for c in corners)
    by0, by1 = min(c[1] for c in corners), max(c[1] for c in corners)
    standing = []
    for a in actors:
        if a in existing:
            continue
        origin, ext = a.get_actor_bounds(False)
        if ext.x <= 0 and ext.y <= 0 and ext.z <= 0:
            continue
        if origin.x + ext.x < bx0 or origin.x - ext.x > bx1 or origin.y + ext.y < by0 or origin.y - ext.y > by1:
            continue
        if origin.z + ext.z < top + 3.0 or origin.z - ext.z > top + CLEAR_HEIGHT:
            continue
        label = a.get_actor_label()
        kind = a.get_class().get_name()
        if label.startswith("ground_") or label.startswith("TEMP_Road"):
            kind += ", the ground"
        elif ext.x > 5000 or ext.y > 5000:
            kind += ", huge"
        standing.append("%s (%s) at (%.0f, %.0f, %.0f) +-(%.0f, %.0f, %.0f)" % (
            label, kind, origin.x, origin.y, origin.z, ext.x, ext.y, ext.z))
    say("%d actors reach into the court's space:" % len(standing))
    for line in standing:
        say("  " + line)

    rim = unreal.Vector(SITE_X, SITE_Y, top)
    sx, sy = to_world(START_X, 0.0)
    start = unreal.Vector(sx, sy, top + 100.0)
    start_yaw = (YAW + 180.0) % 360.0
    say("the court %s with its origin (under the rim) at %s, yaw %.0f; %s" % (
        "would go" if DRY_RUN else "goes", rim, YAW,
        "a court is already here and would move" if existing else "none yet"))
    say("%s %s at %s facing yaw %.0f (the basket %.0f cm ahead)" % (
        START_LABEL, "would go" if DRY_RUN else "goes", start, start_yaw, START_X))
    if DRY_RUN:
        return
    if not court_class:
        say("BasketballCourt class not built yet - rebuild the editor first.")
        return

    court = existing[0] if existing else eas.spawn_actor_from_class(court_class, rim, unreal.Rotator(0.0, 0.0, YAW))
    court.set_actor_location(rim, False, False)
    court.set_actor_rotation(unreal.Rotator(0.0, 0.0, YAW), False)
    court.set_actor_label("BasketballCourt")
    for old in [a for a in actors if a.get_actor_label() == START_LABEL]:
        eas.destroy_actor(old)
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, start, unreal.Rotator(0.0, 0.0, start_yaw))
    ps.set_editor_property("player_start_tag", ARRIVAL_TAG)
    ps.set_actor_label(START_LABEL)
    les.save_current_level()
    say("placed BasketballCourt at %s and %s (tag %s). Saved %s." % (rim, START_LABEL, ARRIVAL_TAG, PARK_MAP))


main()
