"""
build_cabinet_marquee.py — make the apse machine look like a machine, and give it a
lit sign.

WHAT WAS WRONG
--------------
ASlotCabinet's CabinetMesh was an engine cube scaled 1.6 x 1.6 x 1.2 -- a 160 x 160 x
120 cm box with a SQUARE FOOTPRINT, which is why it read as a crate rather than an
upright. Walt: "the cube you click on to play the final slot is inappropriate."

Height alone would only have made it a taller crate. Narrowing it is what turns it into a
cabinet, and it is also what gives the marquee a proportion to sit on: 100 wide over 70
deep is roughly a real upright's stance.

WHY THE SIGN NEEDS NO TEXTURE
-----------------------------
docs/MARQUEE_SPEC.md describes a painted 2048 x 1024 marquee, and that is still the
target. But a UTextRenderComponent draws text as real geometry from a font and a colour,
with no image file anywhere -- so the sign can say the right words TODAY, and the painted
version replaces it whenever the art exists. The only thing waiting on the texture is the
70s script FACE; swapping in a real TTF later is one property on the text actor.

THE THREE THINGS THAT WENT WRONG GETTING HERE (all recorded so they are not repeated)
------------------------------------------------------------------------------------
1. LIT MATERIALS BLOW OUT. The field was first built as a normal DEFAULT_LIT material
   with a near-black base colour, on the reasoning that near-black stays near-black. It
   does not: a flat panel throws the apse's FateLight_Gold and FateLight_Fill back at the
   player as one broad specular lobe across default 0.5 roughness, and the sign rendered
   as a pale blue-white slab. A backlit sign should not receive scene light at all.

2. unreal.Color IS BGRA POSITIONALLY. Color(255, 210, 77, 255) reads as b=255, g=210,
   r=77 -- a pale cyan, not gold. Always use keywords.

3. A HAND-BUILT TEXT MATERIAL CANNOT DRAW TEXT. The font is RobotoDistanceField, and the
   glyph shapes live in a font atlas that the material has to sample and threshold. A
   material with nothing but an emissive constant has no atlas lookup, so there is no
   glyph masking -- the lettering vanished the instant it was assigned. The text material
   is therefore a DUPLICATE of the engine's DefaultTextMaterialOpaque, which already does
   the sampling, with only its shading model changed to unlit and its existing base-colour
   node also routed to emissive. Never build this one from scratch.

THE PIECES (all attached to the cabinet, so dragging it moves the sign with it)
------------------------------------------------------------------------------
    SlotCab_MarqueeBezel   brass surround, M_SlotGold
    SlotCab_MarqueeField   the dark backlit glass, M_MarqueeField
    SlotCab_MarqueeText    "CELESTIAL / FORTUNE" in gold, M_MarqueeText

All three are NoCollision on purpose. The cabinet's own mesh is BlockAll so the
interactor's ECC_Visibility focus trace lands on it (the SC9 lesson in
docs/sib-34-s2-s3-slot-cabinet-notes.md); a colliding panel bolted to its face would
intercept that trace and steal the [E] prompt from the machine behind it.

Idempotent: re-running rebuilds the sign and re-applies the cabinet's shape.

Editor-CLOSED:
    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_cabinet_marquee.py"
"""
import json
import unreal

MAP = "/Game/Maps/L_Cathedral"
CUBE = "/Engine/BasicShapes/Cube.Cube"
GOLD_MAT = "/Game/SlotFactory/M_SlotGold.M_SlotGold"
ENGINE_TEXT_MAT = "/Engine/EngineMaterials/DefaultTextMaterialOpaque"
MAT_PATH = "/Game/SlotFactory"
FIELD_MAT_NAME = "M_MarqueeField"
TEXT_MAT_NAME = "M_MarqueeText"

# The cabinet, as an upright rather than a crate. Local X is depth (it stands at yaw 180,
# so its local +X face is the world -X face -- the one the player walks up to).
CAB_DEPTH, CAB_WIDTH, CAB_HEIGHT = 0.70, 1.00, 2.00      # -> 70 x 100 x 200 cm
FLOOR_Z = 0.0                                             # measured at the apse

