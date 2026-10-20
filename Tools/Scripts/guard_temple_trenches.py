# guard_temple_trenches.py - keep Leonard out of the planted trenches behind the statues.
#
# Walt, 2026-10-19, in L_Archaic_Temple, a screenshot from the bottom: "i got stuck in the ditch
# behind the statues". Measured (numbers only - the level is a bought pack's): behind each side
# aisle, between the rails at y +-720 and y +-1640, runs a trench planted with pines, its floor at
# -180 - 2.8 m below the aisles, with no way back up. The rails are 65 cm high: a jump clears them.
#
# So: an invisible wall standing 3 m above each of the four rails, the rail's whole length - the
# trench stays in view over the rails; nobody gets in. Collision only, never seen, in the outliner
# folder VoidGuards with the edge walls (guard_temple_edges.py).
#
# And one of the pack's own floor tiles, FloorBase_part_3_square_bevel49, is set to simulate physics
# while Static: every step on it logged "has to be 'Movable' if you'd like to AddForceAtLocation"
# (299 times in one walk). It cannot move either way; its physics is switched off.
#
# One transaction: a single Ctrl+Z undoes it all. Does NOT save - Walt walks it, then Ctrl+S.
# Safe to re-run: it replaces the walls it made.
#
# *** RUN WITH L_Archaic_Temple OPEN, PIE stopped ***

import unreal

TAG = "###TRENCH###"
MAP = "/Game/Maps/L_Archaic_Temple"
PREFIX = "TrenchGuard_"
RAIL_LINES = (720.0, 1640.0)    # |y| of the rails either side of the trenches
HEIGHT, THICK = 300.0, 20.0
PHYSICS_TILE = "FloorBase_part_3_square_bevel49"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def say(msg):
    print("%s %s" % (TAG, msg))


def main():
    if not world.get_path_name().startswith(MAP + "."):
        say("open %s first - this is %s; nothing done" % (MAP, world.get_path_name()))
        return
    actors = eas.get_all_level_actors()
    old = [a for a in actors if a.get_actor_label().startswith(PREFIX)]

    lines = {}
    for a in actors:
        if not a.get_actor_label().startswith("FenceBase"):
            continue
        o, e = a.get_actor_bounds(True)
        if e.x < e.y or o.z + e.z > 250.0:
            continue        # rails running along X, at the lower level
        for ly in RAIL_LINES:
            for sign in (1.0, -1.0):
                if abs(o.y - sign * ly) < 25.0:
                    lines.setdefault(sign * ly, []).append((o, e))
    if len(lines) != 4:
        say("expected the four trench rails, found %d lines (%s) - nothing done" % (len(lines), sorted(lines)))
        return

    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    with unreal.ScopedEditorTransaction("Keep out of the temple's trenches"):
        for a in old:
            eas.destroy_actor(a)
        for i, (ly, parts) in enumerate(sorted(lines.items())):
            x0 = min(o.x - e.x for o, e in parts)
            x1 = max(o.x + e.x for o, e in parts)
            top = max(o.z + e.z for o, e in parts)
            w = eas.spawn_actor_from_object(cube, unreal.Vector(0.5 * (x0 + x1), ly, top + HEIGHT / 2.0), unreal.Rotator(0.0, 0.0, 0.0))
            w.set_actor_label("%s%d" % (PREFIX, i + 1))
            w.set_folder_path("VoidGuards")
            w.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, THICK / 100.0, HEIGHT / 100.0))
            c = w.get_component_by_class(unreal.StaticMeshComponent)
            c.set_collision_profile_name("BlockAll")
            c.set_editor_property("cast_shadow", False)
            c.set_editor_property("affect_distance_field_lighting", False)
            c.set_visibility(False)
            say("%s%d: above the rail at y %.0f, x %.0f..%.0f (%.0f m), from %.0f to %.0f" % (
                PREFIX, i + 1, ly, x0, x1, (x1 - x0) / 100.0, top, top + HEIGHT))

        tile = next((a for a in actors if a.get_actor_label() == PHYSICS_TILE), None)
        if tile:
            c = tile.get_component_by_class(unreal.StaticMeshComponent)
            if c.is_simulating_physics():
                c.set_simulate_physics(False)
                say("%s: simulate physics switched off (it is Static; it never moved)" % PHYSICS_TILE)
            else:
                say("%s: physics already off" % PHYSICS_TILE)
    say("done. One Ctrl+Z undoes it all. Not saved - walk it, then Ctrl+S.")


main()
