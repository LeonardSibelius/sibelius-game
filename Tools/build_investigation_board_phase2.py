# build_investigation_board_phase2.py
# Phase 2 -- investigation board ORDINARY-STATE planting on a plane in front of SM_Board_A1.
# Imports the rendered board texture, builds a 2-layer material (Base + Code-Vision Overlay,
# OverlayOpacity reserved for the Phase 3 drop-in), and mounts a thin plane on the board face.
# SM_Board_A1 is NOT modified (LC-4); the plane + material are separate, authored, committable
# assets under /Game/Mechanics/CodeVision/.
#
# Idempotent: deletes any prior "Investigation_Board_Surface" actor + re-creates assets.
# Run headless (editor CLOSED):
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript -script=<this>
#     -unattended -nopause -nosplash -NullRHI -stdout -EnablePlugins=PythonScriptPlugin

import unreal

PNG   = "C:/Users/wpark/projects/sibelius-game/docs/investigation-board/board_phase2_ordinary.png"
DEST  = "/Game/Mechanics/CodeVision"
TEX   = "T_InvestigationBoard_Base"
MAT   = "M_InvestigationBoard"
MI    = "MI_InvestigationBoard"
MAP   = "/Game/Maps/L_Office_v2_QuadArt"
PLANE = "/Engine/BasicShapes/Plane.Plane"

at  = unreal.AssetToolsHelpers.get_asset_tools()
al  = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
warnings = []

