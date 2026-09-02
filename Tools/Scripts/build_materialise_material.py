# build_materialise_material.py - the look of a thing being Generated.
#
# *** RUN ONCE, FROM THE OPEN EDITOR. Safe to re-run (it replaces). ***
#
# ===========================================================================
# WHY THIS MATERIAL EXISTS.
#
# Walt: "making it fade in somehow so it doesn't just pop in."
#
# You cannot fade a vendor mesh by its own material. PackDev's materials are opaque and
# have no opacity parameter, and docs/ART_PIPELINE.md says vendor packs are READ-ONLY -
# duplicate out, never author in. Editing them to add a dissolve would break that rule
# and would have to be redone every time the pack updates.
#
# So the spaceport does not fade its own materials. It temporarily SWAPS every part to
# this one, ramps it from invisible to solid, and then puts the real materials back. That
# works with any mesh from any pack and touches nothing vendor-side.
#
# ---------------------------------------------------------------------------
# WHY IT IS CYAN, AND WHY THAT IS NOT A DECORATION.
#
# The city is full of AI ghosts - Manny with the Phantom material, standing on the plaza
# ignoring you. Generate is an AI power. A spaceport that arrives as a translucent
# apparition and then solidifies is not a workaround for a missing fade; it is the same
# visual language, saying what the power actually is. The thing the AI makes arrives the
# way the AI does.
#
# ---------------------------------------------------------------------------
# WHY M_ai_core COULD NOT BE REUSED.
#
# It was the obvious candidate and it does not work: build_ai_apparition.py makes it
# UNLIT and EMISSIVE-ONLY with no opacity channel and no blend mode, so it can glow but
# it cannot fade. It is also white-gold rather than the ghosts' cyan. Checked before
# writing this file rather than assumed.
#
# ---------------------------------------------------------------------------
# COOKING. /Game/AIApparition is already in DirectoriesToAlwaysCook (DefaultGame.ini),
# so a material created here ships without another config line. That is the only reason
# this folder was chosen over a new one.

import json
import traceback

import unreal

TAG = "###MATERIALISE###"
PKG_DIR = "/Game/AIApparition"
NAME = "M_materialise"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/build_materialise_material.json"

r = {}

try:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary

    full = "%s/%s" % (PKG_DIR, NAME)
    r["replaced_existing"] = bool(eal.does_asset_exist(full))
    if r["replaced_existing"]:
        eal.delete_asset(full)

    mat = asset_tools.create_asset(NAME, PKG_DIR, unreal.Material, unreal.MaterialFactoryNew())

    # UNLIT + TRANSLUCENT. Unlit so a half-formed spaceport does not pick up the city's
    # sun and read as a solid grey object; translucent so Opacity means anything at all.
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    # Two-sided: a half-transparent hollow gantry looks wrong from inside otherwise, and
    # the rocket in this pack HAS an interior.
    mat.set_editor_property("two_sided", True)

    # Cyan-white, to match the phantom ghosts already standing on the plaza.
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -760, 0)
    tint.set_editor_property("constant", unreal.LinearColor(0.35, 0.85, 1.0, 1.0))

    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -760, 250)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 6.0)

    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -460, 60)
    mel.connect_material_expressions(tint, "", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # THE PARAMETER THE SPACEPORT DRIVES. 0 = not there yet, 1 = fully formed ghost.
    # ASpaceport ramps this, then swaps the real materials back in at the top.
    opacity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -460, 320)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.0)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(mat)
    eal.save_asset(full)

    # Readback, not assumption: a material that silently failed to take its parameters
    # would leave the spaceport permanently invisible and look like a code bug.
    reloaded = eal.load_asset(full)
    names = []
    if reloaded:
        for p in mel.get_scalar_parameter_names(reloaded):
            names.append(str(p))
    r["asset"] = full
    r["scalar_parameters"] = names
    r["has_opacity"] = "Opacity" in names
    r["has_glow"] = "Glow" in names
    r["ok"] = r["has_opacity"] and r["has_glow"]
    r["next"] = ("Both parameters present - ASpaceport can drive it."
                 if r["ok"] else
                 "MISSING A PARAMETER - the spaceport would materialise invisibly.")

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("%s %s" % (TAG, json.dumps(r)))
print(json.dumps(r, indent=2))
