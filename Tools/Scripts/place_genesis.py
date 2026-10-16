# place_genesis.py - fills the empty L_Genesis (make_genesis_assets.py) with the AI city:
# one AAICity at the origin (the star's centre) and the PlayerStart the seed upstairs sends him
# to (tag GenesisGallery), on the gallery a few metres back from its middle, facing the star.
#
# *** OPEN /Game/Maps/L_Genesis FIRST (File > Open Level), PIE stopped, THEN RUN ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_genesis.py"
#
# Refuses unless L_Genesis is the open level, and never changes levels itself. Safe to re-run:
# it moves the city it finds back to the origin and replaces the start. Saves the level.

import unreal

TAG = "###GENESIS###"
LEVEL = "/Game/Maps/L_Genesis"
ARRIVAL = "GenesisGallery"          # AUniverseSeed::ArrivalTag
START_LABEL = "PlayerStart_GenesisGallery"
BACK = 600.0                        # cm behind the middle of the gallery, away from the star

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


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
    city = cities[0] if cities else eas.spawn_actor_from_class(city_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    city.set_actor_location(unreal.Vector(0, 0, 0), False, False)
    city.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
    city.set_actor_label("AICity")

    for old in [a for a in actors if a.get_actor_label() == START_LABEL]:
        eas.destroy_actor(old)
    distance = city.get_editor_property("gallery_distance")
    start = unreal.Vector(-distance - BACK, 0.0, 100.0)
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, start, unreal.Rotator(0.0, 0.0, 0.0))
    ps.set_editor_property("player_start_tag", ARRIVAL)
    ps.set_actor_label(START_LABEL)
    les.save_current_level()
    say("AICity at the origin (%s); %s (tag %s) at %s facing the star. Saved %s." % (
        "moved" if cities else "new", START_LABEL, ARRIVAL, start, LEVEL))


main()
