# dump_living_room.py — run via ue_bridge exec-python (must set payload).
keys = ("playerstart", "poker", "hiddendoor", "sofa", "couch", "tv", "window", "glass", "sliding", "cabinet")
out = []
for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Actor):
    try:
        lbl = a.get_actor_label()
    except Exception:
        continue
    low = lbl.lower()
    if not any(k in low for k in keys):
        continue
    l = a.get_actor_location()
    r = a.get_actor_rotation()
    out.append({
        "label": lbl,
        "class": a.get_class().get_name(),
        "x": round(l.x, 1), "y": round(l.y, 1), "z": round(l.z, 1),
        "pitch": round(r.pitch, 1), "yaw": round(r.yaw, 1), "roll": round(r.roll, 1),
    })
out.sort(key=lambda d: d["label"])
payload = {"world": _world().get_name(), "count": len(out), "actors": out}
