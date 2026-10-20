# tune_statue_tablet.py - the tablets' bronze, less of a mirror in the sun.
#
# Walt, 2026-10-19, from the temple: "only the Python sign is too bright now, the others are in
# shade." M_StatueTablet (import_statues.py) was metallic 0.7, roughness 0.55: in shade it read as
# bronze, but Python's tablet stands in the sun and a metal that smooth mirrors Olympus's white sky.
# This makes it weathered rather than polished - metallic 0.35, roughness 0.8 - and its two colours
# a little darker. Values only - the graph is not rebuilt (see tune_statue_marble.py).
#
# *** RUN FROM THE OPEN EDITOR, PIE stopped ***

import unreal

TAG = "###BRONZE###"
PATH = "/Game/Statues/M_StatueTablet"
BRONZE, PATINA = "#4a3522", "#323d30"     # were #5a4128 and #3e4a3a
METALLIC, ROUGHNESS = 0.35, 0.80          # were 0.7 and 0.55

mel = unreal.MaterialEditingLibrary


def srgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    print("%s PIE is running - stop it first; nothing done" % TAG)
else:
    mat = unreal.load_asset(PATH)
    lerp = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    a, b, _ = mel.get_inputs_for_material_expression(mat, lerp)
    for node, hexcol in ((a, BRONZE), (b, PATINA)):
        rgb = srgb(hexcol)
        node.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    for prop, v in ((unreal.MaterialProperty.MP_METALLIC, METALLIC), (unreal.MaterialProperty.MP_ROUGHNESS, ROUGHNESS)):
        node = mel.get_material_property_input_node(mat, prop)
        print("%s %s %.2f -> %.2f" % (TAG, prop, node.get_editor_property("r"), v))
        node.set_editor_property("r", v)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH)
    ca, cb = a.get_editor_property("constant"), b.get_editor_property("constant")
    print("%s colours now %.3f %.3f %.3f / %.3f %.3f %.3f; %s recompiled and saved" % (TAG, ca.r, ca.g, ca.b, cb.r, cb.g, cb.b, PATH))
