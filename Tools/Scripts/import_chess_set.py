# import_chess_set.py — the park chess table's board and pieces.
#
# Imports Tomas Dvorak's free "Chess Set" (Fab, Standard License, "Generated with AI: No")
# from its raw download, unpacked at Tools/VendorSource/ChessSet/ (gitignored, like
# Content/ChessSet/ where this puts the result: a vendor asset stays out of the public repo).
#
# WHAT THE DOWNLOAD IS, measured from its FBX on 2026-09-28 (Y-up, 1 unit = 1 cm nominal):
#   - 33 separate objects: the board and all 32 pieces, standing in the opening position.
#   - Modelled about 6x real size: squares 34.81 apart, the king 57.8 tall. At a tournament
#     square of 5.7 cm that is a scale of 0.16375, which makes the king 9.5 cm and the board
#     53 cm - the proportions of a real Staunton set, so one number fixes everything.
#   - Each piece's pivot is its middle, not its base; every base sits on the playing
#     surface, which is 3 mm below the board's raised frame.
#   - Three materials (Board, White, Black) whose texture paths point at the author's own
#     D: drive, with no base colour or AO linked at all. So the importer is told to import
#     NO materials, and this script builds them from the PNGs itself.
#
# WHAT IT MAKES, under /Game/ChessSet:
#   Meshes/SM_Chess_Board, SM_Chess_White<Type>, SM_Chess_Black<Type> - one of each piece
#     per colour (13 meshes), vertices baked where the vendor stood them, so each colour's
#     knights already face the other side. The table actor centres each one by its bounds.
#   Textures/T_Chess_*  Materials/M_ChessPBR + MI_Chess_White / _Black / _Board
#
# RUN with the editor open (any level), PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_chess_set.py"
# It prints the bounds the table actor needs. Safe to re-run: it replaces what it made.

import os
import re
import unreal

TAG = "###CHESSSET###"
SOURCE = "C:/Users/wpark/projects/sibelius-game/Tools/VendorSource/ChessSet"
FBX = SOURCE + "/source/chess.fbx"
TEXTURES = SOURCE + "/textures"
ROOT = "/Game/ChessSet"
STAGING = ROOT + "/_Import"
SCALE = 5.7 / 34.81          # tournament square / the model's square
FLIP_NORMAL_GREEN = False    # set True if the pieces look lit from below

MATERIALS = ["White", "Black", "Board"]
MAPS = ["Base_color", "Normal", "Roughness", "Metallic", "AO"]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def say(text):
    print("%s %s" % (TAG, text))


def import_meshes():
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = opts.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", False)
    data.set_editor_property("import_uniform_scale", SCALE)
    data.set_editor_property("transform_vertex_to_absolute", True)
    data.set_editor_property("bake_pivot_in_vertex", False)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX)
    task.set_editor_property("destination_path", STAGING)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", opts)

    # The options above belong to the classic FBX importer. UE 5.7 sends FBX to Interchange
    # by default, which ignores them, so switch to the classic one for this import only.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        asset_tools.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")


