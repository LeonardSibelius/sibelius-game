# fix_living_room_book.py — pickup is still at the table origin (the white blowout).
# Move it onto the rim book the player dragged toward, then save.
import unreal

TABLE = "SM_Table_A2"
PICKUP = "BookPickup_LivingRoom"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find(label):
    for a in eas.get_all_level_actors():
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            pass
    return None


def all_actors():
    out = []
    for a in eas.get_all_level_actors():
        try:
            out.append(a)
        except Exception:
            pass
    return out


table = find(TABLE)
pickup = find(PICKUP)
if table is None or pickup is None:
    payload = {
        "ok": False,
        "error": "missing actors",
        "table": bool(table),
        "pickup": bool(pickup),
    }
else:
    tloc = table.get_actor_location()
    ploc = pickup.get_actor_location()
    near = []
    rim = None
    rim_d = 0.0
    for a in all_actors():
        try:
            lbl = a.get_actor_label()
            cls = a.get_class().get_name()
        except Exception:
            continue
        loc = a.get_actor_location()
        dxy = ((loc.x - tloc.x) ** 2 + (loc.y - tloc.y) ** 2) ** 0.5
        dz = abs(loc.z - tloc.z)
        if dxy > 90.0 or dxy < 8.0 or dz > 80.0:
            continue
        low = lbl.lower()
        rec = {
            "label": lbl,
            "class": cls,
            "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1),
            "dxy": round(dxy, 1),
        }
        near.append(rec)
        is_bookish = ("book" in low) or cls == "StaticMeshActor" and "book" in low
        # Prefer a StaticMeshActor book on the rim, not the pickup itself.
        if a is pickup:
            continue
        if "book" in low and cls != "BookPickup":
            if dxy > rim_d:
                rim_d = dxy
                rim = a

    notes = []
    target = None
    if rim:
        target = rim.get_actor_location()
        rot = rim.get_actor_rotation()
        pickup.set_actor_location(target, False, False)
        pickup.set_actor_rotation(rot, False)
        # Hide the set-dressing copy so there is one book.
        rim.set_is_temporarily_hidden_in_editor(True)
        rim.set_actor_hidden_in_game(True)
        for c in rim.get_components_by_class(unreal.PrimitiveComponent):
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        notes.append("moved pickup onto " + rim.get_actor_label() + " and hid the prop")
    else:
        # Fallback: 40 cm toward -X (the screenshot's rim book sits toward the
        # camera / west of center). Keep table-top Z.
        target = unreal.Vector(tloc.x - 40.0, tloc.y - 18.0, ploc.z)
        pickup.set_actor_location(target, False, False)
        notes.append("no rim book found; offset toward table edge")

    nl = pickup.get_actor_location()
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    saved = bool(les.save_current_level()) if les else False
    payload = {
        "ok": True,
        "saved": saved,
        "pickup": {"x": round(nl.x, 1), "y": round(nl.y, 1), "z": round(nl.z, 1)},
        "table": {"x": round(tloc.x, 1), "y": round(tloc.y, 1), "z": round(tloc.z, 1)},
        "notes": notes,
        "near": near,
    }
