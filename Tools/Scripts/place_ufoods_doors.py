# place_ufoods_doors.py - the door into uFoods, and the way back out.
#
# *** RUN IT TWICE, ONCE PER LEVEL, EACH TIME WITH THAT LEVEL ALREADY OPEN ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_ufoods_doors.py"
#
#   with L_City open    -> SELECT THE UFOODS STOREFRONT FIRST. Places the door into the
#                          shop, plus the PlayerStart he returns to on the street.
#   with L_uFoods open  -> places the return door, and tags the PlayerStart already
#                          standing by the trolleys so the city door can aim at it.
#
# It never changes levels itself - Python that calls load_level fatals this editor.
# This is place_cafe_doors.py's recipe, which built the deli door, with one change
# explained below.
#
# ===========================================================================
# WHY THE SELECTION, INSTEAD OF A HARDCODED COORDINATE.
#
# place_cafe_doors.py carried DELI_DOOR = Vector(2374.075928, 1475.929565, 0.0), a number
# somebody found by hand. That worked, and then it went wrong in a way worth not
# repeating: Walt dragged the deli door to where it actually belonged, and a later re-run
# put it back. The script now inherits a hand-moved door's transform instead.
#
# uFoods has no such number and I am not going to invent one - a door guessed into the
# wrong spot on a storefront is a door inside a wall. So: SELECT the uFoods building in
# the editor, and this uses what you picked. You can see the city; I am working from a
# bounding box.
#
# And once a door exists, this script LEAVES IT ALONE. Hand placement wins, always.
#
# ---------------------------------------------------------------------------
# docs/SPACEPORT_PLAN.md Phase E - the supply run.

import json
import math
import traceback

import unreal

CITY = "/Game/Maps/L_City"
SHOP = "/Game/Maps/L_uFoods"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/place_ufoods_doors.json"

DOOR_TAG = "uFoodsDoor"        # actor tag, so a re-run can FIND the door and not re-make it
RETURN_TAG = "uFoodsReturn"    # the return door in the shop
ARRIVE_IN = "uFoodsEntrance"   # PlayerStartTag inside the shop
ARRIVE_OUT = "uFoodsStreet"    # PlayerStartTag back on the street

STEP_OUT = 150.0      # cm from the storefront toward the street, where the door trigger sits
STAND_OUT = 260.0     # cm from the storefront, where he lands coming back out
STAND_H = 100.0       # PlayerStart location is the capsule CENTRE, not the feet
TRIGGER_H = 100.0

# Words that might name the uFoods building, used only to help you find it when nothing
# is selected. A guess that helps you search is fine; a guess that places a door is not.
HINTS = ("food", "market", "grocer", "shop", "store", "super")


CUBE = "/Game/LevelPrototyping/Meshes/SM_Cube"


def setup_body(door):
    """Give the door something the interactor's TRACE can hit.

    ACathedralDoor is reached through IInteractable, and UInteractorComponent finds it by
    a 450 cm line trace from the camera. A trace needs collision. A door spawned with no
    static mesh has none, so it is invisible to the trace and E does nothing at all -
    which reads as a broken level, not as a missing mesh.

    The first version of this script skipped this and created exactly that door.
    place_cafe_doors.py had it right and I did not copy it.

    Hidden in game (the shop already has doors modelled) and QUERY_ONLY, so the trace
    finds it and the player walks straight through.

    THIS IS SAFE TO RE-RUN ON A HAND-MOVED DOOR: it touches the mesh, the scale and the
    collision - never the location or rotation. Where a door IS stays the human's.
    """
    out = {}
    mesh = door.get_component_by_class(unreal.StaticMeshComponent)
    out["had_component"] = mesh is not None
    if not mesh:
        return out
    if not mesh.get_editor_property("static_mesh"):
        cube = unreal.EditorAssetLibrary.load_asset(CUBE)
        if cube:
            mesh.set_editor_property("static_mesh", cube)
    # A metre square and 2.2 m tall - a person-sized volume in the doorway.
    door.set_actor_scale3d(unreal.Vector(1.0, 1.0, 2.2))
    mesh.set_editor_property("hidden_in_game", True)
    mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    got = mesh.get_editor_property("static_mesh")
    out["mesh"] = got.get_name() if got else None
    out["hidden_in_game"] = bool(mesh.get_editor_property("hidden_in_game"))
    return out


