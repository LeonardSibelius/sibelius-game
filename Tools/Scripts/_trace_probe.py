w = _world()
names = []
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
    try:
        cls = a.get_class().get_name()
        if "Poker" in cls or "poker" in a.get_actor_label() or "Machine" in cls:
            names.append(a.get_actor_label() + ":" + cls)
    except Exception:
        pass
payload = {"world": w.get_name() if w else None, "hits": names[:40], "n": len(names)}
