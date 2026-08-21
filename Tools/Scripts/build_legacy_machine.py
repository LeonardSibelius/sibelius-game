"""
build_legacy_machine.py — THE ONE-DAY TEST (docs/MACHINE_PLAN.md §8).

Stands the legacy system up in the office: Mrs. Hall has been complaining about it since
the first minute of the game, and until now grepping the project for it returned her line
and a code comment.

WHAT IT BUILDS

    LegacyMachine        the bed, the workpiece, two bins, and the tally on the housing
    LegacyPart_01..05    five stages, each with a plaque (the docs) and a true name
                         (the source, Code Vision only)

LOOK: Crebotoly crate meshes (SciFiBoxes_A), already in DirectoriesToAlwaysCook.
An ugly production line on a QuadArt rug is the job. Engine cubes were the sketch.

THE PUZZLE IS ONE DISAGREEMENT. On four parts the plaque and the true name are WORD FOR
WORD IDENTICAL, so the eye skims them. GRADER's housing says grade B or better passes;
its source passes only what is better than A, and nothing is better than A. No marker, no
prompt, no highlight. The player watches the machine reject everything, reads five
plaques that all sound fine, holds V, and finds the one line that does not match.

That is the entire question this test exists to answer: is that ten minutes fun?

WHY THE FIX IS A MATERIAL REFACTOR. URefactorableComponent edits a material or a scale.
Scale was the sketch: shrinking a box does not read as correcting a comparison. A cooler
crate material does, and bIsRefactored is still what IsBehaving() reads. The true-name
label also swaps to the plaque claim in C++, so holding V after R shows two matching
lines.

EditType / RefactoredMaterial are PROTECTED in C++ and set here instead, exactly as a
human would set them in the Details panel.

Idempotent: re-running clears and rebuilds the machine and its parts.

Editor-CLOSED:
    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_legacy_machine.py"
"""
import json
import unreal

MAP = "/Game/L_Office_v02"
MACHINE_LABEL = "LegacyMachine"
PART_PREFIX = "LegacyPart_"

# Where it stands. The living room end of the office, on the ground floor, in the open
# so the player meets it early -- this is the ticket, not a secret.
ORIGIN = unreal.Vector(-1900.0, 9500.0, 0.0)
PART_SPACING = 75.0           # cm between stages, along +Y (was 120: the row ran 480cm
                              # and its far end reached the bathroom doorway)
YAW = 0.0

# Unreal Python Rotator positional args are (roll, pitch, yaw) — not C++
# FRotator(pitch, yaw, roll). Rotator(0, 180, 0) is PITCH 180: the plaques
# hang upside down. ACCEPT/REJECT already used (0, 0, 180) and read correctly.
FACE_PLAYER = unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0)

# The five stages. (plaque = the docs, true_name = the source, faulty)
#
# PLAIN ENGLISH, NOT FAKE CODE.
#
# The first version wrote the true names as pseudo-source -- "intake() -> blank [ok]",
# "count += 1 [ok]", "pass = (grade > A)". Walt, a programmer of forty years, walked up to
# the first box, pressed V and asked what the message meant. That is the whole answer: if
# the four HONEST parts read as noise, the liar does not stand out from them, it is just
# more noise. The code styling was decoration -- it made "source" LOOK like source instead
# of doing source's job.
#
# So the true name is written in exactly the same voice as the plaque, and on an honest
# part it is WORD FOR WORD IDENTICAL. The eye skims four matching pairs in a second and
# stops dead at the fifth. Nothing to decode; the only thing that differs is what it says.
#
# GRADER's pair is the puzzle: the housing promises "grade B or better passes" and the
# source passes only what is better than A. Nothing is better than A -- which is a thing
# you realise rather than read, and realising it is the feeling this test is for.

# Crebotoly SciFiBoxes_A — already cooked. Five colour variants so the row is a family,
# not a clone stamp. Bins are open crates. The workpiece is a small closed crate (a blank).
PART_MESHES = [
    "/Game/SciFiBoxes_A/Meshes/Box2/Box2_v0",
    "/Game/SciFiBoxes_A/Meshes/Box2/Box2_v1",
    "/Game/SciFiBoxes_A/Meshes/Box2/Box2_v2",
    "/Game/SciFiBoxes_A/Meshes/Box2/Box2_v3",
    "/Game/SciFiBoxes_A/Meshes/Box2/Box2_v4",
]
BIN_MESH = "/Game/SciFiBoxes_A/Meshes/Box1/Box1_Open_Empty_v0"
BED_MESH = "/Game/SciFiBoxes_A/Meshes/Box3/Box3_v0"
WORKPIECE_MESH = "/Game/SciFiBoxes_A/Meshes/Box1/Box1_Closed_v0"
FIXED_MAT = "/Game/SciFiBoxes_A/Materials/Grey"   # the corrected GRADER: cooler, not shrunk

