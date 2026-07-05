# forest_place_returndoor.py — SIB Forest Phase 4 step 1: place the return door at the forest
# PlayerStart so it's visible on arrival; its self-contained walk-through overlap returns to the
# kitchen (bSelfReturnTrigger=True — no builder on this pre-made level). Idempotent.
# Editor-CLOSED: UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript -script="...forest_place_returndoor.py"
import unreal

MAP = "/Game/Maps/L_Elsewhere_Forest"
TAG = "###FORESTDOOR###"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

door_cls = unreal.load_class(None, "/Script/SibeliusGame.ReturnDoor")
if door_cls is None:
    unreal.log_error("%s AReturnDoor class not found — build the editor target first." % TAG)
else:
    les.load_level(MAP)
    ps = None
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.PlayerStart) or a.get_actor_label() == "PlayerStart":
            ps = a
            break
    if not ps:
        unreal.log_error("%s no PlayerStart in %s" % (TAG, MAP))
    else:
        psl = ps.get_actor_location()
        for a in list(eas.get_all_level_actors()):
            if a.get_actor_label() == "ReturnDoor_Forest":
                eas.destroy_actor(a)
        # A few metres in FRONT of the player (player faces +X by default), on the flat ground (Z~0),
        # yaw 180 so the door's sign face (+X) turns back toward the player. Walt nudges by eye in PIE
        # (it's a placed actor -> Details-editable, unlike the cathedral's spawned door).
        door_loc = unreal.Vector(psl.x + 400.0, psl.y, 0.0)
        door = eas.spawn_actor_from_class(door_cls, door_loc, unreal.Rotator(0.0, 180.0, 0.0))
        door.set_actor_label("ReturnDoor_Forest")
        door.set_editor_property("bSelfReturnTrigger", True)
        door.set_editor_property("HomeLevelName", "L_Office_v02")
        les.save_current_level()
        unreal.log("%s placed ReturnDoor_Forest at (%.0f,%.0f,%.0f); self-trigger ON -> home L_Office_v02." % (
            TAG, door_loc.x, door_loc.y, door_loc.z))
