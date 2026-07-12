# place_fun_plan_curios.py — FUN_PLAN §4: curios in the Many Worlds deck.
#
# One ACurio in four of the eight forests (the other four stay empty — the point
# is quiet discovery, not a checklist). Each sits a short wander from the
# BP_WorldAnchors arrival area, ground-snapped by a downward line trace.
# Idempotent per level (skips if the labeled curio already exists). Saves each
# level after placement.
#
# Run via the bridge with the editor open (any level; it loads each forest):
#   exec(open(r'C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_fun_plan_curios.py').read())

import unreal

PLACEMENTS = [
    ("/Game/Maps/L_Forest_01", "Curio_Forest_01", "FallenStar",  ( 500.0,  350.0)),
    ("/Game/Maps/L_Forest_03", "Curio_Forest_03", "BrassGear",   (-450.0,  520.0)),
    ("/Game/Maps/L_Forest_06", "Curio_Forest_06", "DrownedBook", ( 620.0, -430.0)),
    ("/Game/Maps/L_Forest_08", "Curio_Forest_08", "BottledTide", (-520.0, -560.0)),
]

results = []

for map_path, label, curio_id, offset in PLACEMENTS:
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        results.append("%s: LOAD FAILED" % map_path)
        continue

    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()

    if any(a.get_actor_label() == label for a in actors):
        results.append("%s: already placed" % label)
        continue

    anchor = None
    for a in actors:
        if "WorldAnchors" in a.get_class().get_name() or "Anchor" in a.get_actor_label():
            anchor = a
            break
    if not anchor:
        results.append("%s: NO ANCHOR FOUND" % map_path)
        continue

    base = anchor.get_actor_location()
    x, y = base.x + offset[0], base.y + offset[1]

    # Ground-snap: trace down through the offset point; fall back to anchor
    # height if the trace finds nothing (then Walt nudges by eye).
    z = base.z + 30.0
    try:
        hit = unreal.SystemLibrary.line_trace_single(
            world,
            unreal.Vector(x, y, base.z + 3000.0),
            unreal.Vector(x, y, base.z - 6000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False, [], unreal.DrawDebugTrace.NONE, True,
            unreal.LinearColor.RED, unreal.LinearColor.GREEN, 0.0)
        if hit:
            z = hit.to_tuple()[4].z + 55.0   # tuple index 4 = trace location
    except Exception as trace_err:
        results.append("%s: trace fallback (%s)" % (label, trace_err))

    cls = unreal.load_class(None, "/Script/SibeliusGame.Curio")
    curio = eas.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, 0.0))
    curio.set_actor_label(label)
    curio.set_editor_property("DefaultCurioId", curio_id)
    curio.set_editor_property("DefaultPlaceTypeId", "PoplarForest")

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    results.append("%s: placed %s at %.0f,%.0f,%.0f" % (label, curio_id, x, y, z))

print("RESULT: " + " | ".join(results))
