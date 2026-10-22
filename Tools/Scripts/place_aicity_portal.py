# place_aicity_portal.py - the way to the AI city: a CompilePortal at the end of the upstairs hall,
# where the seed (AUniverseSeed) stood. Walt, 2026-10-22: "now about this UniverseSeed thing. Please
# replace it with the same portal that is used to enter the Agent's home worlds, operated the same
# way with a 'C'". So it copies every look setting from ElisePortal (NS_TeleporterHole stood up,
# "[C] go through") and goes to L_Genesis; C's own gate is Compile, as for the agents' homes - the
# seed's six-power gate went with it. Removes any UniverseSeed still in the level.
#
# Placed as Elise's portal stands, its centre 110 cm over the floor (the hall's floor is z 320),
# facing down the hall - but 35 cm in front of the hall's east end wall (x -1677), not a metre:
# Walt, the same day, "can the portal next to Isla be moved back towards the wall a bit? It is hard
# to see the translucent attic stairs behind all that purple swirling".
#
# *** OPEN /Game/L_Office_v02 FIRST (File > Open Level), PIE stopped, THEN RUN ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_aicity_portal.py"
#
# Refuses unless L_Office_v02 is the open level. One undoable step (Ctrl+Z). Does not save: Ctrl+S.

import unreal

TAG = "###AICITYPORTAL###"
AT = unreal.Vector(-1712.0, 10040.0, 430.0)   # and 30 cm right of the alcove's old line, away from the attic stairs
YAW = 90.0                          # the stood-up disc faces the actor's +-Y: here, along the hall
LOOK = ("prompt_text", "not_ready_text", "claim_radius", "portal_system", "effect_offset",
        "effect_rotation", "effect_scale", "glow_lumens", "glow_color", "prompt_extent")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def say(text):
    print("%s %s" % (TAG, text))


def main():
    if "L_Office_v02" not in world.get_path_name():
        say("REFUSING: the open level is %s. Nothing changed." % world.get_path_name())
        return
    actors = eas.get_all_level_actors()
    elise = [a for a in actors if a.get_actor_label() == "ElisePortal"]
    if not elise:
        say("REFUSING: no ElisePortal to copy the look from.")
        return
    seeds = [a for a in actors if a.get_class().get_name() == "UniverseSeed"]
    old = [a for a in actors if a.get_actor_label() == "AICityPortal"]
    cls = unreal.load_class(None, "/Script/SibeliusGame.CompilePortal")
    with unreal.ScopedEditorTransaction("The seed becomes a portal to the AI city"):
        for o in old + seeds:
            eas.destroy_actor(o)
        portal = eas.spawn_actor_from_class(cls, AT, unreal.Rotator(roll=0.0, pitch=0.0, yaw=YAW))
        for prop in LOOK:
            portal.set_editor_property(prop, elise[0].get_editor_property(prop))
        portal.set_editor_property("travel_target_level", "L_Genesis")
        portal.set_editor_property("arrival_tag", "GenesisGallery")
        portal.set_actor_label("AICityPortal")
    say("AICityPortal at %s, yaw %.0f, to L_Genesis; %d seed(s) and %d old portal(s) removed. NOT saved - Ctrl+S."
        % (AT, YAW, len(seeds), len(old)))


main()