def keep_one_of_each():
    """The vendor's 32 pieces are 12 shapes. Keeps the first of each and the board."""
    kept = {}
    for path in eal.list_assets(STAGING, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        name = asset.get_name().lower()
        if re.search(r"\d{3}", name):
            continue    # pawn_white.001_low and the like: copies
        if "board" in name:
            kept["SM_Chess_Board"] = path
            continue
        # Searched, not prefix-matched: the importer may put the file name in front.
        found = re.search(r"(king|queen|rook|bishop|knight|pawn)_(white|black)", name)
        if found:
            kept["SM_Chess_%s%s" % (found.group(2).capitalize(), found.group(1).capitalize())] = path
    return kept


def import_textures():
    made = {}
    for material in MATERIALS:
        for kind in MAPS:
            png = "%s/%s_%s.png" % (TEXTURES, material, kind)
            if not os.path.isfile(png):
                say("missing %s" % png)
                continue
            name = "T_Chess_%s_%s" % (material, kind.replace("_", ""))
            path = "%s/Textures/%s" % (ROOT, name)
            if eal.does_asset_exist(path):
                eal.delete_asset(path)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", png)
            task.set_editor_property("destination_path", ROOT + "/Textures")
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", False)
            asset_tools.import_asset_tasks([task])
            tex = unreal.load_asset(path)
            if not tex:
                say("texture import FAILED: %s" % png)
                continue
            if kind == "Normal":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("flip_green_channel", FLIP_NORMAL_GREEN)
            elif kind != "Base_color":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                tex.set_editor_property("srgb", False)
            made[(material, kind)] = tex
    return made


def build_materials(textures):
    parent_path = ROOT + "/Materials/M_ChessPBR"
    for path in [parent_path] + ["%s/Materials/MI_Chess_%s" % (ROOT, m) for m in MATERIALS]:
        if eal.does_asset_exist(path):
            eal.delete_asset(path)

    parent = asset_tools.create_asset("M_ChessPBR", ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    wiring = [
        ("BaseColor", "Base_color", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", unreal.MaterialProperty.MP_BASE_COLOR),
        ("Normal", "Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "RGB", unreal.MaterialProperty.MP_NORMAL),
        ("Roughness", "Roughness", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", unreal.MaterialProperty.MP_ROUGHNESS),
        ("Metallic", "Metallic", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", unreal.MaterialProperty.MP_METALLIC),
        ("AO", "AO", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
    ]
    for row, (param, kind, sampler, pin, prop) in enumerate(wiring):
        node = mel.create_material_expression(parent, unreal.MaterialExpressionTextureSampleParameter2D, -500, row * 260)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", sampler)
        node.set_editor_property("texture", textures[("White", kind)])
        mel.connect_material_property(node, pin, prop)
    mel.recompile_material(parent)

    instances = {}
    for material in MATERIALS:
        mi = asset_tools.create_asset("MI_Chess_%s" % material, ROOT + "/Materials",
                                      unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        for param, kind, _, _, _ in wiring:
            tex = textures.get((material, kind))
            if tex:
                mel.set_material_instance_texture_parameter_value(mi, param, tex)
        instances[material] = mi
    return instances


def import_door_sign():
    """The living-room door's sign, rendered from this set's knight by make_chess_sign.py."""
    png = SOURCE + "/T_Sign_Chess.png"
    path = ROOT + "/Textures/T_Sign_Chess"
    if not os.path.isfile(png):
        say("no door sign yet - run Tools/Scripts/make_chess_sign.py, then this again")
        return
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", ROOT + "/Textures")
    task.set_editor_property("destination_name", "T_Sign_Chess")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    asset_tools.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    if tex:
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)    # as T_Sign_Bowling
    else:
        say("door sign import FAILED")


def build_highlight_material():
    """Ours, not the vendor's, so it lives in the tracked /Game/Chess: the see-through colour
    the table lays over a square (last move, picked-up piece, cursor, check) and its dots."""
    path = "/Game/Chess/M_ChessHighlight"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = asset_tools.create_asset("M_ChessHighlight", "/Game/Chess", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    colour = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    colour.set_editor_property("parameter_name", "Color")
    colour.set_editor_property("default_value", unreal.LinearColor(1.0, 0.82, 0.25, 1.0))
    opacity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 200)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.4)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    eal.save_asset(path)


def centre(box):
    return [(box.min.x + box.max.x) * 0.5, (box.min.y + box.max.y) * 0.5, (box.min.z + box.max.z) * 0.5]


def main():
    if not os.path.isfile(FBX):
        say("%s not found - unpack the Fab download there first." % FBX)
        return

    if eal.does_directory_exist(STAGING):
        eal.delete_directory(STAGING)
    import_meshes()
    kept = keep_one_of_each()
    if len(kept) != 13:
        say("expected 13 meshes (board + 6 shapes x 2 colours), found %d: %s" % (len(kept), sorted(kept)))
        return

    for name, path in kept.items():
        final = "%s/Meshes/%s" % (ROOT, name)
        if eal.does_asset_exist(final):
            eal.delete_asset(final)
        eal.rename_asset(path, final)
    eal.delete_directory(STAGING)

    instances = build_materials(import_textures())
    import_door_sign()
    build_highlight_material()
    meshes = {}
    for name in kept:
        mesh = unreal.load_asset("%s/Meshes/%s" % (ROOT, name))
        colour = "Board" if name == "SM_Chess_Board" else ("White" if "White" in name else "Black")
        mesh.set_material(0, instances[colour])
        meshes[name] = mesh

    eal.save_directory(ROOT, only_if_is_dirty=False, recursive=True)

    # What the table actor needs to know. Vertices are baked where the vendor stood the
    # pieces, so each mesh's bounds say which square it came from.
    board = meshes["SM_Chess_Board"].get_bounding_box()
    say("board  min (%.2f %.2f %.2f) max (%.2f %.2f %.2f)  size %.1f x %.1f cm" % (
        board.min.x, board.min.y, board.min.z, board.max.x, board.max.y, board.max.z,
        board.max.x - board.min.x, board.max.y - board.min.y))
    for name in sorted(meshes):
        if name == "SM_Chess_Board":
            continue
        box = meshes[name].get_bounding_box()
        c = centre(box)
        say("%-22s centre (%.2f %.2f) base z %.2f  height %.2f  width %.2f" % (
            name, c[0], c[1], box.min.z, box.max.z - box.min.z, box.max.x - box.min.x))
    wk = centre(meshes["SM_Chess_WhiteKing"].get_bounding_box())
    wq = centre(meshes["SM_Chess_WhiteQueen"].get_bounding_box())
    bk = centre(meshes["SM_Chess_BlackKing"].get_bounding_box())
    say("a->h (queen to king, one file): (%.2f %.2f)" % (wk[0] - wq[0], wk[1] - wq[1]))
    say("white->black (king to king, seven ranks): (%.2f %.2f)" % (bk[0] - wk[0], bk[1] - wk[1]))
    say("done. /Game/ChessSet: 13 meshes, 15 textures, 1 material + 3 instances.")


main()
