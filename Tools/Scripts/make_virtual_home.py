# make_virtual_home.py — build an agent's virtual home from one of the Etherion pack's levels.
#
# Kaia's came first (2026-10-03, as make_kaia_home.py): "we put a dancing Kaia in front of him
# when he enters the level, player presses "E" and gets the virtual world speech and then can
# chat". Elise's followed (2026-10-05, Walt: "Level_Grassland is open and I choose it for
# Elise"). One script for all of them, so a lesson learned in one home reaches every home.
#
# The worlds are Etherion Designs' "Alien Planet Fantasy Environment - Jungle Plants" (Fab,
# bought 2026-10-02; Content/AlienJunglePlants and Content/AlienPhysicsWorld, gitignored). A home
# is one of the pack's levels SAVED AS a new map, with the pack's demo machinery taken out and
# the agent put in. Its water, weather, wind, glowing grass trails, foliage physics, animal
# calls and music all stay.
#
# TAKEN OUT, by what they are rather than where they stand:
#   - the notice boards (planes wearing MI_Notice_* / M_Note_* / MI_Note_* - "HAVING
#     PROBLEMS?", "NOTE: fake lighting", "INTERACTIVE FOLIAGE", distance fields);
#   - BP_PawnReplacer and BP_PawnStorage, which swap the player for the demo mannequin;
#   - BP_SpectatorPawn, BP_BugNotice, BP_QualitySettings (it changes scalability settings);
#   - BP_Ball and BP_Cube, the demo's physics toys;
#   - the PlayerStart, replaced by one tagged for the home in the same place.
# And the World Settings' GameMode override (the demo's GM_Mannequin) is cleared, so the map
# runs the project's own game mode like every other level.
#
# ETHERION'S LEVEL BLUEPRINT MUST BE EMPTY, and this script cannot empty it (Python cannot reach
# graph nodes). In Level_Jungle its nodes set FakeLighting 0.3, then SPAWNED BP_EtherionLevelCommons,
# whose Switch Pawn / Hide_Human_Player answered E - on Kaia's planet E made her vanish, then put
# Leonard in the lake (2026-10-03). Level_Grassland's spawn the same controller AND bind the V key
# to their music menu - V is Code Vision. They are deleted through UnrealClaude's blueprint_query
# get_nodes / blueprint_modify delete_node; build() re-parents the blueprint and checks the map no
# longer uses the controller.
#
# NOT IN GIT: every home is the pack's landscape and foliage, so .gitignore lists each map, as it
# does L_Bowling. Back them up to E:.
#
# RUN in two steps, PIE stopped. Never loads a level from here (that crashes the editor via
# the UnrealClaude plugin, 2026-09-28):
#   1. With the SOURCE level open:  py ".../Tools/Scripts/make_virtual_home.py"  - saves the copy.
#   2. Open the HOME map (File > Open Level) and run it again - builds it and saves.
# It picks the home by the open map, refuses anything else, and step 2 is safe to re-run.

import re
import unreal

TAG = "###VIRTUALHOME###"

DANCES = "/Game/Characters/Retargeting/"   # UDancerAgentComponent's ten
HOMES = [
    {   # Kaia's: the jungle. Walt recorded nothing yet; her welcome is the chat's first line.
        "agent": "Kaia",
        "source": "/Game/AlienJunglePlants/Levels/Level_Jungle",
        "home": "/Game/Maps/L_KaiaHome",
        "arrival": "KaiaHome",
        "agent_class": "/Game/MetaHumans/MHC_Kaia/BP_Dancer_Kaia.BP_Dancer_Kaia_C",   # dances itself
        "dance": None,
        "welcome": "Welcome to my virtual home, Leonard.  I can Refactor it as I please.",
        "scalars": {"FakeLighting": 0.3},   # what Etherion's level blueprint set at Begin Play
    },
    {   # Elise's: the grassland.
        "agent": "Elise",
        "source": "/Game/AlienJunglePlants/Levels/Level_Grassland",
        "home": "/Game/Maps/L_EliseHome",
        "arrival": "EliseHome",
        "agent_class": "/Game/MetaHumans/MHC_Elise/BP_MHC_Elise.BP_MHC_Elise_C",
        # A plain MetaHuman: a dancer is any actor whose body plays one of the ten dances
        # (UDancerAgentSubsystem::ScanForDancers), so her body is set to one here.
        "dance": DANCES + "Anim_Slow_Rhythm_Dance_12_MH.Anim_Slow_Rhythm_Dance_12_MH",
        "welcome": "Welcome to my virtual home, Leonard.",
        "scalars": {},   # Level_Grassland's blueprint set no look dials
    },
]

