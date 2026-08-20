"""
build_slot_cabinet.py — the cathedral apse: stand the altar back up, and leave the
machine alone.

WHAT THIS USED TO DO, AND WHY IT WAS WRONG
------------------------------------------
The original version cleared the apse and stood a PLACEHOLDER cabinet there — three
engine cubes (base / body / screen) — because ASlotCabinet did not exist yet. To make
room it destroyed the altar:

    if a.get_actor_label() == "Altar_Main" or (TAG in a.tags):
        destroy(a)

Both halves have since gone stale, and the altar line did real damage.

  * THE CUBES ARE OBSOLETE. L_Cathedral now holds a real ASlotCabinet actor at
    (3400, 0, 60) carrying its own mesh, and the three SlotCab_* cubes are gone. Building
    them again would plant three stray cubes standing inside the real machine. So this
    script no longer builds them, and it refuses to if the real actor is present.

  * THE ALTAR IS GONE FROM THE LEVEL. Not "would be deleted on the next run" — already
    deleted, in some earlier run, and nothing put it back. So simply removing the delete
    would have fixed nothing: there is no altar left to keep. This restores it.

WHY THE ALTAR MATTERS: a slot machine standing on a cathedral altar is the thesis of the
whole ending in one silhouette — docs/SPINE.md Move 4, and Walt's line, "I served that
floor for years and never built the machine. I built this one." An empty apse with a box
in it says none of that. See docs/MARQUEE_SPEC.md for the rest of the dressing.

ON THE SCALE — READ THIS BEFORE TRUSTING THE DEFAULT
-----------------------------------------------------
SM_Altar_Main_Marble is 1610 x 1609 x 1390 cm at scale 1: nearly CUBIC, and 16 metres
wide. That is an altar ensemble (altar plus reredos/canopy), not a table — which means it
cannot be used as a plinth for the machine to stand on. At any uniform scale its width and
its height stay roughly equal, so "wide enough to carry a 160cm cabinet" and "low enough to
stand a cabinet on" cannot both be true. Scaled to a 60cm-high plinth it is only 70cm
across and the cabinet overhangs it on every side.

So it goes BEHIND the machine as a backdrop, not underneath it. ALTAR_SCALE below is a
starting point measured against the cabinet's footprint and the end of the nave (the level
runs out at X=3690) — not an art direction. Drag it in the editor and re-save; this script
will not move an altar that already exists.

Editor-CLOSED:
    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_slot_cabinet.py"
"""
import json
import unreal

ALTAR_MESH = ("/Game/UltimateGothicCathedralChurch/Mesh/"
              "SM_Altar_Main_Marble_00001__1341.SM_Altar_Main_Marble_00001__1341")
ALTAR_LABEL = "Altar_Main"
TAG = "SlotCabinet"

APSE_X   = 3400.0     # where the real ASlotCabinet stands
APSE_Y   = 0.0
FACE_YAW = 180.0      # faces back down the nave toward the entrance

# Behind the machine (cabinet front face is at -X), clear of its 160cm footprint and
# short of the nave's end at X=3690.
ALTAR_X     = 3580.0
ALTAR_SCALE = 0.12    # -> roughly 193 x 193 x 167 cm. See the note above.

MAP = "/Game/Maps/L_Cathedral"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)

notes = []
saved = False

actors = list(eas.get_all_level_actors())

# What is already at the apse?
altar = None
cabinet = None
stray_cubes = []
for a in actors:
    try:
        label = a.get_actor_label()
        cls = a.get_class().get_name()
    except Exception:
        continue
    if label == ALTAR_LABEL:
        altar = a
    elif cls == "SlotCabinet":
        cabinet = a
    elif label.startswith("SlotCab_"):
        stray_cubes.append(a)

# THE CUBES. Never build them over a real cabinet. If a previous run left some behind and
# the real actor is now present, they are debris standing inside it — clear those.
if cabinet is not None:
    loc = cabinet.get_actor_location()
    notes.append("real ASlotCabinet '%s' at (%.0f, %.0f, %.0f) — placeholder cubes NOT built"
                 % (cabinet.get_actor_label(), loc.x, loc.y, loc.z))
    for c in stray_cubes:
        lbl = c.get_actor_label()
        eas.destroy_actor(c)
        notes.append("removed leftover placeholder '%s' (it stood inside the real cabinet)" % lbl)
else:
    notes.append("NO ASlotCabinet in %s — nothing to stand at the apse. Place one (it "
                 "carries its own mesh); this script no longer builds cube stand-ins." % MAP)

# THE ALTAR. Restore it if it is missing; never move or delete one that is already there.
if altar is not None:
    loc = altar.get_actor_location()
    s = altar.get_actor_scale3d()
    notes.append("%s already present at (%.0f, %.0f, %.0f) scale %.3f — left untouched"
                 % (ALTAR_LABEL, loc.x, loc.y, loc.z, s.x))
else:
    mesh = unreal.load_object(None, ALTAR_MESH)
    if mesh is None:
        notes.append("could not load %s — is UltimateGothicCathedralChurch installed? "
                     "(it is a gitignored vendor pack; see docs/VENDOR_PACKS.md)" % ALTAR_MESH)
    else:
        # SPAWN THE CLASS AND ASSIGN THE MESH — never spawn_actor_from_object here.
        # That call goes through the editor's actor-factory path, which reaches into
        # EditorFramework's mode/selection machinery; headless there is nothing behind it
        # and the commandlet dies on an access violation reading 0x40, with no Python
        # traceback to explain it. spawn_actor_from_class is the path the dancer
        # placements already use, and it survives -run=pythonscript.
        altar = eas.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(ALTAR_X, APSE_Y, 0.0),
            unreal.Rotator(0.0, 0.0, FACE_YAW))
        if altar is None:
            notes.append("spawn_actor_from_class returned nothing for the altar")
        else:
            comp = altar.get_component_by_class(unreal.StaticMeshComponent)
            if comp:
                comp.set_editor_property("static_mesh", mesh)
            altar.set_actor_label(ALTAR_LABEL)
            altar.set_actor_scale3d(unreal.Vector(ALTAR_SCALE, ALTAR_SCALE, ALTAR_SCALE))
            o, e = altar.get_actor_bounds(False)
            notes.append("restored %s at (%.0f, %.0f, 0) scale %.3f -> %.0f x %.0f x %.0f cm, "
                         "top at Z=%.0f"
                         % (ALTAR_LABEL, ALTAR_X, APSE_Y, ALTAR_SCALE,
                            e.x * 2, e.y * 2, e.z * 2, o.z + e.z))

if notes:
    les.save_current_level()
    saved = True
    notes.append("level saved")

payload = {
    "map": MAP,
    "saved": saved,
    "notes": notes,
    "altar_scale_is_a_starting_point": "SM_Altar_Main_Marble is nearly cubic (1610 x 1609 "
        "x 1390 at scale 1), so it is a backdrop and not a plinth — the machine cannot "
        "stand on it at any uniform scale. Nudge it in the editor; re-running will not "
        "move an altar that exists.",
    "REMEMBER": "this script no longer builds the SlotCab_* cube blockout — ASlotCabinet "
                "carries its own mesh now (currently /Engine/BasicShapes/Cube, 160x160x120; "
                "see docs/MARQUEE_SPEC.md for what replaces it)",
}

text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "build_slot_cabinet.json"
try:
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[build_slot_cabinet] wrote " + out)
except Exception as e:
    unreal.log_error("[build_slot_cabinet] could not write %s: %s" % (out, e))

print(text)
