"""
place_elise_compile.py — Elise hands over COMPILE in the bedroom; the library
sphere becomes the attic key; the stair-hanging pole stands down.

Walt, 2026-08-21: looking down the stairs he saw a hanging ball. That is
PowerGrant_Compile's room-height beacon (2.4 m cyan cylinder) dropping into the
stair void. He asked to:

  1. Put Elise in the bedroom. She gives COMPILE the way Kaia gives REFACTOR.
  2. Keep a little glowing sphere in the library alcove, only for the attic key.
  3. Get rid of the hanging ball.

WHAT THIS DOES

  * Places BP_MHC_Elise next to the upstairs bed, dancing (the scan will not
    adopt her without a Morro _MH dance — Aisling's lesson).
  * Spawns a hidden PowerGrant_Compile at her feet, GrantedByAgent = Elise.
    BeginPlay StandDown hides it. [E] on her opens the same trial as before.
  * Converts the existing library PowerGrant_Compile into PowerGrant_AtticKey:
    no power, no slot trial, no room-height beacon, walk-in spends 8 books and
    mints the attic Key. The little sphere stays. The pole does not.

Idempotent. Saves the level.

Editor-CLOSED:
    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_elise_compile.py"
"""
import json
import math
import unreal

MAP = "/Game/L_Office_v02"
ELISE_CLASS = "/Game/MetaHumans/MHC_Elise/BP_MHC_Elise.BP_MHC_Elise_C"
DANCE = "/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_10_MH.Anim_Mid_Rhythm_Dance_10_MH"
ELISE_LABEL = "BP_MHC_Elise"
COMPILE_LABEL = "PowerGrant_Compile"
KEY_LABEL = "PowerGrant_AtticKey"
KEY_GRANT = "Attic.Key"
BOOK_COST = 8
MOVE_EXISTING = False  # she is on the bedroom floor; a hand nudge should stick

notes = []
ok = False

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def label_of(a):
    try:
        return a.get_actor_label()
    except Exception:
        return ""


elise = None
compile_grant = None
key_grant = None
beds = []

for a in eas.get_all_level_actors():
    lab = label_of(a)
    cls = a.get_class().get_name()
    if lab == ELISE_LABEL:
        elise = a
    if cls == "PowerGrant":
        if lab == COMPILE_LABEL:
            compile_grant = a
        elif lab == KEY_LABEL:
            key_grant = a
    low = lab.lower()
    if "bed" in low and "table" not in low and "bedtable" not in low:
        loc = a.get_actor_location()
        if 280.0 <= loc.z <= 520.0:
            beds.append((lab, loc, a))

notes.append("upstairs beds: %s" % [
    "%s (%.0f, %.0f, %.0f)" % (n, p.x, p.y, p.z) for n, p, _ in beds])

anim = unreal.load_object(None, DANCE)
cls = unreal.load_class(None, ELISE_CLASS)
grant_cls = unreal.load_class(None, "/Script/SibeliusGame.PowerGrant")

if cls is None:
    notes.append("could not load %s" % ELISE_CLASS)
elif anim is None:
    notes.append("could not load dance %s — is Characters/Retargeting present?" % DANCE)
elif not beds:
    notes.append("no upstairs bed found — cannot place Elise")