AGENT_AHEAD = 450.0             # cm in front of the arrival point
HOME_TAG = "HomeDancer"         # UDancerAgentComponent::HomeTag - E opens the chat on her welcome
HOME_CLASS = "/Script/SibeliusGame.VirtualHome"   # first person, Refactor lent (VirtualHome.h)
APPEARANCE = "/Game/AlienJunglePlants/Materials/MPC_AlienJungleAppearance"   # the pack's look dials
# What makes the plants answer him - their demo mannequin carried three of these (AVirtualHome::
# FoliageInfluencer). Walt: "Greystone can't interact with the mushrooms in Kaia's world".
INFLUENCER = "/Game/AlienPhysicsWorld/FoliageSystem/BP_FoliageInfluencer.BP_FoliageInfluencer_C"
# F on placed mushrooms: a boing (the pack's nine balloon sounds); on other plants, a rustle.
MUSHROOM_SOUND = "/Game/AlienPhysicsWorld/Sounds/Foliage/Balloon/SC_Balloon"
PLANT_SOUND = "/Game/AlienPhysicsWorld/Sounds/Foliage/Rustling/SC_Foliage_Rustle"

COMMONS = "/Game/AlienPhysicsWorld/DemoAssets/Blueprints/BP_EtherionLevelCommons"
DEMO_CLASSES = ("BP_PawnReplacer_C", "BP_PawnStorage_C", "BP_SpectatorPawn_C", "BP_BugNotice_C",
                "BP_QualitySettings_C", "BP_Ball_C", "BP_Cube_C", "PlayerStart")
NOTICE_MATERIAL = re.compile(r"^(MI_Notice_|M_Note_|MI_Note_)")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def is_notice(actor):
    if actor.get_class().get_name() != "StaticMeshActor":
        return False
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        for mat in comp.get_materials():
            if mat and NOTICE_MATERIAL.match(mat.get_name()):
                return True
    return False


def ground_below(world, where):
    start = unreal.Vector(where.x, where.y, where.z + 500.0)
    end = unreal.Vector(where.x, where.y, where.z - 5000.0)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                 False, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        return hit.to_tuple()[4]   # impact point
    return where


def set_dancing(actor, dance_path):
    """Her body plays the dance, as the dancers in the house do: single-node, looping."""
    anim = unreal.load_asset(dance_path)
    body = next((c for c in actor.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "Body"), None)
    if not anim or not body:
        return "NOT dancing - no %s" % ("dance " + dance_path if not anim else "Body component")
    body.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    data = unreal.SingleAnimationPlayData()
    data.set_editor_property("anim_to_play", anim)
    data.set_editor_property("saved_looping", True)
    data.set_editor_property("saved_playing", True)
    body.set_editor_property("animation_data", data)
    return "dancing %s" % anim.get_name()