def find_by_tag(eas, tag):
    for a in eas.get_all_level_actors():
        try:
            if tag in [str(t) for t in a.get_editor_property("tags")]:
                return a
        except Exception:
            pass
    return None


r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level

    door_cls = unreal.load_class(None, "/Script/SibeliusGame.CathedralDoor")
    if not door_cls:
        raise Exception("no ACathedralDoor class - is the editor running our module?")

    # ====================================================== L_CITY: the way in
    if open_level == CITY:
        r["side"] = "city"

        existing = find_by_tag(eas, DOOR_TAG)
        if existing:
            p = existing.get_actor_location()
            r["door"] = "ALREADY EXISTS - not moved"
            r["door_at"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
            r["door_body"] = setup_body(existing)
            r["note"] = ("Hand placement wins: the LOCATION was left exactly as it is. "
                         "Only the collision body was re-checked, because a door the "
                         "interactor's trace cannot hit does nothing at all.")
        else:
            sel = eas.get_selected_level_actors()
            r["selected_count"] = len(sel)

            if len(sel) != 1:
                # Nothing (or too much) picked - help, do not guess.
                cands = []
                for a in eas.get_all_level_actors():
                    label = a.get_actor_label().lower()
                    if any(h in label for h in HINTS):
                        p = a.get_actor_location()
                        cands.append({"label": a.get_actor_label(),
                                      "at": [round(p.x, 1), round(p.y, 1), round(p.z, 1)]})
                r["candidates"] = cands[:25]
                r["REFUSED"] = ("Select EXACTLY ONE actor - the uFoods storefront - then "
                                "run this again. Candidates above are only a name search; "
                                "the storefront may well not be named for what it sells.")
            else:
                anchor = sel[0]
                ap = anchor.get_actor_location()
                r["anchor"] = anchor.get_actor_label()
                r["anchor_at"] = [round(ap.x, 1), round(ap.y, 1), round(ap.z, 1)]

                # Step from the storefront toward the middle of the city - that is the
                # street side. Same rule the deli door used, and for the same reason:
                # shopfronts face the plaza.
                dx, dy = -ap.x, -ap.y
                length = math.hypot(dx, dy)
                if length < 1.0:
                    raise Exception("the storefront is at the world origin; cannot tell "
                                    "which way is out. Place the door by hand.")
                ux, uy = dx / length, dy / length

                where = unreal.Vector(ap.x + ux * STEP_OUT,
                                      ap.y + uy * STEP_OUT,
                                      ap.z + TRIGGER_H)
                door = eas.spawn_actor_from_class(door_cls, where)
                door.set_actor_label("uFoods_Door")
                door.set_editor_property("tags", [unreal.Name(DOOR_TAG)])
                door.set_editor_property("TargetLevelName", unreal.Name(SHOP))
                door.set_editor_property("ArrivalTag", unreal.Name(ARRIVE_IN))
                door.set_editor_property("bInteractive", True)
                door.set_editor_property("bRequireBattleToll", False)
                door.set_editor_property("bRequireGenerateUse", False)
                door.set_editor_property("PromptText",
                                         unreal.Text("Enter uFoods [E]"))
                r["door"] = "created"
                r["door_at"] = [round(where.x, 1), round(where.y, 1), round(where.z, 1)]
                # Read back rather than assume - a property that silently failed to set
                # is a door that goes nowhere and looks like a broken level.
                r["door_target"] = str(door.get_editor_property("TargetLevelName"))
                r["door_arrival"] = str(door.get_editor_property("ArrivalTag"))
                r["door_body"] = setup_body(door)

        # ---- where he lands coming back OUT of the shop -------------------
        if not find_by_tag(eas, ARRIVE_OUT):
            base = None
            d = find_by_tag(eas, DOOR_TAG)
            if d:
                base = d.get_actor_location()
            if base:
                dx, dy = -base.x, -base.y
                length = math.hypot(dx, dy) or 1.0
                sw = unreal.Vector(base.x + dx / length * (STAND_OUT - STEP_OUT),
                                   base.y + dy / length * (STAND_OUT - STEP_OUT),
                                   base.z - TRIGGER_H + STAND_H)
                # Face him back at the shop he just left.
                yaw = math.degrees(math.atan2(base.y - sw.y, base.x - sw.x))
                # unreal.Rotator is (ROLL, PITCH, YAW) - not C++'s (Pitch, Yaw, Roll).
                # Getting this backwards spawned Leonard staring at the floor in L_uFoods.
                ps = eas.spawn_actor_from_class(unreal.PlayerStart, sw,
                                                unreal.Rotator(0.0, 0.0, yaw))
                ps.set_actor_label("uFoods_Street_Start")
                ps.set_editor_property("tags", [unreal.Name(ARRIVE_OUT)])
                # player_start_tag is what the GameMode matches - NOT the actor tag.
                ps.set_editor_property("player_start_tag", unreal.Name(ARRIVE_OUT))
                r["street_start"] = [round(sw.x, 1), round(sw.y, 1), round(sw.z, 1)]
                r["street_start_tag"] = str(ps.get_editor_property("player_start_tag"))
        else:
            r["street_start"] = "already exists - left untouched"

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Now open L_uFoods by hand and run this again to make the way back.")

    # ====================================================== L_UFOODS: the way out
    elif open_level == SHOP:
        r["side"] = "shop"

        # Tag the PlayerStart that move_ufoods_playerstart.py already positioned by the
        # trolleys. It exists and it is where Walt wants it; all it lacks is a name for
        # the city door to aim at.
        starts = [a for a in eas.get_all_level_actors()
                  if isinstance(a, unreal.PlayerStart)]
        r["player_starts"] = len(starts)
        if starts:
            ps = starts[0]
            ps.set_editor_property("tags", [unreal.Name(ARRIVE_IN)])
            ps.set_editor_property("player_start_tag", unreal.Name(ARRIVE_IN))
            p = ps.get_actor_location()
            r["entrance_start"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
            r["entrance_start_tag"] = str(ps.get_editor_property("player_start_tag"))
            if len(starts) > 1:
                r["NOTE"] = ("More than one PlayerStart here - only the first was "
                             "tagged. Delete the spares.")
        else:
            r["WARNING"] = "no PlayerStart in L_uFoods - run move_ufoods_playerstart.py"

        # ---- the way back to the street ----------------------------------
        existing = find_by_tag(eas, RETURN_TAG)
        if existing:
            p = existing.get_actor_location()
            r["return_door"] = "ALREADY EXISTS - not moved"
            r["return_door_at"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
            r["return_door_body"] = setup_body(existing)
        elif starts:
            # Put it where he arrives, so the way out is where he came in. It is a
            # trigger, not a visible door, and the shop already has doors modelled.
            p = starts[0].get_actor_location()
            f = starts[0].get_actor_rotation().get_forward_vector()
            # A pace BEHIND the arrival point: he turns round and it is there.
            where = unreal.Vector(p.x - f.x * 120.0, p.y - f.y * 120.0, p.z)
            door = eas.spawn_actor_from_class(door_cls, where)
            door.set_actor_label("uFoods_Return_Door")
            door.set_editor_property("tags", [unreal.Name(RETURN_TAG)])
            door.set_editor_property("TargetLevelName", unreal.Name(CITY))
            door.set_editor_property("ArrivalTag", unreal.Name(ARRIVE_OUT))
            door.set_editor_property("bInteractive", True)
            door.set_editor_property("bRequireBattleToll", False)
            door.set_editor_property("bRequireGenerateUse", False)
            door.set_editor_property("PromptText", unreal.Text("Back to the street [E]"))
            r["return_door"] = "created"
            r["return_door_at"] = [round(where.x, 1), round(where.y, 1), round(where.z, 1)]
            r["return_target"] = str(door.get_editor_property("TargetLevelName"))
            r["return_arrival"] = str(door.get_editor_property("ArrivalTag"))
            r["return_door_body"] = setup_body(door)

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Play L_City, walk to uFoods, press E. You should arrive by the "
                     "trolleys; turn round and E takes you back to the street.")

    else:
        r["refused"] = ("Open %s or %s by hand and run this again (currently %s)."
                        % (CITY, SHOP, open_level))

    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###UFOODSDOOR### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