def main():
    unreal.log("=== Phase 2: investigation board (ordinary state) ===")

    # 1) Import the base texture --------------------------------------------------
    tex_path = "%s/%s" % (DEST, TEX)
    if al.does_asset_exist(tex_path):
        al.delete_asset(tex_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", PNG)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", TEX)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    at.import_asset_tasks([task])
    tex = unreal.load_asset(tex_path)
    if not tex:
        raise RuntimeError("texture import failed")
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    al.save_asset(tex_path)
    unreal.log("imported texture %s" % tex_path)

    # 2) Build the 2-layer material ----------------------------------------------
    mat_path = "%s/%s" % (DEST, MAT)
    if al.does_asset_exist(mat_path):
        al.delete_asset(mat_path)
    mat = at.create_asset(MAT, DEST, unreal.Material, unreal.MaterialFactoryNew())

    baseT = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, -120)
    baseT.set_editor_property("parameter_name", "BaseTex"); baseT.set_editor_property("texture", tex)
    ovT = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 220)
    ovT.set_editor_property("parameter_name", "OverlayTex"); ovT.set_editor_property("texture", tex)  # placeholder; opacity 0
    ovO = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 520)
    ovO.set_editor_property("parameter_name", "OverlayOpacity"); ovO.set_editor_property("default_value", 0.0)
    emS = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 360)
    emS.set_editor_property("parameter_name", "EmissiveStrength"); emS.set_editor_property("default_value", 0.45)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 480)
    rough.set_editor_property("r", 0.9)

    mAlpha = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -560, 300)
    lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -300, 40)
    emis = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -120, 320)
    maskAdd = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -300, 560)

    mel.connect_material_expressions(ovT, "A", mAlpha, "A")
    mel.connect_material_expressions(ovO, "", mAlpha, "B")
    mel.connect_material_expressions(baseT, "RGB", lerp, "A")
    mel.connect_material_expressions(ovT, "RGB", lerp, "B")
    mel.connect_material_expressions(mAlpha, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_expressions(lerp, "", emis, "A")
    mel.connect_material_expressions(emS, "", emis, "B")
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # MASKED: items/pins opaque (BaseTex.A), transparent elsewhere so the QuadArt cork + vendor
    # notes show through. The Phase-3 overlay alpha (mAlpha) adds to the mask so threads/circle
    # will appear on top without re-authoring.
    mel.connect_material_expressions(baseT, "A", maskAdd, "A")
    mel.connect_material_expressions(mAlpha, "", maskAdd, "B")
    mel.connect_material_property(maskAdd, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mel.recompile_material(mat)
    al.save_asset(mat_path)
    unreal.log("built material %s" % mat_path)

    # 3) Material instance (Phase 3 will override OverlayTex + OverlayOpacity) ----
    mi_path = "%s/%s" % (DEST, MI)
    if al.does_asset_exist(mi_path):
        al.delete_asset(mi_path)
    mi = at.create_asset(MI, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    # Walt tuned EmissiveStrength down 0.45 -> 0.03 (items were glowing); reconciled from .umap.
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.03)
    al.save_asset(mi_path)
    unreal.log("created MI %s" % mi_path)

    # 4) Mount the plane in front of SM_Board_A1 ---------------------------------
    les.load_level(MAP)
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "Investigation_Board_Surface":
            eas.destroy_actor(a)
    plane_mesh = unreal.load_asset(PLANE)
    # roll=90 reproduces Walt's verified orientation (his editor yaw=180 + the original roll=-90
    # normalized to a clean roll=+90; probed from the saved .umap). Rotator roll MUST be a keyword
    # arg (the 3rd positional is YAW).
    p = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0),
                                   unreal.Rotator(roll=90.0, pitch=0.0, yaw=0.0))
    pc = p.get_component_by_class(unreal.StaticMeshComponent)
    pc.set_static_mesh(plane_mesh)
    # Plane is 100x100; roll -90 -> normal +Y (faces room), local X->world X (width), local Y->world Z (height).
    p.set_actor_scale3d(unreal.Vector(1.30, 0.74, 1.0))   # 130 cm wide x 74 cm tall (inside the wood frame)
    pc.set_material(0, mi)
    p.set_actor_label("Investigation_Board_Surface")
    p.set_editor_property("tags", [unreal.Name("CodeVision.Label=Investigation_Board"), unreal.Name("Phase2Board")])
    # Board face; location reconciled from Walt's saved .umap (manual nudge during rotation).
    p.set_actor_location(unreal.Vector(-185.48, -274.03, 150.0), False, False)

    # ---- Reconcile Walt's manual lighting edits (2026-05-27), read from the saved .umap ----
    # Desk lamp moved to the left of the desk and aimed at the corkboard (motivates the spot).
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "Desk_Lamp":
            a.set_actor_location_and_rotation(unreal.Vector(-81.61, 128.81, 78.44),
                                              unreal.Rotator(roll=0.0, pitch=0.0, yaw=70.0), False, True)
            break
    # Warm accent SpotLight at the lamp, lighting the cork + vendor notes uniformly (idempotent).
    # NOTE: Walt's saved actor is labelled "SpotLight"; the build reproduces it as "Lamp_AccentSpot".
    for a in list(eas.get_all_level_actors()):
        if a.get_actor_label() == "Lamp_AccentSpot":
            eas.destroy_actor(a)
    spot = eas.spawn_actor_from_class(unreal.SpotLight, unreal.Vector(-98.48, 105.29, 143.23),
                                      unreal.Rotator(roll=90.0, pitch=-10.0, yaw=-90.0))
    spot.set_actor_label("Lamp_AccentSpot")
    sc = spot.get_component_by_class(unreal.SpotLightComponent)
    sc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    sc.set_editor_property("intensity", 5.0)
    sc.set_editor_property("light_color", unreal.Color(255, 236, 150, 255))
    sc.set_editor_property("attenuation_radius", 600.0)
    sc.set_editor_property("inner_cone_angle", 20.0)
    sc.set_editor_property("outer_cone_angle", 44.0)

    les.save_current_level()
    al.save_asset(MAP)

    # 5) Verify -------------------------------------------------------------------
    o, e = p.get_actor_bounds(False)
    unreal.log("VERIFY plane AABB X[%.0f,%.0f] Y[%.1f,%.1f] Z[%.0f,%.0f]"
               % (o.x-e.x, o.x+e.x, o.y-e.y, o.y+e.y, o.z-e.z, o.z+e.z))
    unreal.log("VERIFY plane material[0] = %s" % (pc.get_material(0).get_name() if pc.get_material(0) else "None"))
    for ap in (tex_path, mat_path, mi_path):
        unreal.log("VERIFY exists %s -> %s" % (ap, al.does_asset_exist(ap)))
    unreal.log("VERIFY SM_Board_A1 untouched (separate actor; material on plane only)")
    if warnings:
        unreal.log_warning("issues: %s" % "; ".join(warnings))
    unreal.log("=== Phase 2 complete ===")

main()