def build(spec):
    world = ues.get_editor_world()
    actors = eas.get_all_level_actors()
    agent = spec["agent"]
    home_map = spec["home"]
    map_name = home_map.split("/")[-1]
    start_label = "PlayerStart_" + spec["arrival"]
    home_label = "VirtualHome_" + agent

    # The pack's start on the first build; ours (same place) on a re-run.
    starts = [a for a in actors if a.get_class().get_name() == "PlayerStart"]
    start = next((a for a in starts if a.get_actor_label() == start_label), None) or next(iter(starts), None)
    if not start:
        say("no PlayerStart to arrive at - nothing changed.")
        return
    arrive_at = start.get_actor_location()
    arrive_rot = start.get_actor_rotation()

    removed = []
    for actor in actors:
        name = actor.get_class().get_name()
        if name in DEMO_CLASSES or is_notice(actor) or actor.get_actor_label() in (agent, home_label):
            removed.append("%s (%s)" % (actor.get_actor_label(), name))
            eas.destroy_actor(actor)
    say("removed %d: %s" % (len(removed), ", ".join(removed)))

    world.get_world_settings().set_editor_property("default_game_mode", None)
    say("GameMode override cleared - the project's default game mode runs here.")

    # The level blueprint is cut loose from Etherion's demo class; its nodes must already be
    # gone (see the header) - checked after the save.
    level_bp = unreal.load_object(None, "%s.%s:PersistentLevel.%s" % (home_map, map_name, map_name))
    if level_bp:
        unreal.BlueprintEditorLibrary.reparent_blueprint(level_bp, unreal.LevelScriptActor)
        unreal.BlueprintEditorLibrary.compile_blueprint(level_bp)
        say("level blueprint re-parented to LevelScriptActor.")

    ps = eas.spawn_actor_from_class(unreal.PlayerStart, arrive_at, arrive_rot)
    ps.set_actor_label(start_label)
    ps.set_editor_property("player_start_tag", spec["arrival"])

    agent_class = unreal.load_class(None, spec["agent_class"])
    if not agent_class:
        say("could not load %s - no %s placed." % (spec["agent_class"], agent))
    else:
        forward = arrive_rot.get_forward_vector()
        spot = arrive_at + unreal.Vector(forward.x, forward.y, 0.0) * AGENT_AHEAD
        feet = ground_below(world, spot)
        facing = unreal.Rotator(0.0, 0.0, arrive_rot.yaw + 180.0)
        dancer = eas.spawn_actor_from_class(agent_class, feet, facing)
        dancer.set_actor_label(agent)
        dancer.tags = [HOME_TAG]
        dancing = set_dancing(dancer, spec["dance"]) if spec["dance"] else "dancing (her own blueprint)"
        say("%s placed at %s, facing the arrival point, tagged %s, %s." % (agent, dancer.get_actor_location(), HOME_TAG, dancing))

    home_class = unreal.load_class(None, HOME_CLASS)
    if not home_class:
        say("no %s class - build the game module first; no VirtualHome placed." % HOME_CLASS)
    else:
        home = eas.spawn_actor_from_class(home_class, arrive_at, arrive_rot)
        home.set_actor_label(home_label)
        home.set_editor_property("agent_name", agent)
        try:
            home.set_editor_property("welcome_line", spec["welcome"])
        except Exception:
            say("this build's VirtualHome has no WelcomeLine yet - rebuild the game module, then re-run.")
        mpc = unreal.load_asset(APPEARANCE)
        if mpc and spec["scalars"]:
            home.set_editor_property("appearance", mpc)
            home.set_editor_property("appearance_scalars", spec["scalars"])
        influencer = unreal.load_class(None, INFLUENCER)
        if influencer:
            home.set_editor_property("foliage_influencer", influencer)
        for prop, path in (("mushroom_sound", MUSHROOM_SOUND), ("plant_sound", PLANT_SOUND)):
            sound = unreal.load_asset(path)
            if sound:
                home.set_editor_property(prop, sound)
            else:
                say("no %s - that slap will be silent." % path)
        say("VirtualHome placed: %s person, Refactor lent, look dials %s, foliage influencer %s." % (
            "third" if home.get_editor_property("third_person") else "first",
            spec["scalars"] or "untouched",
            "on" if influencer else "NOT SET - no " + INFLUENCER))

    les.save_current_level()
    say("built %s for %s: arrive at %s (tag %s). Saved." % (home_map, agent, arrive_at, spec["arrival"]))

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    deps = ar.get_dependencies(home_map, unreal.AssetRegistryDependencyOptions(True, True, True, True, True)) or []
    if any(str(d) == COMMONS for d in deps):
        say("WARNING: the map still uses BP_EtherionLevelCommons - its level blueprint has Etherion's nodes; "
            "delete them (blueprint_query get_nodes / blueprint_modify delete_node) or E will switch pawns "
            "and drop him in the water.")
    else:
        say("checked: nothing in the map uses BP_EtherionLevelCommons.")


def main():
    here = ues.get_editor_world().get_path_name()
    for spec in HOMES:
        if spec["source"] in here:
            ok = unreal.EditorLoadingAndSavingUtils.save_map(ues.get_editor_world(), spec["home"])
            say("saved a copy as %s for %s: %s. Now open %s (File > Open Level) and run this again." % (
                spec["home"], spec["agent"], ok, spec["home"].split("/")[-1]))
            return
        if spec["home"] in here:
            build(spec)
            return
    say("REFUSING: the open map is %s - not a home or a home's source level. Nothing changed." % here)


main()
