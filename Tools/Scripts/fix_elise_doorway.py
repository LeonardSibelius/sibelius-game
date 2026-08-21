"""
fix_elise_doorway.py — the hanging stair ball, the empty bedroom, the blocked door.

Walt's two screenshots, 2026-08-21:

  1. Looking down the stairs: bouncing ball still there, pole gone.
     That ball is PowerGrant_AtticKey's MESH. We hid the beacon (the pole) and
     left the sphere sitting in the stair opening — the old Compile shrine was
     always in that void, which is why you only saw it looking down the stairs.
  2. Bedroom doorway blocked, nobody inside. Editor shows a sphere-on-a-pole
     by the door. Elise was placed 140cm in -X of the bed, which is the stair/
     door side, so her capsule is the blocker. The pole is the hidden Compile
     grant, visible in the editor because StandDown only runs at BeginPlay.

THIS SCRIPT

  * Moves Elise INTO the bedroom, toward PP_Bedroom (the room volume), not
    toward the stairs.
  * Hides PowerGrant_Compile in the editor as well as in game, and parks it
    at her feet.
  * Moves PowerGrant_AtticKey next to a bookshelf, at standing height, off
    the stair shaft. Beacon stays off.

Editor-CLOSED:
    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/fix_elise_doorway.py"
"""
import json
import math
import unreal

MAP = "/Game/L_Office_v02"
notes = []
ok = False

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def lab(a):
    try:
        return a.get_actor_label()
    except Exception:
        return ""


def hide_grant(g):
    """Editor AND game: no bouncing ball, no pole."""
    for c in g.get_components_by_class(unreal.StaticMeshComponent):
        c.set_visibility(False)
        c.set_hidden_in_game(True)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for c in g.get_components_by_class(unreal.PointLightComponent):
        c.set_visibility(False)
        c.set_hidden_in_game(True)
    trig = None
    try:
        trig = g.get_editor_property("trigger")
    except Exception:
        pass
    if trig:
        trig.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        try:
            trig.set_editor_property("generate_overlap_events", False)
        except Exception:
            pass


elise = None
compile_grant = None
attic_key = None
bed = None
pp_bedroom = None
bookshelf = None
stairs = None

for a in eas.get_all_level_actors():
    n = lab(a)
    loc = a.get_actor_location()
    if n == "BP_MHC_Elise":
        elise = a
    elif n == "PowerGrant_Compile":
        compile_grant = a
    elif n == "PowerGrant_AtticKey":
        attic_key = a
    elif n == "SM_Bed_A1":
        bed = a
    elif n == "PP_Bedroom":
        pp_bedroom = a
    elif n == "BP_BookShelf_A4":
        bookshelf = a
    elif "stair" in n.lower() and stairs is None:
        stairs = a
    elif bookshelf is None and "bookshelf" in n.lower():
        bookshelf = a

notes.append("found elise=%s compile=%s key=%s bed=%s pp=%s shelf=%s" % (
    bool(elise), bool(compile_grant), bool(attic_key), bool(bed),
    bool(pp_bedroom), bool(bookshelf)))

# ---- Elise into the bedroom, toward the room volume, not the stairs.
if elise and bed:
    bed_loc = bed.get_actor_location()
    target = pp_bedroom.get_actor_location() if pp_bedroom else unreal.Vector(
        bed_loc.x + 200.0, bed_loc.y + 140.0, bed_loc.z)
    # 40% of the way from the bed to the room centre — on the rug, not in the door.
    stand_x = bed_loc.x + 0.40 * (target.x - bed_loc.x)
    stand_y = bed_loc.y + 0.40 * (target.y - bed_loc.y)
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(stand_x, stand_y, bed_loc.z + 120.0),
        unreal.Vector(stand_x, stand_y, bed_loc.z - 80.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True)
    if not hit:
        notes.append("no floor inside the bedroom at (%.0f, %.0f)" % (stand_x, stand_y))
    else:
        floor = hit.to_tuple()[4]
        stand = unreal.Vector(stand_x, stand_y, round(floor.z, 1))
        facing = math.degrees(math.atan2(bed_loc.y - stand.y, bed_loc.x - stand.x))
        elise.set_actor_location_and_rotation(
            stand, unreal.Rotator(roll=0.0, pitch=0.0, yaw=facing), False, True)
        notes.append("Elise moved into bedroom (%.0f, %.0f, %.0f) yaw %.0f" % (
            stand.x, stand.y, stand.z, facing))
        ok = True

        if compile_grant:
            compile_grant.set_actor_location(
                unreal.Vector(stand.x, stand.y, stand.z + 40.0), False, False)
            hide_grant(compile_grant)
            compile_grant.set_editor_property("granted_by_agent", elise)
            notes.append("Compile grant hidden at Elise's feet")

# ---- Attic key: off the stair shaft, next to a bookshelf, standing height.
if attic_key:
    key_loc = attic_key.get_actor_location()
    dest = None
    if bookshelf:
        sl = bookshelf.get_actor_location()
        # A step toward the shrine from the shelf, so it sits in front of the books.
        dx = key_loc.x - sl.x
        dy = key_loc.y - sl.y
        mag = math.sqrt(dx * dx + dy * dy) or 1.0
        dest = unreal.Vector(sl.x + dx / mag * 80.0, sl.y + dy / mag * 80.0, sl.z)
        notes.append("bookshelf %s at (%.0f, %.0f, %.0f)" % (
            lab(bookshelf), sl.x, sl.y, sl.z))
    else:
        # Nudge away from the stair actor if we have one; otherwise +Y into the room.
        dest = unreal.Vector(key_loc.x, key_loc.y + 180.0, key_loc.z)
        notes.append("no bookshelf — nudged +Y")

    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(dest.x, dest.y, dest.z + 200.0),
        unreal.Vector(dest.x, dest.y, dest.z - 400.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True)
    if hit:
        floor = hit.to_tuple()[4]
        dest = unreal.Vector(dest.x, dest.y, round(floor.z, 1) + 90.0)
    attic_key.set_actor_location(dest, False, False)
    attic_key.set_editor_property("show_beacon", False)
    # Keep the little sphere; kill the pole in the editor too.
    beacon = None
    try:
        beacon = attic_key.get_editor_property("beacon_mesh")
    except Exception:
        pass
    if beacon:
        beacon.set_visibility(False)
        beacon.set_hidden_in_game(True)
    notes.append("AtticKey moved to (%.0f, %.0f, %.0f)" % (dest.x, dest.y, dest.z))
    ok = True
else:
    notes.append("PowerGrant_AtticKey not in the level")

if ok:
    les.save_current_level()
    notes.append("level saved")

payload = {
    "ok": ok,
    "notes": notes,
    "elise": [round(elise.get_actor_location().x),
              round(elise.get_actor_location().y),
              round(elise.get_actor_location().z)] if elise else None,
    "attic_key": [round(attic_key.get_actor_location().x),
                  round(attic_key.get_actor_location().y),
                  round(attic_key.get_actor_location().z)] if attic_key else None,
    "compile": [round(compile_grant.get_actor_location().x),
                round(compile_grant.get_actor_location().y),
                round(compile_grant.get_actor_location().z)] if compile_grant else None,
}
text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "fix_elise_doorway.json"
with open(out, "w", encoding="utf-8") as f:
    f.write(text)
print(text)
