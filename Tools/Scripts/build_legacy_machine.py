"""
build_legacy_machine.py — THE ONE-DAY TEST (docs/MACHINE_PLAN.md §8).

Stands the legacy system up in the office: Mrs. Hall has been complaining about it since
the first minute of the game, and until now grepping the project for it returned her line
and a code comment.

WHAT IT BUILDS

    LegacyMachine        the bed, the workpiece, two bins, the tally and the RUN LOG
    LegacyPart_01..05    five stages, each with a plaque (the docs), a true name
                         (the source, Code Vision only) and a fault lamp

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
#
# THE CLAIMS WRAP, and that is a plate constraint, not a writing one.
#
# Measured (dump_legacy_machine_geometry.py): a crate is 43.8cm wide and stands 75cm from
# its neighbour. "grade B or better passes" on one line at a readable size is 78cm of
# plate, and "passes only what is better than A" is 105cm — so the five plates merged
# into a single black strip across the row and hid the workpiece travelling behind it.
#
# Wrapped at a sensible word, the widest plate is ~57cm and they separate cleanly. The
# words are untouched: GetPlaqueClaim now returns everything after the heading, and
# FlattenLabel collapses line breaks before any claim is compared to a true name, so the
# word-for-word pairing that IS the puzzle survives the layout change intact.
# (short, plaque, true_name, faulty, fault_chance, armed_by_grant)
#
# TICKET 1 is GRADER: a logic error, wrong every single cycle. "grade B or better passes"
# against "passes only what is better than A" — nothing is better than an A, which is a
# thing you realise rather than read, and realising it is what the first job is for.
#
# TICKET 2 is STAMP, and it is dormant until Ticket.Legacy.Closed lands, so it cannot
# muddy the opening puzzle. Its lie is one word: the housing says it marks THE passing
# blanks, the source says it marks MOST of them. That is a reliability bug rather than a
# logic bug, it shows up in the run log as a mix rather than a wall of rejections, and it
# is the first fault on this machine you cannot confirm you have fixed by watching —
# which is precisely why it is the one that teaches Test-Drive.
PARTS = [
    ("INTAKE",   "INTAKE\ntakes one blank\nper cycle",
                 "takes one blank\nper cycle", False, 0.0, ""),
    ("COUNTER",  "COUNTER\nlogs every blank\nit sees",
                 "logs every blank\nit sees", False, 0.0, ""),
    ("GRADER",   "GRADER\ngrade B or better\npasses",
                 "passes only what is\nbetter than A", True, 0.0, ""),
    ("STAMP",    "STAMP\nmarks the passing\nblanks",
                 "marks most of the\npassing blanks", True, 0.34, "Ticket.Legacy.Closed"),
    ("OUTFEED",  "OUTFEED\ndelivers to the\naccept bin",
                 "delivers to the\naccept bin", False, 0.0, ""),
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


# ---------------------------------------------------------------- the plate material
#
# DARK, UNLIT, and built from scratch -- unlike M_LabelUnlit above, which MUST be a
# duplicate because the font keeps its glyphs in an atlas the engine material samples
# into opacity mask. A plate has no glyphs. It is a flat dark rectangle, so a two-node
# material is the whole thing and there is no atlas to lose.
PLATE_MAT_PATH = "/Game/SlotFactory/M_LabelPlate"
if unreal.EditorAssetLibrary.does_asset_exist(PLATE_MAT_PATH):
    unreal.EditorAssetLibrary.delete_asset(PLATE_MAT_PATH)
plate_mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_LabelPlate", "/Game/SlotFactory", unreal.Material, unreal.MaterialFactoryNew())
if plate_mat:
    plate_mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Not pure black: a dead-black rectangle in a warm room reads as a hole punched in
    # the world. A very dark warm grey reads as a painted panel.
    dark = mel.create_material_expression(plate_mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
    dark.set_editor_property("constant", unreal.LinearColor(0.020, 0.019, 0.017, 1.0))
    mel.connect_material_property(dark, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(plate_mat)
    unreal.EditorAssetLibrary.save_asset(PLATE_MAT_PATH)
    notes.append("M_LabelPlate ready (unlit, near-black)")
else:
    notes.append("could not create the plate material -- labels stay on bare wood")

# --------------------------------------------------------------- the sign materials
#
# THE SIGN IS BUILT IN C++, NOT HERE. ALegacyMachine's constructor owns the plate, the
# card and their two strings, so the geometry has exactly one owner and this script
# cannot drift from it. All that is left for the script is the two surfaces, because a
# material is an asset and an asset needs the editor.
#
# Same recipe as M_LabelPlate: unlit constant colour. Unlit matters more here than
# anywhere else on the machine -- the office is dim, and a LIT sign hung above the line
# either disappears or picks up the fault lamps' red and looks like part of the alarm.
def flat_unlit(name, rgb, why):
    path = "/Game/SlotFactory/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, "/Game/SlotFactory", unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        notes.append("could not create %s -- %s stays default grey" % (name, why))
        return None
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
    c.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    notes.append("%s ready (%s)" % (name, why))
    return mat

# Dull. The plate is old brass nobody has polished since the line was commissioned, and
# the card is a torn-down box. Neither should out-shout the fault lamps.
brass_mat = flat_unlit("M_SignBrass", (0.085, 0.068, 0.030), "the official plate")
card_mat  = flat_unlit("M_SignCard",  (0.205, 0.150, 0.095), "the taped-on card")

PLATE_MESH = "/Engine/BasicShapes/Cube"   # 100 uu; a slab has no facing to get wrong

# Glyph metrics for the default (Roboto) font, as fractions of the text's world size.
# These are estimates, deliberately EXPOSED as constants rather than measured: Unreal's
# GetTextLocalSize has an axis convention this file would have to guess at, and a guess
# in code is harder to nudge than a guess named at the top of a script. If a plate is a
# little tight or a little loose, change these two numbers and re-run.
GLYPH_W = 0.60
LINE_H = 1.28
PLATE_PAD = 0.45     # extra glyph-widths of margin around the text block. Tight, because
                     # the signs have to clear the workpiece lane -- see the Z fractions
                     # in place_labels_on_minus_x_face.
PLATE_THICK = 1.2    # uu, in the part's / machine's own local units


def text_block_size(content, world_size):
    """(width, height) of a multi-line string at a given text world size, in the same
    units that world size is expressed in."""
    lines = content.split("\n")
    longest = max((len(l) for l in lines), default=0)
    w = longest * world_size * GLYPH_W + 2.0 * PLATE_PAD * world_size * GLYPH_W
    h = len(lines) * world_size * LINE_H + 2.0 * PLATE_PAD * world_size * GLYPH_W
    return w, h


def fit_plate(plate, content, world_size, anchor, left_aligned, back_offset_x):
    """Size and place a slab behind a text block.

    anchor is the text component's own relative location. Text is vertically CENTRED on
    it, so the plate shares its Z. Horizontally: a centred text block shares the anchor's
    Y; a LEFT-aligned block runs from the anchor toward +Y (verified in the level -- the
    run log's rows all began at the head and ran along the row into INTAKE), so its plate
    sits half a width further along."""
    if plate is None:
        return None
    mesh = unreal.EditorAssetLibrary.load_asset(PLATE_MESH)
    if mesh is None:
        notes.append("missing %s -- no plate" % PLATE_MESH)
        return None
    plate.set_editor_property("static_mesh", mesh)
    if plate_mat:
        plate.set_material(0, plate_mat)
    w, h = text_block_size(content, world_size)
    plate.set_editor_property("relative_scale3d",
                              unreal.Vector(PLATE_THICK / 100.0, w / 100.0, h / 100.0))
    y = anchor.y + (w / 2.0 if left_aligned else 0.0)
    plate.set_editor_property("relative_location",
                              unreal.Vector(anchor.x + back_offset_x, y, anchor.z))
    return w, h

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


def place_labels_on_minus_x_face(part, static_mesh, plaque_text, true_text):
    """THE PAIRING IS THE PUZZLE. Plaque above, true name below, on the -X face, each on
    its own dark plate.

    Offsets come from the mesh's own bounds -- they were tuned for a 100cm cube once and
    every new mesh put them in the air.

    The labels now stand FURTHER off the face than they used to (8uu, was 2). These
    crates are not flat boxes: they have raised panels, straps and handles, and a plate
    laid flush against the bounding-box front would push through them. 8uu clears the
    detail and reads as a placard bolted on, which is what it is."""
    mn, mx = mesh_local_box(static_mesh)
    height = mx.z - mn.z
    front = mn.x - 8.0
    mid_z = (mx.z + mn.z) * 0.5
    size = 9.0
    # ---- WHERE THE SIGNS SIT, and why these exact fractions
    #
    # Measured, not reasoned (dump_legacy_machine_geometry.py -- these offsets have been
    # wrong three times from arguing about mesh bounds):
    #
    #     crate        world z 90 .. 160
    #     workpiece    world z 130 .. 148   (CarryHeight 40; the mesh is base-at-origin
    #                                        and 18cm tall, so it sits ON 130, not around it)
    #
    # That leaves 40cm of crate face BELOW the travelling piece and 12cm above it. The
    # plaque and the true name go below, together -- the pairing is the puzzle and they
    # have to stay one surface, one directly under the other. The fault lamp goes above,
    # where a fault light belongs and where it sits directly over a jammed piece.
    #
    #     plaque plate  z 106.0 .. 128.0    2cm under the piece
    #     true plate    z  88.9 .. 104.5    1.5cm under the plaque
    #     lamp plate    z 149.4 .. 158.6    1.4cm over the piece, inside the crate top
    #
    # It fits with centimetres to spare, which is why PLATE_PAD is 0.45 rather than 0.6.
    PLAQUE_Z = -0.113
    TRUE_Z = -0.404
    LAMP_Z = 0.415

    plaque = None
    true_label = None
    fault_lamp = None
    for c in part.get_components_by_class(unreal.TextRenderComponent):
        name = c.get_name()
        if "Plaque" in name:
            plaque = c
        elif "True" in name:
            true_label = c
        elif "Fault" in name:
            fault_lamp = c

    plaque_plate = None
    true_plate = None
    lamp_plate = None
    for c in part.get_components_by_class(unreal.StaticMeshComponent):
        name = c.get_name()
        if "PlaquePlate" in name:
            plaque_plate = c
        elif "TruePlate" in name:
            true_plate = c
        elif "LampPlate" in name:
            lamp_plate = c

    # Plate sits BEHIND its text: the player is at -X, so "behind" is +X of the glyphs.
    BACK = 3.0

    if plaque:
        anchor = unreal.Vector(front, 0.0, mid_z + height * PLAQUE_Z)
        plaque.set_editor_property("relative_location", anchor)
        plaque.set_editor_property("relative_rotation", FACE_PLAYER)
        plaque.set_world_size(size)
        fit_plate(plaque_plate, plaque_text, size, anchor, False, BACK)

    if true_label:
        anchor = unreal.Vector(front - 2.0, 0.0, mid_z + height * TRUE_Z)
        true_label.set_editor_property("relative_location", anchor)
        true_label.set_editor_property("relative_rotation", FACE_PLAYER)
        true_label.set_world_size(size)
        # Sized for the LONGER of the two things this label can ever say: the authored
        # lie, or the plaque's claim once the part has been refactored. A plate that
        # changed width the moment the player pressed R would look like the fix broke
        # something. Both are multi-line now, so compare by widest LINE.
        claim = "\n".join(plaque_text.split("\n")[1:]).strip()

        def widest_line(s):
            return max((len(l) for l in s.split("\n")), default=0)

        widest = true_text if widest_line(true_text) >= widest_line(claim) else claim
        fit_plate(true_plate, widest, size, anchor, False, BACK)

    if fault_lamp:
        # ABOVE THE PLAQUE, near the top edge -- where a fault light goes on real
        # equipment, and out of the way of the plaque/true-name pair below, which has to
        # stay a clean doubling for the puzzle to read at a glance.
        anchor = unreal.Vector(front, 0.0, mid_z + height * LAMP_Z)
        fault_lamp.set_editor_property("relative_location", anchor)
        fault_lamp.set_editor_property("relative_rotation", FACE_PLAYER)
        fault_lamp.set_world_size(size)
        fit_plate(lamp_plate, "REJECTED HERE", size, anchor, False, BACK)

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
    for i, (short, plaque, true_name, faulty, fault_chance, armed_by) in enumerate(PARTS):
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

        # Ticket 2: an intermittent fault, and the grant that brings it to life. Both are
        # plain UPROPERTYs, so a missing one fails loudly here rather than quietly turning
        # the second job into a clone of the first.
        try:
            p.set_editor_property("fault_chance", float(fault_chance))
            p.set_editor_property("armed_by_grant",
                                  unreal.Name(armed_by) if armed_by else unreal.Name("None"))
        except Exception as e:
            notes.append("could not set the intermittent fault on %s: %s" % (short, e))

        mesh_comp = p.get_editor_property("mesh")
        mesh_path = PART_MESHES[i] if i < len(PART_MESHES) else PART_MESHES[-1]
        sm = assign_mesh(mesh_comp, mesh_path)
        if sm is not None:
            mn, mx = mesh_local_box(sm)
            s = scale_to_height(mx.z - mn.z, PART_TARGET_HEIGHT)
            p.set_actor_scale3d(unreal.Vector(s, s, s))
            place_labels_on_minus_x_face(p, sm, plaque, true_name)
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
        unlit_text(p)   # plaque, true name + fault lamp, readable at any lighting
        placed.append(p)

    notes.append("placed %d parts; ticket 1 = %s, ticket 2 = %s"
                 % (len(placed),
                    [s for s, _, _, f, c, g in PARTS if f and not g],
                    [("%s @ %.0f%%" % (s, c * 100)) for s, _, _, f, c, g in PARTS if f and g]))

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

        # ---- the two readouts, and why they are this size
        #
        # A TextRenderComponent's world size is multiplied by its component's scale. The
        # PARTS are scaled 0.558 to stand 70cm tall, so their plaques -- authored at 9 --
        # actually render at 5.0cm. The machine is scale 1, so the tally at 10 and the log
        # at 7 rendered at 10cm and 7cm: the tally was TWICE the size of the plaques it
        # exists to support, and the log sprawled back across INTAKE and COUNTER. The two
        # quiet instrument readouts were the loudest thing in the room.
        #
        # Sized against the plaques' effective 5.0cm now: the tally slightly above it (it
        # is the headline gauge), the log below it (it is reference text you lean in for).
        PLAQUE_EFFECTIVE = 9.0 * 0.558
        TALLY_SIZE = 6.0
        LOG_SIZE = 4.0
        BACK = 3.0          # plate sits behind the glyphs; the player is at -X
        notes.append("plaques render at %.1fcm; tally %.1f, log %.1f"
                     % (PLAQUE_EFFECTIVE, TALLY_SIZE, LOG_SIZE))

        # Worst-case strings, so each plate is cut once for the widest thing its readout
        # can ever say and never breathes in and out while the machine runs.
        widest_tally = "SINCE 03:00\nACCEPTED 000   REJECTED 000\nLINE HALTED"
        widest_log = "RUN LOG"
        for _ in range(6):                       # RunLogRows, the C++ default
            widest_log += "\n03:46  REJECTED AT GRADER"

        tally = m.get_editor_property("tally")
        if tally:
            anchor = unreal.Vector(-75.0, -205.0, 55.0)
            tally.set_editor_property("relative_location", anchor)
            tally.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 180.0))
            tally.set_world_size(TALLY_SIZE)
            fit_plate(m.get_editor_property("tally_plate"),
                      widest_tally, TALLY_SIZE, anchor, False, BACK)

        # THE RUN LOG, stacked above the tally so the housing has ONE instrument cluster
        # instead of captions scattered round the room. Left aligned (the tally is
        # centred) because it is a table and the timestamps have to form a column the eye
        # can run down.
        #
        # LEFT-ALIGNED TEXT RUNS TOWARD +Y here -- verified in the level, not assumed: the
        # first build anchored the log at -150 and its rows marched off along the row and
        # straight through INTAKE and COUNTER. Anchored past the head instead, so the
        # block ends before the first crate.
        run_log = m.get_editor_property("run_log")
        if run_log:
            anchor = unreal.Vector(-75.0, -225.0, 98.0)
            run_log.set_editor_property("relative_location", anchor)
            run_log.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 180.0))
            run_log.set_world_size(LOG_SIZE)
            fit_plate(m.get_editor_property("run_log_plate"),
                      widest_log, LOG_SIZE, anchor, True, BACK)
        else:
            notes.append("no RunLog component -- the overnight history has nowhere to show")

        unlit_text(m)   # tally, run log + ACCEPT/REJECT labels

        # THE SIGN'S TWO SURFACES. unlit_text(m) above already caught SignOfficialText
        # and SignText, because it walks every TextRenderComponent on the actor -- the
        # sign needed no new line for its writing. Only the slabs do.
        #
        # Everything else about the sign (where it hangs, how far the card is pushed off
        # centre so both ends of the real name still show, the 4-degree lean) lives in
        # ALegacyMachine's constructor. Do not add it here as well.
        for prop, mat in (("sign_plate", brass_mat), ("sign_card", card_mat)):
            c = m.get_editor_property(prop)
            if c and mat:
                c.set_material(0, mat)
                notes.append("%s surfaced" % prop)

        # THE REJECT SPILL NEEDS NOTHING HERE ON PURPOSE. DressTheVerdict() copies the
        # mesh and the scale straight off the workpiece component, so the heap is always
        # the box this machine actually throws out, and swapping WORKPIECE_MESH above
        # re-dresses the carpet for free.
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
    "ticket_2": "STAMP, dormant until Ticket.Legacy.Closed. Its plaque says it marks "
                "THE passing blanks; its source says MOST of them, and it drops one in "
                "three. The run log shows a mix instead of a wall. You cannot confirm a "
                "fix by watching, so: [6] to branch, E on the machine to run a 20-piece "
                "test batch off the record, [7] to keep it. The job will not close on a "
                "lucky cycle -- it needs a clean batch AND the fix.",
    "how_to_play": "walk up, watch a piece die AT the broken stage (its fault lamp "
                   "lights and the run log names it), read that stage's plaque -- it "
                   "sounds fine -- then hold V to see what it really does. E halts the "
                   "line and steps it one beat at a time if you want a closer look. R "
                   "is locked: ask Kaia upstairs, come back, R the GRADER, watch the "
                   "next piece land in ACCEPT. The ticket closes -- and the second one "
                   "arms. Reload keeps closed jobs done without Deploy.",
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