MARQUEE_CENTRE_Z = 172.5        # spans roughly Z 148-197 on the upper front
BEZEL = (0.06, 0.94, 0.49)      # 6 x 94 x 49 cm
FIELD = (0.04, 0.90, 0.45)      # 4 x 90 x 45 cm -- the spec's 2:1 panel
SIGN_TEXT = "CELESTIAL\nFORTUNE"
GOLD = unreal.Color(r=255, g=210, b=77, a=255)   # #FFD24D -- keywords, see note 2 above

# How hard the lettering glows. Vertex colour tops out at 1.0, which is merely "not dark"
# -- emissive has to exceed 1 before the scene's bloom picks it up and the sign starts
# throwing light rather than just being a colour. THIS IS THE DIAL to turn if the marquee
# is too dim or too hot.
TEXT_GLOW = 5.0

notes = []
mel = unreal.MaterialEditingLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)

cube = unreal.load_object(None, CUBE)
gold_mat = unreal.load_object(None, GOLD_MAT)

# ------------------------------------------------------------- clear the old sign FIRST
# Before the materials, so nothing in the level still references M_MarqueeText while it is
# being replaced.
cabinet = None
for a in list(eas.get_all_level_actors()):
    try:
        label = a.get_actor_label()
    except Exception:
        continue
    if a.get_class().get_name() == "SlotCabinet":
        cabinet = a
    elif label.startswith("SlotCab_Marquee"):
        eas.destroy_actor(a)

# ------------------------------------------------------------------- the field material
field_path = "%s/%s" % (MAT_PATH, FIELD_MAT_NAME)
field_mat = unreal.load_object(None, "%s.%s" % (field_path, FIELD_MAT_NAME))
if field_mat is None:
    field_mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        FIELD_MAT_NAME, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
