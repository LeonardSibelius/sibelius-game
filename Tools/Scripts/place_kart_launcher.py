# place_kart_launcher.py — the go-kart in the living room that starts the Transhuman Grand Prix.
#
# Walt, 2026-10-09: "Can we build something similar, launched from an object in the living
# room?" The object is a kart: the same plain-shapes kart he will race (AKartLauncher). [E]
# on it travels to L_Kart.
#
# WHERE: in front of BP_Cabinet_C1 (Walt: "can you put the cart in front of BP_Cabinet_C1?",
# after he had taken out another cabinet for walking space). The cabinet's back is on the
# wall at -X, so the kart stands 15 cm off its front, nose and sign to the room, a little
# toward +Y so the outside door beside the cabinet keeps 90 cm and the level's PlayerStart
# in front of it keeps its own room.
#
# THE FLOOR IS THE CABINET'S FOOT, NOT A TRACE: the first version traced for the floor, and
# the living room's floor does not answer the Visibility channel - the kart went 80 cm down,
# onto the ground under the house, out of sight.
#
# RUN with L_Office_v02 open, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_kart_launcher.py"
# Safe to re-run: it replaces the KartLauncher it made. DRY_RUN only reports the spot.

import unreal

TAG = "###KARTLAUNCHER###"
DRY_RUN = True
ANCHOR = "BP_Cabinet_C1"
OFF_FRONT = 15.0           # cm from the cabinet's front to the kart's tail
TAIL = 100.0               # the kart's tail is this far behind its centre
SIDEWAYS_Y = 9205.0        # the kart's centre line, clear of the outside door at y ~ 9040

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def main():
    acts = eas.get_all_level_actors()
    cab = next((a for a in acts if a.get_actor_label() == ANCHOR), None)
    if not cab:
        say("no %s here - open L_Office_v02." % ANCHOR)
        return
    o, e = cab.get_actor_bounds(False)
    spot = unreal.Vector(o.x + e.x + OFF_FRONT + TAIL, SIDEWAYS_Y, o.z - e.z)
    if DRY_RUN:
        say("DRY RUN: the go-kart would stand at (%.0f, %.0f, %.0f) facing +X." % (spot.x, spot.y, spot.z))
        return
    for old in [a for a in acts if a.get_class().get_name() == "KartLauncher"]:
        eas.destroy_actor(old)
    kart_class = unreal.load_class(None, "/Script/SibeliusGame.KartLauncher")
    kart = eas.spawn_actor_from_class(kart_class, spot, unreal.Rotator(0.0, 0.0, 0.0))
    kart.set_actor_label("KartLauncher")
    les.save_current_level()
    say("KartLauncher in front of %s at (%.0f, %.0f, %.0f) -> L_Kart. Saved." % (ANCHOR, spot.x, spot.y, spot.z))


main()
