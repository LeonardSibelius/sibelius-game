# move_ufoods_playerstart.py - arrive at the door, not in the confectionery.
#
# *** OPEN /Game/Maps/L_uFoods FIRST, THEN RUN THIS ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/move_ufoods_playerstart.py"
#
# It refuses to do anything unless L_uFoods is already the open level, and it never
# changes levels itself - Python that calls load_level fatals this editor.
#
# ===========================================================================
# WHY.  make_ufoods_from_showcase.py put the PlayerStart at the middle of the geometry,
# on the reasoning that the centre of a supermarket is an aisle. It is - and Walt landed
# in one, nose to a shelf of chocolate bars, which is a fine place to stand and a
# terrible place to ARRIVE. He is supposed to be walking in off the street.
#
# FINDING THE DOOR WITHOUT BEING TOLD WHERE IT IS.
#
# The pack has no SM_Entrance and no SM_Door. It has something better: ten SM_Cart_01,
# and a supermarket's trolley bay is at the entrance by definition - that is the whole
# point of a trolley bay. So the carts ARE the door, as far as this script is concerned.
#
# Standing exactly on them would put Leonard inside a shopping trolley, so the spot is
# the cart centroid stepped ENTRY_STEP toward the middle of the room: through the doors
# and clear of them, which is where a person actually is a second after walking in.
#
# Then face him at the room's centre, because arriving looking at a wall is the same
# mistake in a different coat. A PlayerStart's ROTATION is where the pawn looks when it
# spawns, so this is the one chance to aim him down the aisles.
#
# Re-running is safe: it recomputes from the same geometry and lands in the same place.
# If it is wrong, drag the PlayerStart by hand and DO NOT run this again - the level is
# ours now, and a script that overwrites hand-placement is the place_cafe_doors mistake.

import json
import math
import traceback

import unreal

LEVEL = "/Game/Maps/L_uFoods"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/move_ufoods_playerstart.json"

# ==========================================================================
# THE DIAL. How far in from the trolley bay, in centimetres, measured toward the middle
# of the shop. Change this one number and re-run; nothing else needs touching.
#
#   larger   deeper into the shop        250 was too far in (Walt, 3 Sep)
#   150      better, still too far in
#    50      current - a metre back from that, among the trolleys
#     0      standing at the middle of the trolley bay
#   negative BEHIND the bay, toward the doors and the street
#
# 100 cm is roughly one pace, so the number is readable as paces from the trolleys.
ENTRY_STEP = 50.0

# A PlayerStart's location is its capsule CENTRE, not its feet. This is the mistake that
# left Nyra hanging a metre above the pavement outside the deli on 3 Sep.
CAPSULE_LIFT = 100.0

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level

    if open_level != LEVEL:
        r["refused"] = ("Not the open level. Open %s by hand and run this again."
                        % LEVEL)
    else:
        actors = eas.get_all_level_actors()

        cart_x, cart_y = [], []
        all_x, all_y, all_z = [], [], []

        for a in actors:
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                m = c.get_editor_property("static_mesh")
                if not m:
                    continue
                path = m.get_path_name()
                if "/Poly_Supermarket_01/" not in path:
                    continue
                t = c.get_world_transform().translation
                all_x.append(t.x); all_y.append(t.y); all_z.append(t.z)
                # "SM_Cart_01" and friends - the trolley bay. Matched on the mesh name so
                # a re-labelled actor still counts.
                name = path.split(".")[-1]
                if name.startswith("SM_Cart_"):
                    cart_x.append(t.x); cart_y.append(t.y)

        r["pack_meshes"] = len(all_x)
        r["carts_found"] = len(cart_x)

        if not all_x:
            raise Exception("no Poly_Supermarket_01 meshes - is this really L_uFoods?")
        if not cart_x:
            raise Exception("no SM_Cart_* found; cannot locate the entrance this way. "
                            "Drag the PlayerStart to the door by hand instead.")

        room_cx = (min(all_x) + max(all_x)) / 2.0
        room_cy = (min(all_y) + max(all_y)) / 2.0
        floor_z = min(all_z)

        bay_x = sum(cart_x) / len(cart_x)
        bay_y = sum(cart_y) / len(cart_y)

        # Step from the trolley bay toward the middle of the shop.
        dx, dy = room_cx - bay_x, room_cy - bay_y
        dist = math.hypot(dx, dy)
        if dist < 1.0:
            raise Exception("the trolley bay IS the room centre - geometry is not what "
                            "this script assumes. Place the PlayerStart by hand.")
        ux, uy = dx / dist, dy / dist

        spot = unreal.Vector(bay_x + ux * ENTRY_STEP,
                             bay_y + uy * ENTRY_STEP,
                             floor_z + CAPSULE_LIFT)
        # Look down the shop.
        yaw = math.degrees(math.atan2(uy, ux))

        # *** unreal.Rotator IS (ROLL, PITCH, YAW) - NOT (Pitch, Yaw, Roll) ***
        #
        # C++ FRotator takes (Pitch, Yaw, Roll). The Python binding does NOT: its
        # constructor order matches the struct's property order, roll first. Writing
        # Rotator(0.0, yaw, 0.0) - the C++ habit - puts the yaw into PITCH.
        #
        # First run of this script did exactly that with yaw = -74.4, and Leonard spawned
        # pitched 74 degrees down: "Leonard is staring at floor. he needs to look up and
        # left". Both halves of that sentence are this one argument in the wrong slot -
        # the pitch was the floor, and the yaw that never got applied was the left.
        facing = unreal.Rotator(0.0, 0.0, yaw)

        r["room_centre"] = [round(room_cx, 1), round(room_cy, 1), round(floor_z, 1)]
        r["trolley_bay"] = [round(bay_x, 1), round(bay_y, 1)]
        r["bay_to_centre_cm"] = round(dist, 1)

        starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
        r["player_starts_found"] = len(starts)

        if not starts:
            s = eas.spawn_actor_from_class(unreal.PlayerStart, spot)
            s.set_actor_label("uFoods_PlayerStart")
            s.set_actor_rotation(facing, False)
            r["action"] = "spawned (there was none)"
        else:
            s = starts[0]
            old = s.get_actor_location()
            r["moved_from"] = [round(old.x, 1), round(old.y, 1), round(old.z, 1)]
            s.set_actor_location(spot, False, False)
            s.set_actor_rotation(facing, False)
            r["action"] = "moved"
            if len(starts) > 1:
                r["NOTE"] = ("More than one PlayerStart in this level - only the first "
                             "was moved. Delete the spares or the game may pick one.")

        r["moved_to"] = [round(spot.x, 1), round(spot.y, 1), round(spot.z, 1)]
        r["facing_yaw"] = round(yaw, 1)

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Play L_uFoods. You should be just inside the doors by the "
                     "trolleys, looking down the shop. If it is wrong, DRAG IT BY HAND "
                     "and do not run this again - hand placement wins.")

        del actors, starts
        unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###UFOODS_START### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
