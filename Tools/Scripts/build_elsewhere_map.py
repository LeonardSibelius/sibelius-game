# build_elsewhere_map.py — SIB-47 Sauce Door: the Elsewhere generation level (June 2026).
#
# Builds /Game/Maps/L_Elsewhere: a sun + skylight, a PlayerStart just inside the west
# doorway, and ONE AElsewhereBuilder at origin. The builder generates the room at
# BeginPlay from the plan the Sauce Door staged before travel; opened directly (no
# staged plan) it builds a PREVIEW of the Server Cathedral (bPreviewWhenUnstaged=True)
# so the dressing can be reviewed standalone.
#
# The room geometry comes from the place-type's kit palette (Crebotoly for the Server
# Cathedral); with the kit not yet installed the builder falls back to engine shapes,
# so the room STRUCTURE renders either way. NO marketplace bytes are committed.
#
# ORDER: Build.bat the editor target FIRST (the C++ classes must exist), then run:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_elsewhere_map.py"
# Then PIE L_Elsewhere to walk the Server Cathedral; or PIE from the kitchen via the
# Sauce Door for the full loop.

import unreal

MAP = "/Game/Maps/L_Elsewhere"
TAG = "###ELSEWHERE###"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

builder_cls = unreal.load_class(None, "/Script/SibeliusGame.ElsewhereBuilder")
gm_cls = unreal.load_class(None, "/Script/SibeliusGame.ElsewhereGameMode")
if builder_cls is None:
    unreal.log_error("%s AElsewhereBuilder not found — Build.bat the SibeliusGameEditor target first." % TAG)
else:
    # Idempotent: new_level refuses to overwrite, so drop any prior version first.
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        unreal.EditorAssetLibrary.delete_asset(MAP)
    les.new_level(MAP)

    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(-55.0, -40.0, 0.0))
    sun.set_actor_label("Sun")
    # Dimmed so the enclosed hall + ceiling don't blow out (the builder's MoodLight
    # carries the interior). Tune to taste.
    sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if sun_comp:
        sun_comp.set_intensity(2.0)
    sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800))
    sky.set_actor_label("SkyLight")
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    if sky_comp:
        sky_comp.set_intensity(0.4)

    # Just inside the west doorway gap (room is centered on the builder at origin),
    # facing +X into the hall.
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-800.0, 0.0, 120.0), unreal.Rotator(0.0, 0.0, 0.0))
    ps.set_actor_label("PlayerStart")

    builder = eas.spawn_actor_from_class(builder_cls, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    builder.set_actor_label("ElsewhereBuilder")
    # Standalone preview of the dressed place-type; a real arrival stages its own plan.
    builder.set_editor_property("bPreviewWhenUnstaged", True)
    builder.set_editor_property("PreviewPlaceType", "ServerCathedral")

    # Clean GameMode override: no build HUD overlay, and UBranchPIEComponent skips the
    # deploy-save restore here (no stray generated build sites on the curio).
    if gm_cls:
        world = ues.get_editor_world()
        for ws in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings):
            ws.set_editor_property("default_game_mode", gm_cls)

    les.save_current_level()
    unreal.log("%s built %s (preview=ServerCathedral). PIE it to walk the room; "
               "arrive via the Sauce Door for the full loop." % (TAG, MAP))
