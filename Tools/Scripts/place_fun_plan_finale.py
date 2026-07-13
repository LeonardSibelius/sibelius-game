# place_fun_plan_finale.py — FUN_PLAN §5: the Ch7 Synthesis in L_Cathedral.
#
# Geometry (from recon): the nave runs along +X — PlayerStart at the origin,
# SlotCabinet at x=3400 facing back down the nave. So:
#   - AFinaleAltar at x=2400 on the nave centerline (the rite happens here),
#   - one wall slab across the nave at x=2800, tagged FinaleWall, sealing the
#     cabinet until the Synthesis completes (the altar destroys tagged actors).
# Placeholder cube slab; Walt dresses it as Mrs. Hall's error-blocks later.
# Idempotent; saves the level.

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_Cathedral")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()
results = []


def find(label):
    for a in actors:
        if a.get_actor_label() == label:
            return a
    return None


CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")

# --- The Synthesis altar -----------------------------------------------------
if find("FinaleAltar"):
    results.append("FinaleAltar: already placed")
else:
    cls = unreal.load_class(None, "/Script/SibeliusGame.FinaleAltar")
    altar = eas.spawn_actor_from_class(cls, unreal.Vector(2400.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    altar.set_actor_label("FinaleAltar")
    for c in altar.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() == "Mesh":
            c.set_editor_property("StaticMesh", CUBE)
            c.set_editor_property("RelativeScale3D", unreal.Vector(1.6, 1.6, 0.9))
            c.set_editor_property("RelativeLocation", unreal.Vector(0.0, 0.0, 45.0))
    results.append("FinaleAltar: placed at 2400,0,0")

# --- The last Mrs. Hall wall -------------------------------------------------
if find("FinaleWall_Slab"):
    results.append("FinaleWall_Slab: already placed")
else:
    wall = eas.spawn_actor_from_class(unreal.StaticMeshActor,
                                      unreal.Vector(2800.0, 0.0, 600.0), unreal.Rotator(0.0, 0.0, 0.0))
    wall.set_actor_label("FinaleWall_Slab")
    smc = wall.get_editor_property("StaticMeshComponent")
    smc.set_editor_property("StaticMesh", CUBE)
    wall.set_actor_scale3d(unreal.Vector(0.5, 30.0, 12.0))   # 0.5m thick, 30m wide, 12m tall
    wall.set_editor_property("Tags", ["FinaleWall"])          # the altar drops everything with this tag
    results.append("FinaleWall_Slab: placed at 2800,0,600 (tagged FinaleWall)")

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print("RESULT: " + " | ".join(results))
