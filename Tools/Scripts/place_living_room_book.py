# place_living_room_book.py — one ABookPickup on the living-room coffee table.
# Walt 2026-08-19: E tutorial + 5 sauce at spawn; no upstairs invite.
# Idempotent by label BookPickup_LivingRoom. Duplicates an existing library book
# so mesh/glow match. Run via ue_bridge with L_Office_v02 open, then save.
import unreal

LABEL = "BookPickup_LivingRoom"
SPAWN = unreal.Vector(-2050.0, 9450.0, 158.0)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def actors():
    out = []
    for a in eas.get_all_level_actors():
        try:
            out.append(a)
        except Exception:
            pass
    return out


def find_label(name):
    for a in actors():
        try:
            if a.get_actor_label() == name:
                return a
        except Exception:
            continue
    return None


def xy_dist(a, b):
    return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2) ** 0.5


existing = find_label(LABEL)
notes = []

# Furniture hunt: coffee table / small table nearest spawn, else sofa, else a
# point a step in front of the player (yaw 90 = +Y).
table = None
sofa = None
best_table = 1e9
best_sofa = 1e9
furniture = []
for a in actors():
    try:
        lbl = a.get_actor_label()
        cls = a.get_class().get_name()
    except Exception:
        continue
    if cls != "StaticMeshActor":
        continue
    low = lbl.lower()
    loc = a.get_actor_location()
    d = xy_dist(loc, SPAWN)
    rec = {"label": lbl, "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1), "d": round(d, 1)}
    if d < 1200 and any(k in low for k in ("table", "sofa", "couch", "desk", "coffee")):
        furniture.append(rec)
    if d < 900 and "table" in low:
        if d < best_table:
            best_table = d
            table = a
    if d < 900 and ("sofa" in low or "couch" in low):
        if d < best_sofa:
            best_sofa = d
            sofa = a

# Sit the book on the table top (bounds max Z) or in front of the sofa.
book_loc = None
anchor = None
if table:
    origin = table.get_actor_location()
    z = origin.z + 8.0
    for c in table.get_components_by_class(unreal.StaticMeshComponent):
        try:
            box = c.get_local_bounds()
            max_z = box[1].z if isinstance(box, (tuple, list)) else box.max.z
            scale = table.get_actor_scale3d()
            z = origin.z + abs(max_z) * abs(scale.z) + 2.0
        except Exception:
            pass
        break
    book_loc = unreal.Vector(origin.x, origin.y, z)
    anchor = table.get_actor_label()
elif sofa:
    origin = sofa.get_actor_location()
    # Coffee-table spot: a half-meter toward spawn from the sofa, table height.
    toward = SPAWN - origin
    toward.z = 0.0
    n = (toward.x ** 2 + toward.y ** 2) ** 0.5
    if n > 1.0:
        origin = unreal.Vector(origin.x + toward.x / n * 80.0, origin.y + toward.y / n * 80.0, origin.z)
    book_loc = unreal.Vector(origin.x, origin.y, origin.z + 40.0)
    anchor = sofa.get_actor_label()
else:
    # Fallback: one meter in front of PlayerStart (+Y), coffee-table height.
    book_loc = unreal.Vector(SPAWN.x, SPAWN.y + 120.0, 95.0)
    anchor = "PlayerStart+Y fallback"

src = None
for a in actors():
    try:
        if a.get_class().get_name() == "BookPickup" and a.get_actor_label() != LABEL:
            src = a
            break
    except Exception:
        continue

if existing:
    existing.set_actor_location(book_loc, False, False)
    notes.append("moved existing " + LABEL)
    book = existing
elif src is None:
    cls = unreal.load_class(None, "/Script/SibeliusGame.BookPickup")
    book = eas.spawn_actor_from_class(cls, book_loc, unreal.Rotator(0.0, 0.0, 0.0))
    book.set_actor_label(LABEL)
    notes.append("spawned fresh BookPickup (no library original to copy)")
else:
    book = eas.duplicate_actor(src)
    book.set_actor_label(LABEL)
    book.set_actor_location(book_loc, False, False)
    notes.append("duplicated " + src.get_actor_label())

wl = book.get_actor_location()
payload = {
    "ok": True,
    "label": book.get_actor_label(),
    "anchor": anchor,
    "location": {"x": round(wl.x, 1), "y": round(wl.y, 1), "z": round(wl.z, 1)},
    "notes": notes,
    "furniture_near": furniture[:20],
}
