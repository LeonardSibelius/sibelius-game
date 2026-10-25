# clear_temple_dust.py - take Olympus's drifting dust, and the dirt on its marble, out of the
# Archaic Language Temple.
#
# Walt, 2026-10-24: "in that Archaic Language Temple downstairs, there are irritating particles
# flying through the air and when they land on the floor they produce an annoying dirty rain
# effect". Two things, as it turned out:
#
#   THE DUST. The level's only particle effect is the pack's N_Dust (/Game/Olympus/Dust/BP/N_Dust,
#   one "HangingParticulates" emitter of sprites drifting on curl noise), placed as a NiagaraActor.
#   It goes; the god-ray cards (BP_LightBeam), which are not particles, stay.
#
#   THE "RAIN". The floors' master (M_Triplanar_Master) lays a grunge mask - a brushstroke texture -
#   over every surface: base colour x lerp(ColorGrunge, 1, mask), and roughness x a factor that runs
#   from RoughnessGrunge to 2 - RoughnessGrunge across the mask. MI_CaynMarble, the floor in front of
#   every statue, had a blue ColorGrunge and RoughnessGrunge 3.25, so it was blotched dark blue and
#   swung from rough to mirror in patches that reflected the sky: spots, like rain on dirt. White
#   and 1 make both multipliers 1 - plain polished marble. MI_GrayMarble (the nave's floors) had the
#   same in dark grey at 1.87. The pack's own green tiles already ship this way.
#
# The dust is one transaction: Ctrl+Z puts it back. The level is NOT saved - Walt walks it, then
# Ctrl+S. The two marbles are the pack's assets, not the level's, so they are saved here, and only
# when SAVE_MARBLE is True (Walt looks first); only the temple uses Olympus. Run this again after a
# re-download of the pack: safe to re-run, it changes only what is not yet changed.
#
# *** RUN WITH L_Archaic_Temple OPEN, PIE stopped ***

import unreal

TAG = "###DUST###"
MAP = "/Game/Maps/L_Archaic_Temple"
DUST = "/Game/Olympus/Dust/BP/N_Dust"
MARBLES = ("/Game/Olympus/TilingMaterials/Marble/M/MI_CaynMarble",
           "/Game/Olympus/TilingMaterials/Marble/M/MI_GrayMarble")
SAVE_MARBLE = False

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def say(msg):
    print("%s %s" % (TAG, msg))


def clear_dust():
    dust = []
    for actor in eas.get_all_level_actors():
        for comp in actor.get_components_by_class(unreal.NiagaraComponent):
            system = comp.get_asset()
            if system and system.get_path_name().split(".")[0] == DUST:
                dust.append(actor)
                break
    if not dust:
        say("no N_Dust in the temple - nothing to remove.")
        return
    with unreal.ScopedEditorTransaction("Clear the temple's dust"):
        for actor in dust:
            say("removing %s at %s" % (actor.get_actor_label(), actor.get_actor_location()))
            eas.destroy_actor(actor)
    say("removed %d. Not saved: walk the temple, then Ctrl+S (Ctrl+Z puts them back)." % len(dust))


def clean_marble():
    white = unreal.LinearColor(1.0, 1.0, 1.0, 1.0)
    for path in MARBLES:
        mi = unreal.load_asset(path)
        if not mi:
            say("no %s - is the Olympus pack in the project?" % path)
            continue
        was_color = mel.get_material_instance_vector_parameter_value(mi, "ColorGrunge")
        was_rough = mel.get_material_instance_scalar_parameter_value(mi, "RoughnessGrunge")
        clean = (abs(was_color.r - 1.0) + abs(was_color.g - 1.0) + abs(was_color.b - 1.0)) < 1e-4 and abs(was_rough - 1.0) < 1e-4
        if not clean:
            mel.set_material_instance_vector_parameter_value(mi, "ColorGrunge", white)
            mel.set_material_instance_scalar_parameter_value(mi, "RoughnessGrunge", 1.0)
            mel.update_material_instance(mi)
            say("%s: grunge colour %s -> white, grunge roughness %.2f -> 1." % (mi.get_name(), was_color, was_rough))
        else:
            say("%s: already plain." % mi.get_name())
        if SAVE_MARBLE:
            say("%s saved: %s" % (mi.get_name(), unreal.EditorAssetLibrary.save_loaded_asset(mi, False)))
    if not SAVE_MARBLE:
        say("marble NOT saved (SAVE_MARBLE is False) - look in Play first.")


def main():
    if MAP not in world.get_path_name():
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (world.get_path_name(), MAP))
        return
    clear_dust()
    clean_marble()


main()
