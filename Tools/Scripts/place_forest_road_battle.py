# place_forest_road_battle.py — forest road battles: clone NavCorridor_Template
# along the level's MAIN road spline (longest SubBiome_Road spline), rotated to
# follow each bend and ground-snapped; place Shinbi companions at the arrival
# anchor and Gideon Refusers up the road. Idempotent per level (skips existing
# labels). Runs in the live editor via ue_bridge; save happens separately.
#
# Tunables per run (edit here): STEP/extents, counts.
import unreal

# Dense sampling: this road bends up to ~90 degrees between coarse samples, and
# long straight boxes cut the corners, splitting the navmesh into islands (the
# original 8000/5000 pass stranded the AI at island edges).
STEP = 2500.0            # spline distance between corridor boxes
HALF_ALONG = 2000.0      # box half-size along the road (overlaps neighbors)
HALF_ACROSS = 2500.0     # box half-size across the road
HALF_TALL = 2500.0       # box half-height (terrain variance)
NUM_SHINBI = 3
NUM_GIDEON = 3
GIDEON_START_DIST = 5000.0   # first Gideon this far up-road from the anchor
GIDEON_SPACING = 5000.0

payload = {"volumes": 0, "shinbi": [], "gideon": [], "notes": []}

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def find(label):
    for a in actors:
        if a.get_actor_label() == label:
            return a
    return None


def ground_z(x, y, fallback):
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            world,
            unreal.Vector(x, y, 1000.0),
            unreal.Vector(x, y, -10000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False, [], unreal.DrawDebugTrace.NONE, True,
            unreal.LinearColor.RED, unreal.LinearColor.GREEN, 0.0)
        if hit:
            return hit.to_tuple()[4].z
    except Exception as e:
        payload["notes"].append("trace fail at %.0f,%.0f: %s" % (x, y, e))
    return fallback


template = find("NavCorridor_Template")
if not template:
    payload["notes"].append("ABORT: NavCorridor_Template not found")
else:
    # --- main road spline = the longest one on any SubBiome_Road actor ------
    best_spline = None
    best_len = 0.0
    for a in actors:
        if "SubBiome_Road" not in a.get_class().get_name():
            continue
        for sp in a.get_components_by_class(unreal.SplineComponent):
            length = sp.get_spline_length()
            if length > best_len:
                best_len = length
                best_spline = sp
    if not best_spline:
        payload["notes"].append("ABORT: no road spline found")
    else:
        payload["road_length"] = round(best_len)

        # Template's unscaled brush extent, so clones scale to the real target.
        t_origin, t_extent = template.get_actor_bounds(False)
        t_scale = template.get_actor_scale3d()
        base = unreal.Vector(
            max(1.0, t_extent.x / max(0.01, t_scale.x)),
            max(1.0, t_extent.y / max(0.01, t_scale.y)),
            max(1.0, t_extent.z / max(0.01, t_scale.z)))
        want_scale = unreal.Vector(HALF_ALONG / base.x, HALF_ACROSS / base.y, HALF_TALL / base.z)

        d = 0.0
        idx = 0
        while d <= best_len:
            label = "NavCorridor_%03d" % idx
            existing = find(label)
            loc = best_spline.get_location_at_distance_along_spline(
                d, unreal.SplineCoordinateSpace.WORLD)
            tan = best_spline.get_tangent_at_distance_along_spline(
                d, unreal.SplineCoordinateSpace.WORLD)
            yaw = unreal.MathLibrary.conv_vector_to_rotator(tan).yaw
            gz = ground_z(loc.x, loc.y, loc.z - 1250.0)
            target = unreal.Vector(loc.x, loc.y, gz + 400.0)
            vol = existing if existing else eas.duplicate_actor(template)
            vol.set_actor_label(label)
            vol.set_actor_location(target, False, True)
            vol.set_actor_rotation(unreal.Rotator(0.0, yaw, 0.0), True)
            vol.set_actor_scale3d(want_scale)
            payload["volumes"] += 1
            d += STEP
            idx += 1

        # Park the hand-placed template inside the first box's spot too.
        first = best_spline.get_location_at_distance_along_spline(0.0, unreal.SplineCoordinateSpace.WORLD)
        template.set_actor_label("NavCorridor_Template")  # keep its name
        template.set_actor_location(
            unreal.Vector(first.x, first.y, ground_z(first.x, first.y, first.z - 1250.0) + 400.0),
            False, True)
        template.set_actor_scale3d(want_scale)

        # --- combatants ------------------------------------------------------
        anchor = find("BP_WorldAnchors1") or find("PlayerStart")
        if not anchor:
            payload["notes"].append("no anchor/PlayerStart; skipping characters")
        else:
            a_loc = anchor.get_actor_location()
            # Distance along the road nearest the anchor.
            key = best_spline.find_input_key_closest_to_world_location(a_loc)
            anchor_dist = best_spline.get_distance_along_spline_at_spline_input_key(key)

            shinbi_cls = unreal.load_class(None, "/Game/Characters/BP_Shinbi_Companion.BP_Shinbi_Companion_C")
            gideon_cls = unreal.load_class(None, "/Game/Characters/BP_Gideon_Refuser.BP_Gideon_Refuser_C")

            def place_char(cls, label, x, y):
                existing2 = find(label)
                if existing2:
                    payload["notes"].append(label + ": already placed")
                    return existing2
                z = ground_z(x, y, a_loc.z) + 100.0
                c = eas.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, 0.0))
                c.set_actor_label(label)
                return c

            if not shinbi_cls:
                payload["notes"].append("BP_Shinbi_Companion class not found")
            else:
                offsets = [(250.0, 250.0), (-250.0, 250.0), (0.0, -350.0)]
                for i, (ox, oy) in enumerate(offsets[:NUM_SHINBI], 1):
                    c = place_char(shinbi_cls, "Shinbi_%02d" % i, a_loc.x + ox, a_loc.y + oy)
                    if c:
                        loc2 = c.get_actor_location()
                        payload["shinbi"].append([c.get_actor_label(), round(loc2.x), round(loc2.y), round(loc2.z)])

            if not gideon_cls:
                payload["notes"].append("BP_Gideon_Refuser class not found")
            else:
                for i in range(NUM_GIDEON):
                    gd = (anchor_dist + GIDEON_START_DIST + i * GIDEON_SPACING) % best_len
                    loc3 = best_spline.get_location_at_distance_along_spline(
                        gd, unreal.SplineCoordinateSpace.WORLD)
                    c = place_char(gideon_cls, "RoadGideon_%02d" % (i + 1), loc3.x, loc3.y)
                    if c:
                        loc4 = c.get_actor_location()
                        payload["gideon"].append([c.get_actor_label(), round(loc4.x), round(loc4.y), round(loc4.z)])

        # --- navmesh runtime generation --------------------------------------
        recast = None
        for a in eas.get_all_level_actors():
            if a.get_class().get_name() == "RecastNavMesh":
                recast = a
                break
        if recast:
            recast.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
            payload["recast"] = "set to Dynamic"
        else:
            payload["recast"] = "NOT FOUND yet - run RebuildNavigation then re-run this script"
