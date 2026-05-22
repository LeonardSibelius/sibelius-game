# build_office_blockout.py
# Checkpoint 2 — grey-box office room for the Sibelius one-room MVP.
#
# Builds /Game/Maps/L_Office_MVP from engine primitive meshes: room shell with a
# window opening, desk, chair, dual monitors, a bookshelf, and a HIDDEN DOOR on the
# wall behind the bookshelf. Adds a golden-hour directional sun, sky light,
# sky atmosphere, height fog, and an unbound post-process volume (Lumen GI is already
# enabled project-wide in DefaultEngine.ini).
#
# Megascans art is deferred (free Megascans-via-Fab ended 2024-12-31); these primitives
# are dimensioned to the real layout so art can be swapped in later without moving anything.
#
# Run headless:
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript \
#     -script="<abs path>/Tools/build_office_blockout.py" \
#     -unattended -nopause -nosplash -EnablePlugins=PythonScriptPlugin
#
# Units are centimetres (Unreal world units). Interior box: 600 (X) x 500 (Y) x 300 (Z).

import unreal

MAP_PATH = "/Game/Maps/L_Office_MVP"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_lib = unreal.EditorAssetLibrary

CUBE = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")

created = []
warnings = []


def _mesh_component(actor):
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    return comp


def box(label, center_cm, size_cm, tags=None, stencil=None):
    """Spawn a labelled cube scaled to size_cm (a 3-tuple), centred at center_cm."""
    loc = unreal.Vector(*center_cm)
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, unreal.Rotator(0, 0, 0))
    comp = _mesh_component(actor)
    comp.set_static_mesh(CUBE)
    # BasicShapes/Cube is 100cm per side, pivot at centre -> scale = size / 100.
    actor.set_actor_scale3d(unreal.Vector(size_cm[0] / 100.0, size_cm[1] / 100.0, size_cm[2] / 100.0))
    actor.set_actor_label(label)
    if tags:
        actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
    if stencil is not None:
        # For the future Code Vision custom-depth/stencil reveal (Checkpoint 3).
        comp.set_editor_property("render_custom_depth", True)
        comp.set_editor_property("custom_depth_stencil_value", stencil)
    created.append(label)
    return actor


