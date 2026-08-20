# dump_nearby_books.py — any book-like actor near spawn / table.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
origin = unreal.Vector(-2137.6, 9445.2, 60.7)
books = []
for a in eas.get_all_level_actors():
    try:
        lbl = a.get_actor_label()
        cls = a.get_class().get_name()
    except Exception:
        continue
    low = lbl.lower()
    loc = a.get_actor_location()
    dxy = ((loc.x - origin.x) ** 2 + (loc.y - origin.y) ** 2) ** 0.5
    if dxy > 250.0:
        continue
    if "book" not in low and "magazine" not in low and cls != "BookPickup":
        continue
    books.append({
        "label": lbl,
        "class": cls,
        "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1),
        "dxy": round(dxy, 1),
        "dz": round(loc.z - origin.z, 1),
    })
books.sort(key=lambda d: d["dxy"])
payload = {"ok": True, "count": len(books), "books": books}
