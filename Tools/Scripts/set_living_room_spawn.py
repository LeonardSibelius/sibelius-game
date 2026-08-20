# set_living_room_spawn.py — PlayerStart in the living room, hallway ahead, poker glass behind.
# Walt 2026-08-19. Run via ue_bridge exec-python (must set payload) or -run=pythonscript.
# Location: open floor between the desk (south-west) and the sofa. Yaw 90 = look +Y
# down the room toward the hall. PokerDoor_Kitchen sits on the south sliding glass,
# which is then behind the player.
import unreal

LOC = unreal.Vector(-2050.0, 9450.0, 158.0)  # carpet Z~62 + capsule half-height ~96
# UE Python Rotator is (roll, pitch, yaw) here — C++ FRotator is Pitch,Yaw,Roll.
# Yaw 90 looks +Y down the living room toward the hall; south glass / poker behind.
ROT = unreal.Rotator(0.0, 0.0, 90.0)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
start = None
for a in eas.get_all_level_actors():
    try:
        if a.get_actor_label() == "PlayerStart":
            start = a
            break
    except Exception:
        pass

if start is None:
    payload = {"error": "PlayerStart not found in editor world"}
else:
    start.set_actor_location(LOC, False, False)
    start.set_actor_rotation(ROT, False)
    wl = start.get_actor_location()
    wr = start.get_actor_rotation()
    payload = {
        "ok": True,
        "label": start.get_actor_label(),
        "location": {"x": round(wl.x, 1), "y": round(wl.y, 1), "z": round(wl.z, 1)},
        "rotation": {"pitch": round(wr.pitch, 1), "yaw": round(wr.yaw, 1), "roll": round(wr.roll, 1)},
        "note": "hallway +Y ahead; PokerDoor_Kitchen / south glass behind. Stop PIE and Play to see it.",
    }