def main():
    unreal.log("=== Building L_Office_MVP grey-box ===")
    # Idempotent: delete any prior build so a re-run doesn't duplicate actors.
    if asset_lib.does_asset_exist(MAP_PATH):
        unreal.log_warning("Map exists; deleting for a clean rebuild: %s" % MAP_PATH)
        asset_lib.delete_asset(MAP_PATH)
    les.new_level(MAP_PATH)

    T = 20          # wall thickness
    W, D, H = 600, 500, 300   # interior width(X), depth(Y), height(Z)
    hx, hy = W / 2, D / 2

    # ---- Shell ------------------------------------------------------------
    box("Floor_Concrete", (0, 0, -T / 2), (W + 2 * T, D + 2 * T, T))
    box("Ceiling", (0, 0, H + T / 2), (W + 2 * T, D + 2 * T, T))
    box("Wall_West", (-(hx + T / 2), 0, H / 2), (T, D, H))   # window-opposite side
    box("Wall_East", (hx + T / 2, 0, H / 2), (T, D, H))

    # North wall (y = +hy) = WINDOW wall: build around a 240w x 150h opening, sill at z=90.
    win_w, win_h, sill = 240, 150, 90
    side = (W - win_w) / 2
    box("Wall_North_L", (-(win_w / 2 + side / 2), hy + T / 2, H / 2), (side, T, H))
    box("Wall_North_R", (win_w / 2 + side / 2, hy + T / 2, H / 2), (side, T, H))
    box("Wall_North_Sill", (0, hy + T / 2, sill / 2), (win_w, T, sill))
    header_h = H - (sill + win_h)
    box("Wall_North_Header", (0, hy + T / 2, sill + win_h + header_h / 2), (win_w, T, header_h))
    # Window glass placeholder — tagged translucent so predicted bug #3 (Code Vision shader
    # breaking on transparent surfaces) has a concrete test target in Checkpoint 3.
    box("Window_Glass", (0, hy + T / 2, sill + win_h / 2), (win_w, 4, win_h),
        tags=["CodeVision.Translucent"])

    # South wall (y = -hy) = solid; bookshelf + hidden door live here.
    box("Wall_South", (0, -(hy + T / 2), H / 2), (W, T, H))

    # ---- Furniture --------------------------------------------------------
    box("Desk_Oak", (0, hy - 120, 37), (180, 80, 75), tags=["CodeVision.Label=Desk_Oak"])
    box("Chair_Office", (0, hy - 210, 45), (55, 55, 90), tags=["CodeVision.Label=Chair_Office"])
    box("Monitor_L", (-40, hy - 95, 95), (50, 6, 32), tags=["CodeVision.Label=Monitor"])
    box("Monitor_R", (40, hy - 95, 95), (50, 6, 32), tags=["CodeVision.Label=Monitor"])

    # Bookshelf sits flush against the south wall, in front of the hidden door.
    box("Bookshelf_Wood", (0, -(hy - 18), 130), (200, 36, 260),
        tags=["CodeVision.Label=Bookshelf_Wood", "CodeVision.Reveal"], stencil=2)

    # Hidden door — embedded in the south wall, directly behind the bookshelf.
    # Invisible without Code Vision; stays revealed after first reveal (Checkpoint 3 logic).
    door = box("Door_Hidden_Ch1Exit", (0, -(hy + 1), 105), (90, 8, 210),
               tags=["CodeVision.Hidden", "CodeVision.Label=Door_Hidden_Ch1Exit"], stencil=1)

    # ---- PlayerStart (third-person spawn, facing the bookshelf/south wall) ----
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, hy - 260, 95),
                                    unreal.Rotator(0, -90, 0))
    ps.set_actor_label("PlayerStart")
    created.append("PlayerStart")

    # ---- Lighting (golden hour) ------------------------------------------
    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 280),
                                     unreal.Rotator(0, 0, 0))
    # Low warm sun streaming in from the north window: pitch -10 (just above horizon),
    # yaw so the beam crosses the room toward the south wall.
    sun.set_actor_rotation(unreal.Rotator(-10.0, -100.0, 0.0), False)
    sun.set_actor_label("Sun_GoldenHour")
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_editor_property("intensity", 6.0)
    sc.set_light_color(unreal.LinearColor(1.0, 0.78, 0.46, 1.0))
    sc.set_editor_property("atmosphere_sun_light", True)
    sc.set_editor_property("use_temperature", True)
    sc.set_editor_property("temperature", 4200.0)
    created.append("Sun_GoldenHour")

    for label, cls in [("SkyAtmosphere", unreal.SkyAtmosphere),
                       ("SkyLight", unreal.SkyLight),
                       ("HeightFog", unreal.ExponentialHeightFog)]:
        try:
            a = eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 150), unreal.Rotator(0, 0, 0))
            a.set_actor_label(label)
            created.append(label)
        except Exception as e:
            warnings.append("%s: %s" % (label, e))

    # Sky light needs real-time capture to pick up the atmosphere (else it renders black).
    try:
        for a in eas.get_all_level_actors():
            if isinstance(a, unreal.SkyLight):
                slc = a.get_component_by_class(unreal.SkyLightComponent)
                slc.set_editor_property("real_time_capture", True)
    except Exception as e:
        warnings.append("skylight recapture: %s" % e)

    # ---- Post-process volume (unbound): warm grade, bloom, exposure -------
    try:
        ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 150),
                                         unreal.Rotator(0, 0, 0))
        ppv.set_actor_label("PostProcess_Office")
        ppv.set_editor_property("unbound", True)
        s = ppv.get_editor_property("settings")
        s.set_editor_property("override_bloom_intensity", True)
        s.set_editor_property("bloom_intensity", 0.6)
        s.set_editor_property("override_auto_exposure_method", True)
        s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM)
        s.set_editor_property("override_auto_exposure_min_brightness", True)
        s.set_editor_property("auto_exposure_min_brightness", 0.3)
        s.set_editor_property("override_auto_exposure_max_brightness", True)
        s.set_editor_property("auto_exposure_max_brightness", 2.0)
        # Warm shadow tones / slightly cool highlights to match leonardsibelius.com palette.
        s.set_editor_property("override_color_grading_intensity", True)
        s.set_editor_property("override_white_temp", True)
        s.set_editor_property("white_temp", 5200.0)
        ppv.set_editor_property("settings", s)
        created.append("PostProcess_Office")
    except Exception as e:
        warnings.append("PostProcessVolume: %s" % e)

    # ---- Save -------------------------------------------------------------
    les.save_current_level()
    asset_lib.save_asset(MAP_PATH)

    unreal.log("=== L_Office_MVP build complete: %d actors ===" % len(created))
    for c in created:
        unreal.log("  + %s" % c)
    if warnings:
        unreal.log_warning("Non-fatal issues (%d):" % len(warnings))
        for w in warnings:
            unreal.log_warning("  ! %s" % w)
    exists = asset_lib.does_asset_exist(MAP_PATH)
    unreal.log("ASSERT map asset exists at %s -> %s" % (MAP_PATH, exists))
    if not exists:
        raise RuntimeError("L_Office_MVP was not saved")


main()