else:
    # Prefer a proper bed over a child's, then the one furthest from the library
    # alcove so she is in the bedroom, not standing on the study nook.
    library = unreal.Vector(-2225.0, 10423.0, 420.0)
    def bed_score(item):
        name, loc, _ = item
        child = 1 if "child" in name.lower() else 0
        d = loc - library
        dist = math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z)
        return (child, -dist)
    beds.sort(key=bed_score)
    bed_name, bed_loc, _ = beds[0]
    notes.append("bedroom bed = %s at (%.0f, %.0f, %.0f)" % (
        bed_name, bed_loc.x, bed_loc.y, bed_loc.z))

    # Stand off the long side of the bed, toward -X. Trace from just above the
    # mattress, not from the storey above — the first run started at Z=600 and
    # landed Elise on the attic slab (Z=580) over the bedroom.
    spot = unreal.Vector(bed_loc.x - 140.0, bed_loc.y, bed_loc.z)
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(spot.x, spot.y, bed_loc.z + 120.0),
        unreal.Vector(spot.x, spot.y, bed_loc.z - 80.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True)
    if not hit:
        notes.append("no floor beside the bed at (%.0f, %.0f)" % (spot.x, spot.y))
    else:
        floor = hit.to_tuple()[4]
        stand = unreal.Vector(spot.x, spot.y, round(floor.z, 1))
        facing = math.degrees(math.atan2(bed_loc.y - stand.y, bed_loc.x - stand.x))
        face = unreal.Rotator(roll=0.0, pitch=0.0, yaw=facing)

        ignore = [elise] if elise else []
        blockers = unreal.SystemLibrary.sphere_overlap_actors(
            world, unreal.Vector(stand.x, stand.y, stand.z + 95.0), 55.0,
            [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1,
             unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2], None, ignore) or []
        if blockers:
            notes.append("bedside spot blocked by %s — trying the other side"
                         % [b.get_actor_label() for b in blockers][:4])
            spot = unreal.Vector(bed_loc.x + 140.0, bed_loc.y, bed_loc.z)
            hit2 = unreal.SystemLibrary.line_trace_single(
                world,
                unreal.Vector(spot.x, spot.y, bed_loc.z + 120.0),
                unreal.Vector(spot.x, spot.y, bed_loc.z - 80.0),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                unreal.DrawDebugTrace.NONE, True)
            if hit2:
                floor = hit2.to_tuple()[4]
                stand = unreal.Vector(spot.x, spot.y, round(floor.z, 1))
                facing = math.degrees(math.atan2(bed_loc.y - stand.y, bed_loc.x - stand.x))
                face = unreal.Rotator(roll=0.0, pitch=0.0, yaw=facing)
                blockers = []

        if elise is None:
            elise = eas.spawn_actor_from_class(cls, stand, face)
            if elise:
                elise.set_actor_label(ELISE_LABEL)
                notes.append("spawned %s at (%.0f, %.0f, %.0f) yaw %.0f" % (
                    ELISE_LABEL, stand.x, stand.y, stand.z, facing))
            else:
                notes.append("spawn of %s returned None" % ELISE_LABEL)
        elif MOVE_EXISTING:
            elise.set_actor_location_and_rotation(stand, face, False, True)
            notes.append("moved existing Elise")
        else:
            loc = elise.get_actor_location()
            notes.append("Elise already at (%.0f, %.0f, %.0f) — left where she is"
                         % (loc.x, loc.y, loc.z))

        if elise:
            body = None
            for c in elise.get_components_by_class(unreal.SkeletalMeshComponent):
                if c.get_name() == "Body":
                    body = c
                    break
            if body is None:
                notes.append("no Body mesh on Elise — the scan will not adopt her")
            else:
                body.set_editor_property("animation_mode",
                                         unreal.AnimationMode.ANIMATION_SINGLE_NODE)
                data = unreal.SingleAnimationPlayData()
                data.set_editor_property("anim_to_play", anim)
                for key in ("saved_looping", "looping"):
                    try:
                        data.set_editor_property(key, True)
                        break
                    except Exception:
                        continue
                for key in ("saved_playing", "playing"):
                    try:
                        data.set_editor_property(key, True)
                        break
                    except Exception:
                        continue
                body.set_editor_property("animation_data", data)
                notes.append("Elise dancing %s" % anim.get_name())

            # Library alcove FIRST. The actor currently labelled PowerGrant_Compile
            # lives there. Convert it to the attic key BEFORE spawning Elise's grant,
            # or we would bind the hanging pole to her and lose the alcove sphere.
            alcove = key_grant
            if alcove is None:
                for a in eas.get_all_level_actors():
                    if a.get_class().get_name() != "PowerGrant":
                        continue
                    loc = a.get_actor_location()
                    lab = label_of(a)
                    at_library = abs(loc.x + 2225.0) < 80 and abs(loc.y - 10423.0) < 80
                    if lab == COMPILE_LABEL or at_library:
                        alcove = a
                        break
            if alcove is None:
                notes.append("could not find the library shrine to convert to the attic key")
            else:
                alcove.set_actor_label(KEY_LABEL)
                alcove.set_editor_property("grants_power", False)
                set_key = False
                for key in ("grants_key", "b_grants_key"):
                    try:
                        alcove.set_editor_property(key, True)
                        set_key = True
                        break
                    except Exception:
                        continue
                if not set_key:
                    notes.append("COULD NOT SET grants_key — rebuild the editor target first")
                alcove.set_editor_property("slot_trial", False)
                alcove.set_editor_property("show_beacon", False)
                alcove.set_editor_property("summon_refusers_on_trial", False)
                alcove.set_editor_property("sauce_reward", 0)
                alcove.set_editor_property("grant_key", KEY_GRANT)
                try:
                    alcove.set_editor_property("book_cost", BOOK_COST)
                except Exception as e:
                    notes.append("could not set book_cost: %s" % e)
                try:
                    alcove.set_editor_property("granted_by_agent", None)
                except Exception:
                    pass
                notes.append("%s is now the attic key (8 books, no pole)" % KEY_LABEL)

            # COMPILE grant, hidden at her feet, asked for with E.
            hidden = None
            for a in eas.get_all_level_actors():
                if label_of(a) == COMPILE_LABEL and a.get_class().get_name() == "PowerGrant":
                    hidden = a
                    break
            if hidden is None and grant_cls:
                eloc = elise.get_actor_location()
                hidden = eas.spawn_actor_from_class(
                    grant_cls, unreal.Vector(eloc.x, eloc.y, eloc.z + 80.0),
                    unreal.Rotator(0.0, 0.0, 0.0))
                if hidden:
                    hidden.set_actor_label(COMPILE_LABEL)
                    notes.append("spawned %s next to Elise" % COMPILE_LABEL)
                else:
                    notes.append("could not spawn PowerGrant_Compile")
            if hidden:
                eloc = elise.get_actor_location()
                hidden.set_actor_location(
                    unreal.Vector(eloc.x, eloc.y, eloc.z + 80.0), False, False)
                try:
                    hidden.set_editor_property("power", unreal.PowerVerb.COMPILE)
                except Exception:
                    try:
                        hidden.set_editor_property("power", 2)
                    except Exception as e:
                        notes.append("could not set Compile verb: %s" % e)
                hidden.set_editor_property("grants_power", True)
                hidden.set_editor_property("slot_trial", True)
                hidden.set_editor_property("granted_by_agent", elise)
                notes.append("%s.GrantedByAgent -> %s" % (COMPILE_LABEL, ELISE_LABEL))

            les.save_current_level()
            ok = True
            notes.append("level saved")

final = []
for a in eas.get_all_level_actors():
    try:
        if a.get_class().get_name() != "PowerGrant":
            continue
        g = a.get_editor_property("granted_by_agent")
        final.append({
            "grant": a.get_actor_label(),
            "power": str(a.get_editor_property("power")),
            "grants_power": str(a.get_editor_property("grants_power")),
            "granted_by_agent": g.get_actor_label() if g else None,
            "loc": [round(a.get_actor_location().x),
                    round(a.get_actor_location().y),
                    round(a.get_actor_location().z)],
        })
    except Exception:
        continue

payload = {
    "ok": ok,
    "map": MAP,
    "notes": notes,
    "all_grants_after": final,
    "how_to_play": "E on Elise in the bedroom for COMPILE. Walk into the little "
                   "library sphere with 8 books for the attic key. No pole in the stairwell.",
}
text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "place_elise_compile.json"
try:
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[place_elise_compile] wrote " + out)
except Exception as e:
    unreal.log_error("[place_elise_compile] could not write %s: %s" % (out, e))
print(text)
