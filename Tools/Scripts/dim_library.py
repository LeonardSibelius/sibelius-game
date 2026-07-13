# dim_library.py — bring the library's exposure down (Walt: "too bright").
#
# Adds one unbound PostProcessVolume ("PP_LibraryDim") to L_Carousel with a
# negative exposure bias — a single Details knob Walt can tune by eye
# (Exposure > Exposure Compensation). Idempotent.

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_Carousel")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if any(a.get_actor_label() == "PP_LibraryDim" for a in eas.get_all_level_actors()):
    print("RESULT: PP_LibraryDim already placed")
else:
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume,
                                     unreal.Vector(0.0, 900.0, 300.0), unreal.Rotator(0.0, 0.0, 0.0))
    ppv.set_actor_label("PP_LibraryDim")
    ppv.set_editor_property("unbound", True)
    # Struct copies in python: fetch, mutate, write back.
    s = ppv.get_editor_property("settings")
    s.set_editor_property("override_auto_exposure_bias", True)
    s.set_editor_property("auto_exposure_bias", -1.25)
    ppv.set_editor_property("settings", s)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("RESULT: PP_LibraryDim placed (exposure bias -1.25), level saved")
