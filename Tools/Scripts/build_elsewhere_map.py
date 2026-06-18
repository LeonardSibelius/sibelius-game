# build_elsewhere_map.py — SIB-47 Sauce Door: the Elsewhere generation level + mood.
#
# Builds /Game/Maps/L_Elsewhere: a faint cool skylight (NO directional sun — dark void),
# a PlayerStart inside the west doorway, one AElsewhereBuilder at origin (preview =
# Server Cathedral), the clean ElsewhereGameMode override, and the ATMOSPHERE (static):
#   - ExponentialHeightFog with volumetric scattering (so the god-ray shafts read), dark
#     navy inscattering so anything past the walls is darkness, not a day sky.
#   - A cinematic PostProcessVolume (navy + gold grade, gentle bloom + vignette).
#   - The enclosed temple sits in a dark void; warm-gold shafts + the curio glow carry it.
# The slot-aligned god-ray shafts themselves are spawned by AElsewhereBuilder (so they
# line up with the room grid). This pass is BY-EYE — see the TUNE KNOBS below.
#
# Idempotent. Run (editor target built, editor CLOSED is fine via -run=pythonscript):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_elsewhere_map.py"

import unreal

MAP = "/Game/Maps/L_Elsewhere"
TAG = "###ELSEWHERE###"

# --- TUNE KNOBS (static atmosphere) — Walt tweaks these by eye ---
# NOTE: no directional sun — the enclosed temple sits in a DARK VOID. A directional
# sun lit a bright day-sky through the fog/gaps ("outdoors at noon"); removing it makes
# anything past the walls read as dark navy fog, and the warm-gold shafts carry the
# light. (If you want a faint directional fill later, add a DirectionalLight at low
# intensity with Atmosphere-Sun OFF.)
SKY_INTENSITY  = 0.15    # faint cool skylight ambient; lower = the shafts/glow carry more
FOG_DENSITY    = 0.02    # exponential height fog density (subtle = light has body)
BLOOM          = 0.8     # gentle bloom
VIGNETTE       = 0.4     # cinematic edge darkening
GRADE_STRENGTH = 1.0     # color-grade intensity (0..1); navy shadows + gold highlights
SHADOW_NAVY    = [0.55, 0.70, 1.00, 1.0]  # color gain on shadows (blue/navy)
HIGHLIGHT_GOLD = [1.00, 0.85, 0.55, 1.0]  # color gain on highlights (warm gold)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

builder_cls = unreal.load_class(None, "/Script/SibeliusGame.ElsewhereBuilder")
gm_cls = unreal.load_class(None, "/Script/SibeliusGame.ElsewhereGameMode")


def tryset(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as e:
        unreal.log_warning("%s set %s failed: %r" % (TAG, name, e))
        return False


if builder_cls is None:
    unreal.log_error("%s AElsewhereBuilder not found — Build.bat the SibeliusGameEditor target first." % TAG)
else:
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        unreal.EditorAssetLibrary.delete_asset(MAP)
    les.new_level(MAP)

    # No directional sun — dark void. Faint cool skylight only; the shafts + curio glow
    # carry the mood.
    sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800))
    sky.set_actor_label("SkyLight")
    kc = sky.get_component_by_class(unreal.SkyLightComponent)
    if kc:
        kc.set_intensity(SKY_INTENSITY)

    # Volumetric height fog — gives light body so the shafts read as beams.
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label("HeightFog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fc:
        tryset(fc, "fog_density", FOG_DENSITY)
        tryset(fc, "fog_inscattering_luminance", unreal.LinearColor(0.02, 0.04, 0.10, 1.0))  # cool navy haze
        tryset(fc, "fog_height_falloff", 0.05)
        tryset(fc, "enable_volumetric_fog", True)                       # REQUIRED for the shafts
        tryset(fc, "volumetric_fog_scattering_distribution", 0.4)
        tryset(fc, "volumetric_fog_albedo", unreal.Color(160, 185, 230, 255))
        tryset(fc, "volumetric_fog_extinction_scale", 1.0)

    # Cinematic grade: navy shadows + gold highlights, gentle bloom + vignette (unbound
    # so it grades the whole hall).
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    ppv.set_actor_label("CathedralGrade")
    tryset(ppv, "unbound", True)
    s = ppv.get_editor_property("settings")
    tryset(s, "override_bloom_intensity", True);          tryset(s, "bloom_intensity", BLOOM)
    tryset(s, "override_vignette_intensity", True);       tryset(s, "vignette_intensity", VIGNETTE)
    tryset(s, "override_color_grading_intensity", True);  tryset(s, "color_grading_intensity", GRADE_STRENGTH)
    tryset(s, "override_color_gain_shadows", True);       tryset(s, "color_gain_shadows", unreal.Vector4(*SHADOW_NAVY))
    tryset(s, "override_color_gain_highlights", True);    tryset(s, "color_gain_highlights", unreal.Vector4(*HIGHLIGHT_GOLD))
    tryset(s, "override_color_saturation", True);         tryset(s, "color_saturation", unreal.Vector4(1.0, 1.0, 1.0, 0.92))
    ppv.set_editor_property("settings", s)

    # Just inside the west doorway gap, facing +X into the hall.
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-800.0, 0.0, 120.0), unreal.Rotator(0.0, 0.0, 0.0))
    ps.set_actor_label("PlayerStart")

    builder = eas.spawn_actor_from_class(builder_cls, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    builder.set_actor_label("ElsewhereBuilder")
    builder.set_editor_property("bPreviewWhenUnstaged", True)
    builder.set_editor_property("PreviewPlaceType", "ServerCathedral")

    # Clean GameMode override (no build HUD; skips the deploy-save restore).
    if gm_cls:
        world = ues.get_editor_world()
        for ws in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings):
            ws.set_editor_property("default_game_mode", gm_cls)

    les.save_current_level()
    unreal.log("%s built %s (Server Cathedral preview + atmosphere). PIE to walk it." % (TAG, MAP))
