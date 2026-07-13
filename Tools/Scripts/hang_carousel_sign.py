# hang_carousel_sign.py — hang the Code-Vision inscription on the Carousel door.
#
# The texture already exists (Content/Signs/T_sign_carousel — Walt's art).
# Assign it to CarouselDoor_Kitchen.SignTexture with the same sign geometry as
# SauceDoor_Kitchen, so the kitchen's two secrets hang their signs identically.
# PK16: assigning on the placed actor is also what gets the texture cooked.
# Idempotent. Run via bridge or -run=pythonscript.

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/L_Office_v02")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find(label):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


door = find("CarouselDoor_Kitchen")
sauce = find("SauceDoor_Kitchen")
tex = unreal.load_asset("/Game/Signs/T_sign_carousel")

if not door:
    print("RESULT: CarouselDoor_Kitchen not found")
elif not tex:
    print("RESULT: T_sign_carousel texture not found")
else:
    door.set_editor_property("SignTexture", tex)
    copied = "defaults"
    if sauce:
        for prop in ("SignRelativeLocation", "SignRelativeRotation", "SignWidth", "SignHeight"):
            try:
                door.set_editor_property(prop, sauce.get_editor_property(prop))
                copied = "copied from SauceDoor_Kitchen"
            except Exception:
                pass
    # OnConstruction hangs the sign when SignTexture is set — it re-runs on
    # every level load, so the next open shows it. (rerun_construction_scripts
    # is not exposed to Python in 5.7.)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("RESULT: sign hung (geometry %s), level saved" % copied)