PART_TARGET_HEIGHT = 70.0     # cm, after scale — plaque-readable, not furniture-tall
PARTS = [
    ("INTAKE",   "INTAKE\ntakes one blank per cycle",
                 "takes one blank per cycle", False),
    ("COUNTER",  "COUNTER\nlogs every blank it sees",
                 "logs every blank it sees", False),
    ("GRADER",   "GRADER\ngrade B or better passes",
                 "passes only what is better than A", True),
    ("STAMP",    "STAMP\nmarks the passing blanks",
                 "marks the passing blanks", False),
    ("OUTFEED",  "OUTFEED\ndelivers to the accept bin",
                 "delivers to the accept bin", False),
]

notes = []
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)

machine_cls = unreal.load_class(None, "/Script/SibeliusGame.LegacyMachine")
part_cls = unreal.load_class(None, "/Script/SibeliusGame.LegacyMachinePart")

# ---------------------------------------------------------------- the label material
#
# UNLIT, OR THE TEXT WASHES OUT. UTextRenderComponent defaults to
# DefaultTextMaterialOpaque, which is DEFAULT_LIT -- in a bright room the tally rendered
# as a pale orange smear and the plaques were hard to read. Exactly the same fault the
# cathedral marquee hit, and the same fix.
#
# DUPLICATED from the engine material, never built from scratch: the font keeps its glyph
# shapes in an atlas that the material samples into OPACITY MASK, and a hand-built
# material has no atlas lookup, so the letters simply do not exist. (Learned the hard way
# on the marquee -- see Tools/Scripts/build_cabinet_marquee.py.)
LABEL_MAT_PATH = "/Game/SlotFactory/M_LabelUnlit"
mel = unreal.MaterialEditingLibrary
label_mat = None
if unreal.EditorAssetLibrary.does_asset_exist(LABEL_MAT_PATH):
    unreal.EditorAssetLibrary.delete_asset(LABEL_MAT_PATH)
label_mat = unreal.EditorAssetLibrary.duplicate_asset(
    "/Engine/EngineMaterials/DefaultTextMaterialOpaque", LABEL_MAT_PATH)
if label_mat:
    label_mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    src = mel.get_material_property_input_node(label_mat, unreal.MaterialProperty.MP_BASE_COLOR)
    if src:
        # Unlit ignores base colour, so the font-atlas-times-vertex-colour node has to
        # reach emissive too. x1.6 -- readable and crisp, deliberately NOT the marquee's
        # x5: these are engraved plaques, not a neon sign.
        k = mel.create_material_expression(label_mat, unreal.MaterialExpressionConstant, -600, 260)
        k.set_editor_property("r", 1.6)
        mul = mel.create_material_expression(label_mat, unreal.MaterialExpressionMultiply, -350, 160)
        mel.connect_material_expressions(src, "", mul, "A")
        mel.connect_material_expressions(k, "", mul, "B")
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(label_mat)
    unreal.EditorAssetLibrary.save_asset(LABEL_MAT_PATH)
    notes.append("M_LabelUnlit ready (unlit, x1.6)")
else:
    notes.append("could not duplicate the engine text material — labels stay lit")


def unlit_text(actor):
    """Point every TextRenderComponent on an actor at the unlit material."""
    if not label_mat:
        return
    for c in actor.get_components_by_class(unreal.TextRenderComponent):
        c.set_material(0, label_mat)


