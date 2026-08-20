# make_poker_deck.py — replace the Video Poker checkerboard cube with a standing deck.
#
# Creates /Game/Cards/M_CardBack (T_card_back) and /Game/Cards/M_CardEdge (paper).
# Reshapes PokerMachine CabinetMesh: Engine cube, card proportions, card-back material.
# Idempotent. Editor OPEN via ue_bridge, then save the map.
#
# Walt 2026-08-19: "can Video Poker be a big deck of cards?"

import unreal

TAG = "###POKERDECK###"
# The cabinet lives in the library, not the office.
unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_Carousel")
PKG = "/Game/Cards"
CUBE = "/Engine/BasicShapes/Cube.Cube"
BACK_TEX = "/Game/Cards/T_card_back.T_card_back"
ACE_TEX = "/Game/Cards/T_card_a_s.T_card_a_s"

# Standing deck: 18cm thick, 114cm wide, 160cm tall (poker 2.5:3.5).
# Rel Z = half height so it sits on the actor origin (library floor).
DECK_SCALE = unreal.Vector(0.18, 1.14, 1.60)
DECK_LOC = unreal.Vector(0.0, 0.0, 80.0)


def ensure_texture_material(name, texture_path):
    path = "%s/%s" % (PKG, name)
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    at = unreal.AssetToolsHelpers.get_asset_tools()
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = at.create_asset(name, PKG, unreal.Material, unreal.MaterialFactoryNew())
    tex = unreal.load_asset(texture_path)
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
    if tex:
        ts.set_editor_property("texture", tex)
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 280)
    rough.set_editor_property("r", 0.65)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property("two_sided", True)
    mel.recompile_material(mat)
    eal.save_asset(path)
    return mat


def ensure_edge_material():
    path = "%s/M_CardEdge" % PKG
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    at = unreal.AssetToolsHelpers.get_asset_tools()
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = at.create_asset("M_CardEdge", PKG, unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(0.82, 0.74, 0.58, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 280)
    rough.set_editor_property("r", 0.7)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(path)
    return mat


def find_poker():
    for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Actor):
        try:
            cls = a.get_class().get_name()
            lbl = a.get_actor_label()
        except Exception:
            continue
        if cls == "PokerMachine" or "PokerMachine" in lbl:
            return a
    return None


back = ensure_texture_material("M_CardBack", BACK_TEX)
ace = ensure_texture_material("M_CardAce", ACE_TEX)
edge = ensure_edge_material()
cube = unreal.load_asset(CUBE)
poker = find_poker()

notes = []
if poker is None:
    payload = {"ok": False, "error": "no PokerMachine in the editor world — open L_Carousel"}
else:
    cabinet = None
    glow = None
    for c in poker.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() == "CabinetMesh":
            cabinet = c
            break
    for c in poker.get_components_by_class(unreal.PointLightComponent):
        glow = c
        break
    if cabinet is None:
        payload = {"ok": False, "error": "CabinetMesh missing"}
    else:
        cabinet.set_editor_property("static_mesh", cube)
        cabinet.set_editor_property("relative_scale3d", DECK_SCALE)
        cabinet.set_editor_property("relative_location", DECK_LOC)
        cabinet.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
        # Ace facing the player; the back pattern still reads as a deck from the sides
        # better than the default checkerboard. Front face of the cube uses slot 0.
        face_mat = ace if ace else back
        if face_mat:
            cabinet.set_material(0, face_mat)
        if glow:
            glow.set_editor_property("relative_location", unreal.Vector(50.0, 0.0, 80.0))
        notes.append("CabinetMesh -> standing deck (ace of spades, card proportions)")
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        saved = bool(les.save_current_level()) if les else False
        payload = {
            "ok": True,
            "actor": poker.get_actor_label(),
            "scale": [DECK_SCALE.x, DECK_SCALE.y, DECK_SCALE.z],
            "materials": {
                "back": bool(back),
                "ace": bool(ace),
                "edge": bool(edge),
            },
            "saved_level": saved,
            "notes": notes,
        }