if field_mat:
    try:
        mel.delete_all_material_expressions(field_mat)
    except Exception:
        pass
    field_mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Dark glass, not a lamp -- but not a void either. At 0.03 it read as a black hole
    # punched in the cabinet; this stays clearly darker than the marble while reading as
    # something switched on.
    c = mel.create_material_expression(field_mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
    c.set_editor_property("constant", unreal.LinearColor(0.090, 0.090, 0.240, 1.0))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(field_mat)
    unreal.EditorAssetLibrary.save_asset(field_path)
    notes.append("%s ready (unlit, dark blue glass)" % FIELD_MAT_NAME)
else:
    notes.append("could not create " + FIELD_MAT_NAME)

# -------------------------------------------------------------------- the text material
# Duplicated from the engine's text material, NOT built from scratch -- see note 3.
text_path = "%s/%s" % (MAT_PATH, TEXT_MAT_NAME)
if unreal.EditorAssetLibrary.does_asset_exist(text_path):
    unreal.EditorAssetLibrary.delete_asset(text_path)
text_mat = unreal.EditorAssetLibrary.duplicate_asset(ENGINE_TEXT_MAT, text_path)
if text_mat is None:
    notes.append("could not duplicate %s — falling back to the engine material" % ENGINE_TEXT_MAT)
else:
    text_mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Unlit ignores base colour, so whatever the engine graph fed into BaseColor (the font
    # atlas lookup times the vertex colour) has to go to Emissive as well or the glyphs
    # render black. Reuse the node rather than rebuilding it.
    src = mel.get_material_property_input_node(text_mat, unreal.MaterialProperty.MP_BASE_COLOR)
    if src:
        # ...multiplied up on the way, so the words glow instead of merely being gold.
        # Straight through, emissive maxes out at the vertex colour's 1.0 and the sign sits
        # flat against a cathedral full of bright marble.
        k = mel.create_material_expression(text_mat, unreal.MaterialExpressionConstant, -600, 260)
        k.set_editor_property("r", TEXT_GLOW)
        mul = mel.create_material_expression(text_mat, unreal.MaterialExpressionMultiply, -350, 160)
        mel.connect_material_expressions(src, "", mul, "A")
        mel.connect_material_expressions(k, "", mul, "B")
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        notes.append("%s ready (unlit, %s x %.1f into emissive)"
                     % (TEXT_MAT_NAME, src.get_class().get_name(), TEXT_GLOW))
    else:
        notes.append("%s: no BaseColor node to reroute — glyphs may render black"
                     % TEXT_MAT_NAME)
    mel.recompile_material(text_mat)
    unreal.EditorAssetLibrary.save_asset(text_path)

# ------------------------------------------------------------------------- build it
if cabinet is None:
    notes.append("no ASlotCabinet in %s — nothing to build on" % MAP)
else:
    loc = cabinet.get_actor_location()
    body = cabinet.get_component_by_class(unreal.StaticMeshComponent)
    if body:
        body.set_editor_property("relative_scale3d",
                                 unreal.Vector(CAB_DEPTH, CAB_WIDTH, CAB_HEIGHT))
    # The mesh is centred on the actor origin, so a 200cm body needs the actor at Z=100
    # or it sinks half a metre through the floor.
    cabinet.set_actor_location(
        unreal.Vector(loc.x, loc.y, FLOOR_Z + CAB_HEIGHT * 50.0), False, True)
    notes.append("cabinet reshaped to %.0f x %.0f x %.0f cm, standing on Z=%.0f"
                 % (CAB_DEPTH * 100, CAB_WIDTH * 100, CAB_HEIGHT * 100, FLOOR_Z))

    front_x = loc.x - (CAB_DEPTH * 100.0) / 2.0     # yaw 180 -> the front face is -X

    def panel(label, scale, x, mat):
        a = eas.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(x, loc.y, MARQUEE_CENTRE_Z),
            unreal.Rotator(0.0, 0.0, 180.0))
        if a is None:
            notes.append("could not spawn " + label)
            return None
        a.set_actor_label(label)
        a.set_actor_scale3d(unreal.Vector(*scale))
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        if c:
            c.set_editor_property("static_mesh", cube)
            if mat:
                c.set_material(0, mat)
            # NEVER let the sign block the focus trace to the machine — see the header.
            c.set_collision_profile_name("NoCollision")
            # MOVABLE OR THE ATTACH IS SILENTLY REFUSED. A StaticMeshActor is Static by
            # default and the editor will not parent a Static actor to anything -- the
            # attach below returns without complaint and the panel is left loose, so
            # dragging the cabinet leaves its sign behind. (TextRenderActor is Movable
            # already, which is why the words attached on the first run and these did not.)
            c.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        a.attach_to_actor(cabinet, "", unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, False)
        return a

    panel("SlotCab_MarqueeBezel", BEZEL, front_x - 3.0, gold_mat)
    panel("SlotCab_MarqueeField", FIELD, front_x - 6.5, field_mat)
    notes.append("bezel + field built on the front face at X=%.0f" % front_x)

    # The words sit 2.5cm proud of the field's front face. At 0.5cm they were close
    # enough to be lost against it.
    text_actor = eas.spawn_actor_from_class(
        unreal.TextRenderActor, unreal.Vector(front_x - 11.0, loc.y, MARQUEE_CENTRE_Z),
        unreal.Rotator(0.0, 0.0, 180.0))
    if text_actor is None:
        notes.append("could not spawn the marquee text")
    else:
        text_actor.set_actor_label("SlotCab_MarqueeText")
        tc = text_actor.get_component_by_class(unreal.TextRenderComponent)
        if tc:
            tc.set_text(unreal.Text(SIGN_TEXT))
            tc.set_text_render_color(GOLD)
            tc.set_world_size(16.0)
            tc.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
            tc.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
            if text_mat:
                tc.set_material(0, text_mat)
            notes.append("sign reads %r in gold at 16cm"
                         % SIGN_TEXT.replace("\n", " / "))
        text_actor.attach_to_actor(cabinet, "", unreal.AttachmentRule.KEEP_WORLD,
                                   unreal.AttachmentRule.KEEP_WORLD,
                                   unreal.AttachmentRule.KEEP_WORLD, False)

    les.save_current_level()
    notes.append("level saved")

payload = {
    "map": MAP,
    "notes": notes,
    "still_a_placeholder": "the FACE is Roboto, not the 70s casino script. "
                           "docs/MARQUEE_SPEC.md is still the target: import a script TTF "
                           "and point SlotCab_MarqueeText's Font at it, or replace the "
                           "field with the painted 2048x1024.",
    "verify_in_PIE": "walk the nave — CELESTIAL / FORTUNE should read in gold on a dark "
                     "blue panel, and [E] PLAY THE MACHINE must still appear (the panels "
                     "are NoCollision so they cannot steal the focus trace)",
}
text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "build_cabinet_marquee.json"
try:
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[build_cabinet_marquee] wrote " + out)
except Exception as e:
    unreal.log_error("[build_cabinet_marquee] could not write %s: %s" % (out, e))
print(text)
