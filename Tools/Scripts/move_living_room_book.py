# move_living_room_book.py — sit the pickup on the rim, by the blue mug (SM_Mug_A3).
# The glow was still at the table origin (white blowout on the newspaper).
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find(label):
    for a in eas.get_all_level_actors():
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            pass
    return None


pickup = find("BookPickup_LivingRoom")
mug = find("SM_Mug_A3")
table = find("SM_Table_A2")
if pickup is None:
    payload = {"ok": False, "error": "no BookPickup_LivingRoom"}
else:
    # Rim of the round table, south-west of center, next to the blue mug.
    if mug and table:
        m = mug.get_actor_location()
        t = table.get_actor_location()
        # Left of the mug from the player's view (more -X), still on the table top.
        loc = unreal.Vector(m.x - 42.0, m.y - 8.0, pickup.get_actor_location().z)
        # Keep it over the table disk (~55 cm radius).
        dx = loc.x - t.x
        dy = loc.y - t.y
        dist = (dx * dx + dy * dy) ** 0.5
        if dist > 52.0:
            s = 52.0 / dist
            loc = unreal.Vector(t.x + dx * s, t.y + dy * s, loc.z)
    else:
        t = table.get_actor_location() if table else unreal.Vector(-2137.6, 9445.2, 60.7)
        loc = unreal.Vector(t.x - 28.0, t.y - 42.0, 125.5)

    old = pickup.get_actor_location()
    pickup.set_actor_location(loc, False, False)
    pickup.set_actor_rotation(unreal.Rotator(0.0, 0.0, -25.0), False)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saved = bool(les.save_current_level()) if les else False
    nl = pickup.get_actor_location()
    payload = {
        "ok": True,
        "saved": saved,
        "old": {"x": round(old.x, 1), "y": round(old.y, 1), "z": round(old.z, 1)},
        "new": {"x": round(nl.x, 1), "y": round(nl.y, 1), "z": round(nl.z, 1)},
    }
