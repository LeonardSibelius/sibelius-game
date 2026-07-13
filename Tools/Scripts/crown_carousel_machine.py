# crown_carousel_machine.py — the fate-glyph ring crowns the Carousel machine.
#
# Walt's call: the halo from the AI apparition (which borrowed the Carousel's
# own nine glyphs) comes home. Places an AFateCarousel above CarouselMachine
# in L_Carousel — pure set dressing, orbiting glyph cards, no new code.
# Idempotent. Run via bridge (editor open) or -run=pythonscript (closed).

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_Carousel")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

machine = None
crown = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "CarouselMachine":
        machine = a
    elif a.get_actor_label() == "FateCrown":
        crown = a

if not machine:
    print("RESULT: CarouselMachine not found")
elif crown:
    print("RESULT: FateCrown already placed")
else:
    loc = machine.get_actor_location() + unreal.Vector(0.0, 0.0, 330.0)
    cls = unreal.load_class(None, "/Script/SibeliusGame.FateCarousel")
    ring = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(0.0, 0.0, 0.0))
    ring.set_actor_label("FateCrown")
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print("RESULT: FateCrown placed above the machine at %s" % loc)
