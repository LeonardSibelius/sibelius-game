# tune_statue_marble.py - the temple's marble, less blinding.
#
# Walt, 2026-10-19, from the temple in Play, a screenshot of the Laocoon: "the light is a bit bright
# against the statues". M_StatueMarble (import_statues.py) was two near-whites - linear albedo about
# 0.8 - and Olympus's sun and sky lit them past white, so the carving went flat and the tablets read
# as blank blocks. Real marble is nearer 0.5. This sets the material's own values: its two colours
# about a third darker, and its roughness up from 0.32-0.48 to 0.45-0.60 so the highlights spread
# instead of burning. Values only - the graph is not rebuilt (make_genesis_assets.py: rebuilding a
# material in place crashed the editor, !IsRooted).
#
# Finds the nodes by what they feed: the Lerp into Base Color (its A and B are the colours) and the
# Lerp into Roughness (its A and B the two roughnesses). Saves the material.
#
# *** RUN FROM THE OPEN EDITOR, PIE stopped ***

import unreal

TAG = "###MARBLE###"
PATH = "/Game/Statues/M_StatueMarble"
WARM, COOL = "#c8c1b4", "#b4afa6"     # were #ece6db and #d9d6cf
ROUGH = (0.45, 0.60)                  # were 0.32 and 0.48

mel = unreal.MaterialEditingLibrary


def srgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def lerp_inputs(mat, prop):
    lerp = mel.get_material_property_input_node(mat, prop)
    if not isinstance(lerp, unreal.MaterialExpressionLinearInterpolate):
        raise RuntimeError("%s is not fed by a Lerp (%s)" % (prop, lerp))
    return mel.get_inputs_for_material_expression(mat, lerp)   # A, B, Alpha


if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    print("%s PIE is running - stop it first; nothing done" % TAG)
else:
    mat = unreal.load_asset(PATH)
    a, b, _ = lerp_inputs(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    for node, hexcol in ((a, WARM), (b, COOL)):
        before = node.get_editor_property("constant")
        rgb = srgb(hexcol)
        node.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        print("%s colour %.2f %.2f %.2f -> %.2f %.2f %.2f" % (TAG, before.r, before.g, before.b, rgb[0], rgb[1], rgb[2]))
    ra, rb, _ = lerp_inputs(mat, unreal.MaterialProperty.MP_ROUGHNESS)
    for node, v in ((ra, ROUGH[0]), (rb, ROUGH[1])):
        print("%s roughness %.2f -> %.2f" % (TAG, node.get_editor_property("r"), v))
        node.set_editor_property("r", v)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH)
    print("%s %s recompiled and saved" % (TAG, PATH))
