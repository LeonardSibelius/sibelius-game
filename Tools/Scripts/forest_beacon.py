# forest_beacon.py — SIB Forest findability: a tall aurora light-column beacon above the canopy at
# the return door + a glowing portal look on the door itself. Saved emissive MI (persists), placed
# beacon column + point light, and a per-instance emissive override on the door mesh. Run via bridge.
import unreal
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# 1) Aurora emissive MI (parent = the kit's emissive lamp material the curio also uses).
MI = "/Game/Forest/MI_DreamBeacon"
if not unreal.EditorAssetLibrary.does_asset_exist(MI):
    mi = at.create_asset("MI_DreamBeacon", "/Game/Forest", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    parent = unreal.load_asset("/Game/ModularSciFiEnv_K/Materials/Base/M_LampEmiss_MAT.M_LampEmiss_MAT")
    if parent:
        mi.set_editor_property("parent", parent)
    aurora = unreal.LinearColor(0.30, 1.0, 0.80, 1.0)
    for p in ("Emissive", "BaseColor"):
        mel.set_material_instance_vector_parameter_value(mi, p, aurora)
    mel.set_material_instance_scalar_parameter_value(mi, "Intens", 18.0)
    mel.set_material_instance_scalar_parameter_value(mi, "TurnOn", 1.0)
    unreal.EditorAssetLibrary.save_asset(MI)
mi = unreal.load_asset(MI)

les.load_level("/Game/Maps/L_Elsewhere_Forest")
door = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "ReturnDoor_Forest":
        door = a
        break
if not door:
    unreal.log_error("###BEACON### no ReturnDoor_Forest — place the return door first.")
else:
    dl = door.get_actor_location()
    for a in list(eas.get_all_level_actors()):
        if a.get_actor_label() in ("ForestBeacon_Column", "ForestBeacon_Light"):
            eas.destroy_actor(a)

    # Tall thin emissive column rising ~75 m (cylinder centre at 37.5 m). Visible across the 508 m forest.
    col = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(dl.x, dl.y, 3750.0))
    col.set_actor_label("ForestBeacon_Column")
    col.set_actor_scale3d(unreal.Vector(1.6, 1.6, 75.0))
    smc = col.static_mesh_component
    smc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder"))
    smc.set_material(0, mi)
    smc.set_editor_property("cast_shadow", False)
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

    # Aurora point light at the door base (a halo so the ground around home glows too).
    pl = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(dl.x, dl.y, dl.z + 250.0))
    pl.set_actor_label("ForestBeacon_Light")
    lc = pl.point_light_component
    lc.set_intensity(80000.0)
    lc.set_attenuation_radius(2500.0)
    lc.set_light_color(unreal.LinearColor(0.30, 1.0, 0.80, 1.0))
    lc.set_editor_property("cast_shadows", False)

    # Glowing portal look on the door itself (per-instance emissive override on the root door mesh).
    if door.root_component:
        door.root_component.set_material(0, mi)

    les.save_current_level()
    unreal.log("###BEACON### column + light + door glow placed at (%.0f,%.0f); saved." % (dl.x, dl.y))