def load_mesh(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        notes.append("missing mesh %s" % path)
    return asset


def mesh_local_box(static_mesh):
    """Axis-aligned box in mesh space (cm). Falls back to a 100cm cube."""
    try:
        box = static_mesh.get_bounding_box()
        return box.min, box.max
    except Exception:
        return unreal.Vector(-50, -50, -50), unreal.Vector(50, 50, 50)


def scale_to_height(current_height, target_height):
    if current_height < 1.0:
        return 1.0
    return target_height / current_height


def assign_mesh(comp, path):
    asset = load_mesh(path)
    if asset is None or comp is None:
        return None
    try:
        comp.set_editor_property("static_mesh", asset)
    except Exception as e:
        notes.append("could not assign %s: %s" % (path, e))
        return None
    return asset


def place_labels_on_minus_x_face(part, static_mesh):
    """THE PAIRING IS THE PUZZLE. Plaque above, true name below, on the -X face.
    Offsets were tuned for a 100cm cube; new meshes put them in the air unless
    they are placed from the mesh's own bounds."""
    mn, mx = mesh_local_box(static_mesh)
    height = mx.z - mn.z
    front = mn.x - 2.0
    mid_z = (mx.z + mn.z) * 0.5
    plaque = None
    true_label = None
    for c in part.get_components_by_class(unreal.TextRenderComponent):
        name = c.get_name()
        if "Plaque" in name:
            plaque = c
        elif "True" in name:
            true_label = c
    if plaque:
        plaque.set_editor_property(
            "relative_location", unreal.Vector(front, 0.0, mid_z + height * 0.22))
        plaque.set_editor_property("relative_rotation", FACE_PLAYER)
        plaque.set_world_size(9.0)
    if true_label:
        true_label.set_editor_property(
            "relative_location", unreal.Vector(front - 2.0, 0.0, mid_z - height * 0.18))
        true_label.set_editor_property("relative_rotation", FACE_PLAYER)
        true_label.set_world_size(9.0)

if machine_cls is None or part_cls is None:
    notes.append("LegacyMachine/LegacyMachinePart classes not found — build the editor "
                 "target first (they are new UCLASSes; Live Coding cannot add them)")
else:
    # ---- idempotent clear
    removed = 0
    for a in list(eas.get_all_level_actors()):
        try:
            label = a.get_actor_label()
        except Exception:
            continue
        if label == MACHINE_LABEL or label.startswith(PART_PREFIX):
            eas.destroy_actor(a)
            removed += 1
    if removed:
        notes.append("cleared %d actor(s) from a previous run" % removed)

    # ---- the parts
    fixed_mat = unreal.EditorAssetLibrary.load_asset(FIXED_MAT)
    if fixed_mat is None:
        notes.append("missing corrected material %s — GRADER will have no visible fix" % FIXED_MAT)

    placed = []
    for i, (short, plaque, true_name, faulty) in enumerate(PARTS):
        loc = unreal.Vector(ORIGIN.x, ORIGIN.y + i * PART_SPACING, ORIGIN.z + 90.0)
        p = eas.spawn_actor_from_class(part_cls, loc, unreal.Rotator(0.0, 0.0, YAW))
        if p is None:
            notes.append("could not spawn part %s" % short)
            continue
        p.set_actor_label("%s%02d_%s" % (PART_PREFIX, i + 1, short))
        p.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        p.set_editor_property("plaque_text", plaque)
        p.set_editor_property("true_name", true_name)
        # bIsFaulty -> "is_faulty". Unreal's Python bindings STRIP the leading 'b' from
        # boolean UPROPERTYs and snake_case the rest; "b_is_faulty" throws. Same trap
        # dump_power_grants.py recorded for bGrantsPower, and it is worth being loud
        # about: a silently unset fault would make every part honest, and the machine
        # would run clean with no puzzle in it at all.
        set_fault = False
        for key in ("is_faulty", "b_is_faulty"):
            try:
                p.set_editor_property(key, faulty)
                set_fault = True
                break
            except Exception:
                continue
        if not set_fault:
            notes.append("COULD NOT SET the fault flag on %s — no puzzle" % short)

        mesh_comp = p.get_editor_property("mesh")
        mesh_path = PART_MESHES[i] if i < len(PART_MESHES) else PART_MESHES[-1]
        sm = assign_mesh(mesh_comp, mesh_path)
        if sm is not None:
            mn, mx = mesh_local_box(sm)
            s = scale_to_height(mx.z - mn.z, PART_TARGET_HEIGHT)
            p.set_actor_scale3d(unreal.Vector(s, s, s))
            place_labels_on_minus_x_face(p, sm)
            notes.append("%s mesh %s scale %.3f bounds_z %.1f" % (
                short, mesh_path.split("/")[-1], s, mx.z - mn.z))

        # The refactor edit, set here because these are protected in C++.
        # MATERIAL, not scale: shrinking a crate is not a fix. Grey is the
        # corrected look; bIsRefactored is what the machine actually reads.
        refac = p.get_component_by_class(unreal.RefactorableComponent)
        if refac:
            try:
                refac.set_editor_property("edit_type", unreal.RefactorEditType.MATERIAL)
                if faulty and fixed_mat is not None:
                    refac.set_editor_property("refactored_material", fixed_mat)
            except Exception as e:
                notes.append("could not set the refactor edit on %s: %s" % (short, e))
        else:
            notes.append("%s has no RefactorableComponent — it cannot be fixed" % short)
        unlit_text(p)   # plaque + true name, readable at any lighting
        placed.append(p)

    notes.append("placed %d parts, faulty = %s"
                 % (len(placed), [s for s, _, _, f in PARTS if f]))

    # ---- the machine
    mid_y = ORIGIN.y + (len(PARTS) - 1) * PART_SPACING / 2.0
    m = eas.spawn_actor_from_class(
        machine_cls, unreal.Vector(ORIGIN.x, mid_y, ORIGIN.z + 40.0),
        unreal.Rotator(0.0, 0.0, YAW))
    if m is None:
        notes.append("could not spawn the machine")
    else:
        m.set_actor_label(MACHINE_LABEL)
        m.set_editor_property("parts", placed)

        def fit_comp(comp, mesh_path, target_x, target_y, target_z):
            sm = assign_mesh(comp, mesh_path)
            if sm is None or comp is None:
                return
            mn, mx = mesh_local_box(sm)
            sx = target_x / max(mx.x - mn.x, 1.0)
            sy = target_y / max(mx.y - mn.y, 1.0)
            sz = target_z / max(mx.z - mn.z, 1.0)
            comp.set_editor_property("relative_scale3d", unreal.Vector(sx, sy, sz))

        # The bed runs along Y through the parts; bins sit past the last stage.
        bed = m.get_editor_property("bed")
        if bed:
            fit_comp(bed, BED_MESH, 50.0, 360.0, 12.0)

        def place_child(prop, rel, scale):
            c = m.get_editor_property(prop)
            if c:
                c.set_editor_property("relative_location", rel)
                c.set_editor_property("relative_scale3d", scale)

        # REAL CENTIMETRES. These hang off a bare Root now, not off the stretched bed,
        # so an offset means what it says. Under the old Bed parenting the tally's 14cm
        # became 1.68cm and sat on the floor behind the parts, invisible.
        # BOTH BINS ON THE PLAYER'S SIDE (-X), not straddling the row.
        #
        # The first layout put them +/-95cm either side of the machine line and 90cm past
        # OUTFEED. That is where the bathroom doorway is: ACCEPT landed on the living-room
        # rug and REJECT landed on bathroom tile, through a wall. It also meant the bin
        # labels were read from a different angle than the part plaques, so they rendered
        # back-to-front.
        #
        # Keeping them on the -X side -- the side the plaques face and the player walks --
        # fixes the room AND the facing, because now they are read from the same place
        # everything else is.
        place_child("workpiece", unreal.Vector(0.0, -170.0, 55.0), unreal.Vector(1.0, 1.0, 1.0))
        fit_comp(m.get_editor_property("workpiece"), WORKPIECE_MESH, 18.0, 18.0, 18.0)
        place_child("accept_bin", unreal.Vector(-70.0, 235.0, 18.0), unreal.Vector(1.0, 1.0, 1.0))
        place_child("reject_bin", unreal.Vector(-165.0, 235.0, 18.0), unreal.Vector(1.0, 1.0, 1.0))
        fit_comp(m.get_editor_property("accept_bin"), BIN_MESH, 55.0, 55.0, 28.0)
        fit_comp(m.get_editor_property("reject_bin"), BIN_MESH, 55.0, 55.0, 28.0)
        place_child("accept_label", unreal.Vector(-70.0, 235.0, 52.0), unreal.Vector(1.0, 1.0, 1.0))
        place_child("reject_label", unreal.Vector(-165.0, 235.0, 52.0), unreal.Vector(1.0, 1.0, 1.0))

        tally = m.get_editor_property("tally")
        if tally:
            tally.set_editor_property("relative_location", unreal.Vector(-75.0, -190.0, 45.0))
            tally.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 180.0))

        unlit_text(m)   # tally + ACCEPT/REJECT labels
        notes.append("machine wired to %d parts" % len(placed))
        les.save_current_level()
        notes.append("level saved")

payload = {
    "map": MAP,
    "notes": notes,
    "the_puzzle": "Four parts say the same thing on their plaque and under Code "
                  "Vision, word for word. GRADER does not: the housing promises "
                  "'grade B or better passes' and the source passes only what is "
                  "better than A. Nothing is better than A. Nothing in the level "
                  "points at it.",
    "how_to_play": "walk up, watch a cycle reject, read the five plaques, hold V to see "
                   "the true names. R is locked — ask Kaia upstairs, come back, R the "
                   "GRADER, watch the next piece land in ACCEPT. The ticket closes. "
                   "Reload keeps it producing without Deploy.",
}
text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "build_legacy_machine.json"
try:
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[build_legacy_machine] wrote " + out)
except Exception as e:
    unreal.log_error("[build_legacy_machine] could not write %s: %s" % (out, e))
print(text)
