# build_fate_altar.py — the Carousel of Fates altar (SIB-34 set dressing).
#
# Does three things, idempotently, in L_Cathedral (OPEN IT FIRST):
#   1. Creates /Game/SlotFactory/Materials/M_fate_base — unlit, masked, glowing
#      card material with a "Sprite" texture parameter + "Glow" scalar. The
#      AFateCarousel actor builds its nine orbiting symbol cards from MIDs of
#      this base. RUN THIS BEFORE PLACING THE CAROUSEL.
#   2. Restyles the placed SlotCabinet into a MARBLE PLINTH (black marble from
#      the StainedGlass3D skins we own; squat altar proportions). The E-screen
#      logic is untouched — the plinth IS the interactable.
#   3. Stands two lights over the altar: a warm gold point light at carousel
#      height + a soft cool fill, tagged FateAltar (cleared on re-run).
#
# RUN NATIVELY (editor Cmd box, never the bridge):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_fate_altar.py"
# Then: close editor -> Build.bat (for AFateCarousel) -> reopen -> place
# "Fate Carousel" at (3400, 0, 330) -> PIE.

import unreal

TAG = "###FATE###"
MAT_DIR = "/Game/SlotFactory/Materials"
ALTAR_TAG = "FateAltar"
APSE = unreal.Vector(3400.0, 0.0, 0.0)
MARBLE = "/Game/StainedGlass3D/Materials/M_BlackMarbleFloor"


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary

    # ---- 1. M_fate_base ----------------------------------------------------
    full = "%s/M_fate_base" % MAT_DIR
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mat = asset_tools.create_asset("M_fate_base", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.25)

    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    ts.set_editor_property("parameter_name", "Sprite")

    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 260)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 5.0)

    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -420, 60)
    mel.connect_material_expressions(ts, "", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # the vector sprites carry real alpha — use it directly as the mask
    mel.connect_material_property(ts, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

    mel.recompile_material(mat)
    eal.save_asset(full)
    unreal.log("%s M_fate_base created (Sprite param + Glow=5, alpha-masked, unlit)" % TAG)

    # ---- 2. plinth ----------------------------------------------------------
    actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    cab = actors.get("SlotCabinet")
    if cab:
        cab.set_actor_location(unreal.Vector(APSE.x, APSE.y, 60.0), False, False)
        mesh = None
        for c in cab.get_components_by_class(unreal.StaticMeshComponent):
            mesh = c
            break
        if mesh:
            cube = unreal.load_asset("/Engine/BasicShapes/Cube")
            if cube:
                mesh.set_static_mesh(cube)
            marble = unreal.load_asset(MARBLE)
            if marble:
                mesh.set_material(0, marble)
                unreal.log("%s plinth: black marble applied" % TAG)
            else:
                unreal.log("%s plinth: %s not found — grey it stays (fix by hand)" % (TAG, MARBLE))
            # squat altar block: 160 x 160 x 120 cm
            mesh.set_relative_scale3d(unreal.Vector(1.6, 1.6, 1.2))
        unreal.log("%s plinth: SlotCabinet restyled at (%.0f, %.0f, 60)" % (TAG, APSE.x, APSE.y))
    else:
        unreal.log_error("%s SlotCabinet actor not found in this level — open L_Cathedral!" % TAG)

    # ---- 3. lights (idempotent) ---------------------------------------------
    for a in list(eas.get_all_level_actors()):
        try:
            if unreal.Name(ALTAR_TAG) in a.tags:
                eas.destroy_actor(a)
        except Exception:
            pass

    gold = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(APSE.x, APSE.y, 360.0), unreal.Rotator(roll=0, pitch=0, yaw=0))
    gc = gold.get_component_by_class(unreal.PointLightComponent)
    gc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    gc.set_editor_property("intensity", 12000.0)
    gc.set_editor_property("attenuation_radius", 900.0)
    gc.set_light_color(unreal.LinearColor(1.0, 0.82, 0.45, 1.0))
    gold.set_actor_label("FateLight_Gold")
    gold.tags = [unreal.Name(ALTAR_TAG)]

    cool = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(APSE.x - 180.0, APSE.y, 200.0), unreal.Rotator(roll=0, pitch=0, yaw=0))
    cc = cool.get_component_by_class(unreal.PointLightComponent)
    cc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    cc.set_editor_property("intensity", 3500.0)
    cc.set_editor_property("attenuation_radius", 600.0)
    cc.set_light_color(unreal.LinearColor(0.7, 0.8, 1.0, 1.0))
    cool.set_actor_label("FateLight_Fill")
    cool.tags = [unreal.Name(ALTAR_TAG)]

    unreal.log("%s lights placed. NEXT: close editor -> Build.bat -> reopen -> Place Actors 'Fate Carousel' at (3400, 0, 330) -> save -> PIE." % TAG)


main()
