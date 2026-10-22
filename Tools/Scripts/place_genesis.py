# place_genesis.py - fills the empty L_Genesis (make_genesis_assets.py) with the AI city:
# one AAICity at the origin (the star's centre) and the PlayerStart the seed upstairs sends him
# to (tag GenesisGallery). Where he starts hardly matters since 2026-10-22: the city takes him
# at once and flies him in from 25 km out (AAICity::StartFlight). The start stands where he
# lands, below the star, so a pawn the city has not yet taken is not left floating anywhere odd.
# The universe forge it used to place is gone with the universe game (Walt, 2026-10-22).
#
# *** OPEN /Game/Maps/L_Genesis FIRST (File > Open Level), PIE stopped, THEN RUN ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_genesis.py"
#
# Refuses unless L_Genesis is the open level, and never changes levels itself. Safe to re-run:
# it moves the city it finds back to the origin and replaces the start. Does not save: Ctrl+S.

import unreal

TAG = "###GENESIS###"
LEVEL = "/Game/Maps/L_Genesis"
ARRIVAL = "GenesisGallery"          # the arrival tag AICityPortal (place_aicity_portal.py) travels with
START_LABEL = "PlayerStart_GenesisGallery"
LANDING_DEPTH = 327479.0            # pad A's floor below the star once the city has turned (AICity.cpp)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def main():
    here = ues.get_editor_world().get_path_name()
    if LEVEL not in here:
        say("REFUSING: the open level is %s, not %s. Nothing changed." % (here, LEVEL))
        return
    city_class = unreal.load_class(None, "/Script/SibeliusGame.AICity")
    if not city_class:
        say("AICity class not built yet - rebuild the editor first.")
        return
    actors = eas.get_all_level_actors()
    cities = [a for a in actors if a.get_class().get_name() == "AICity"]
    with unreal.ScopedEditorTransaction("Place the AI city"):
        city = cities[0] if cities else eas.spawn_actor_from_class(city_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        city.set_actor_location(unreal.Vector(0, 0, 0), False, False)
        city.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
        city.set_actor_label("AICity")
        for old in [a for a in actors if a.get_actor_label() == START_LABEL]:
            eas.destroy_actor(old)
        start = unreal.Vector(0.0, 0.0, -LANDING_DEPTH + 100.0)
        ps = eas.spawn_actor_from_class(unreal.PlayerStart, start, unreal.Rotator(0.0, 0.0, 0.0))
        ps.set_editor_property("player_start_tag", ARRIVAL)
        ps.set_actor_label(START_LABEL)
    say("AICity at the origin (%s); %s (tag %s) at %s. NOT saved - Ctrl+S." % (
        "moved" if cities else "new", START_LABEL, ARRIVAL, start))


main()
