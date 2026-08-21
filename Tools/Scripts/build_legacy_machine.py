"""
build_legacy_machine.py — THE ONE-DAY TEST (docs/MACHINE_PLAN.md §8).

Stands the legacy system up in the office: Mrs. Hall has been complaining about it since
the first minute of the game, and until now grepping the project for it returned her line
and a code comment.

WHAT IT BUILDS

    LegacyMachine        the bed, the workpiece, two bins, and the tally on the housing
    LegacyPart_01..05    five stages, each with a plaque (the docs) and a true name
                         (the source, Code Vision only)

THE PUZZLE IS ONE DISAGREEMENT. Every plaque describes a reasonable part and every part
except one is telling the truth. GRADER's housing says it passes anything at grade B or
better; its source says the comparison is backwards. Nothing points at it -- no marker, no
prompt, no highlight. The player watches the machine reject everything, reads five
plaques that all sound fine, holds V, and finds the one line that does not match.

That is the entire question this test exists to answer: is that ten minutes fun?

WHY THE FIX IS A "SCALE" REFACTOR. It has to be SOMETHING, because URefactorableComponent
edits a material or a scale and there is no authored material for a corrected part. Scale
gives the part a small visible shrug when it is fixed. What actually matters is
bIsRefactored -- LegacyMachinePart::IsBehaving() reads it, and because that component is
already IBranchable with a level-baked GUID, Test-Drive and Deploy work on this machine
with no new code.

EditType/RefactoredScale are PROTECTED in C++ and set here instead, exactly as a human
would set them in the Details panel.

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

# The five stages. (plaque = the docs, true_name = the source, faulty)
#
# Read the plaques and nothing is obviously wrong; read the true names and exactly one
# contradicts its own housing. INTAKE claims and does the same thing. So does COUNTER.
# GRADER's plaque promises "grade B or better passes" and its source rejects everything.
PARTS = [
    ("INTAKE",   "INTAKE\ntakes one blank per cycle",
                 "intake()  ->  blank  [ok]", False),
    ("COUNTER",  "COUNTER\nlogs every blank it sees",
                 "count += 1  [ok]", False),
    ("GRADER",   "GRADER\ngrade B or better passes",
                 "pass = (grade > A)   <-- nothing is above A", True),
    ("STAMP",    "STAMP\nmarks the passing blanks",
                 "stamp(passing)  [ok]", False),
    ("OUTFEED",  "OUTFEED\ndelivers to the accept bin",
                 "deliver(stamped)  [ok]", False),
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
    placed = []
    for i, (short, plaque, true_name, faulty) in enumerate(PARTS):
        loc = unreal.Vector(ORIGIN.x, ORIGIN.y + i * PART_SPACING, ORIGIN.z + 90.0)
        p = eas.spawn_actor_from_class(part_cls, loc, unreal.Rotator(0.0, 0.0, YAW))
        if p is None:
            notes.append("could not spawn part %s" % short)
            continue
        p.set_actor_label("%s%02d_%s" % (PART_PREFIX, i + 1, short))
        p.set_actor_scale3d(unreal.Vector(0.55, 0.55, 0.9))
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

        # The refactor edit, set here because these are protected in C++.
        refac = p.get_component_by_class(unreal.RefactorableComponent)
        if refac:
            try:
                refac.set_editor_property("edit_type", unreal.RefactorEditType.SCALE)
                refac.set_editor_property("refactored_scale",
                                          unreal.Vector(0.55, 0.55, 0.75))
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

        # The bed runs along Y through the parts; bins sit past the last stage.
        bed = m.get_editor_property("bed")
        if bed:
            bed.set_editor_property("relative_scale3d", unreal.Vector(0.5, 5.6, 0.12))

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
        place_child("workpiece", unreal.Vector(0.0, -170.0, 55.0), unreal.Vector(0.25, 0.25, 0.25))
        place_child("accept_bin", unreal.Vector(-70.0, 235.0, 18.0), unreal.Vector(0.6, 0.6, 0.3))
        place_child("reject_bin", unreal.Vector(-165.0, 235.0, 18.0), unreal.Vector(0.6, 0.6, 0.3))
        place_child("accept_label", unreal.Vector(-70.0, 235.0, 52.0), unreal.Vector(1.0, 1.0, 1.0))
        place_child("reject_label", unreal.Vector(-165.0, 235.0, 52.0), unreal.Vector(1.0, 1.0, 1.0))

        tally = m.get_editor_property("tally")
        if tally:
            tally.set_editor_property("relative_location", unreal.Vector(-70.0, 0.0, 165.0))
            tally.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 180.0))

        unlit_text(m)   # tally + ACCEPT/REJECT labels
        notes.append("machine wired to %d parts" % len(placed))
        les.save_current_level()
        notes.append("level saved")

payload = {
    "map": MAP,
    "notes": notes,
    "the_puzzle": "GRADER's plaque promises 'grade B or better passes'; its true name "
                  "reads 'pass = (grade > A)' — nothing is above A, so it rejects "
                  "everything. Nothing in the level points at it.",
    "how_to_play": "walk up, watch a cycle reject, read the five plaques, hold V to see "
                   "the true names, R the GRADER, watch the next piece land in ACCEPT. "
                   "Then [6] branch, [8] discard to see the fault come back, and Deploy "
                   "to make the fix survive a reload.",
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
