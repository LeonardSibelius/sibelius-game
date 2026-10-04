# make_kaia_home.py — L_KaiaHome, Kaia's virtual home, behind the portal at the end of the hall.
#
# Walt, 2026-10-03: "we put a dancing Kaia in front of him when he enters the level, player
# presses "E" and gets the virtual world speech and then can chat". Her line, which he
# recorded: "Welcome to my virtual home, Leonard. I can Refactor it as I please."
#
# The world is Etherion Designs' "Alien Planet Fantasy Environment - Jungle Plants" (Fab,
# bought 2026-10-02; Content/AlienJunglePlants, gitignored): L_KaiaHome is its Level_Jungle
# SAVED AS a new map, with the pack's demo machinery taken out and Kaia put in. Its water,
# weather, wind, glowing grass trails, foliage physics, animal calls and music all stay. The
# level blueprint is re-parented off Etherion's demo class (see build()), and the plants'
# "fake lighting" it used to switch on is set by the VirtualHome actor instead.
#
# TAKEN OUT, by what they are rather than where they stand:
#   - the four notice boards (planes wearing MI_Notice_* / M_Note_* / MI_Note_* - "HAVING
#     PROBLEMS?", "NOTE: fake lighting", "INTERACTIVE FOLIAGE", distance fields);
#   - BP_PawnReplacer and BP_PawnStorage, which swap the player for the demo mannequin;
#   - BP_SpectatorPawn, BP_BugNotice, BP_QualitySettings (it changes scalability settings);
#   - BP_Ball and BP_Cube, the demo's physics toys;
#   - the PlayerStart, replaced by one tagged KaiaHome in the same place.
# And the World Settings' GameMode override (the demo's GM_Mannequin) is cleared, so the map
# runs the project's own game mode like every other level.
#
# NOT IN GIT: the map is the pack's landscape and foliage, so .gitignore lists it, as it does
# L_Bowling. Back it up to E:.
#
# RUN in two steps, PIE stopped. Never loads a level from here (that crashes the editor via
# the UnrealClaude plugin, 2026-09-28):
#   1. With Level_Jungle open:  py ".../Tools/Scripts/make_kaia_home.py"  - saves the copy.
#   2. Open L_KaiaHome (File > Open Level) and run it again - builds it and saves.
# Step 2 refuses to touch anything unless the open map is L_KaiaHome, and is safe to re-run.

import re
import unreal

TAG = "###KAIAHOME###"
SOURCE_MAP = "/Game/AlienJunglePlants/Levels/Level_Jungle"
HOME_MAP = "/Game/Maps/L_KaiaHome"
ARRIVAL_TAG = "KaiaHome"
KAIA_CLASS = "/Game/MetaHumans/MHC_Kaia/BP_Dancer_Kaia.BP_Dancer_Kaia_C"
KAIA_AHEAD = 450.0      # cm in front of the arrival point
KAIA_LABEL = "Kaia"
HOME_TAG = "HomeDancer"         # UDancerAgentComponent::HomeTag - E opens the chat on her welcome
HOME_CLASS = "/Script/SibeliusGame.VirtualHome"   # first person, Refactor lent (VirtualHome.h)
HOME_LABEL = "VirtualHome_Kaia"
APPEARANCE = "/Game/AlienJunglePlants/Materials/MPC_AlienJungleAppearance"   # the pack's look dials

FAKE_LIGHTING = 0.3     # the value Etherion's level blueprint set at Begin Play
# What makes her plants answer him - their demo mannequin carried three of these (AVirtualHome::
# FoliageInfluencer). Walt: "Greystone can't interact with the mushrooms in Kaia's world".
INFLUENCER = "/Game/AlienPhysicsWorld/FoliageSystem/BP_FoliageInfluencer.BP_FoliageInfluencer_C"
# F on her placed mushrooms: a boing (the pack's nine balloon sounds); on other plants, a rustle.
MUSHROOM_SOUND = "/Game/AlienPhysicsWorld/Sounds/Foliage/Balloon/SC_Balloon"
PLANT_SOUND = "/Game/AlienPhysicsWorld/Sounds/Foliage/Rustling/SC_Foliage_Rustle"

# ETHERION'S LEVEL BLUEPRINT MUST BE EMPTY. Its four nodes were: Begin Play -> set FakeLighting
# 0.3 -> SPAWN BP_EtherionLevelCommons -> start its demo menu. That spawned controller's Switch
# Pawn / Hide_Human_Player answer E: on Kaia's planet E made her vanish, then put Leonard in the
# lake out of Greystone's body (log, 2026-10-03). Re-parenting the level blueprint (build())
# does not remove nodes, and Python cannot reach graph nodes, so they were deleted through
# UnrealClaude's blueprint_modify delete_node. build() checks it stayed that way.
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


def build():
    world = ues.get_editor_world()
    actors = eas.get_all_level_actors()

    # The pack's start on the first build; ours (same place) on a re-run.
    starts = [a for a in actors if a.get_class().get_name() == "PlayerStart"]
    start = next((a for a in starts if a.get_actor_label() == "PlayerStart_KaiaHome"), None) \
        or next(iter(starts), None)
    if not start:
        say("no PlayerStart to arrive at - nothing changed.")
        return
    arrive_at = start.get_actor_location()
    arrive_rot = start.get_actor_rotation()

    removed = []
    for actor in actors:
        name = actor.get_class().get_name()
        if name in DEMO_CLASSES or is_notice(actor) or actor.get_actor_label() in (KAIA_LABEL, HOME_LABEL):
            removed.append("%s (%s)" % (actor.get_actor_label(), name))
            eas.destroy_actor(actor)
    say("removed %d: %s" % (len(removed), ", ".join(removed)))

    ws = world.get_world_settings()
    ws.set_editor_property("default_game_mode", None)
    say("GameMode override cleared - the project's default game mode runs here.")

    # THE LEVEL BLUEPRINT IS CUT LOOSE FROM ETHERION'S. BP_EtherionLevelCommons runs the demo's
    # pawn switching: when Kaia's talk close-up took the view (the demo reads a changed view
    # target as "switch to the spectator"), its Hide_Human_Player moved the player to where
    # BP_PawnStorage had stood - gone, so the map origin, the middle of the lake - and Switch
    # Pawn knocked him out of third person (Walt, 2026-10-03, and the log). The nodes that
    # spawned it are gone (see COMMONS); the parent goes too. Its one useful job, FakeLighting,
    # AVirtualHome now does (below).
    level_bp = unreal.load_object(None, HOME_MAP + ".L_KaiaHome:PersistentLevel.L_KaiaHome")
    if level_bp:
        unreal.BlueprintEditorLibrary.reparent_blueprint(level_bp, unreal.LevelScriptActor)
        unreal.BlueprintEditorLibrary.compile_blueprint(level_bp)
        say("level blueprint re-parented to LevelScriptActor.")

    ps = eas.spawn_actor_from_class(unreal.PlayerStart, arrive_at, arrive_rot)
    ps.set_actor_label("PlayerStart_KaiaHome")
    ps.set_editor_property("player_start_tag", ARRIVAL_TAG)

    kaia_class = unreal.load_class(None, KAIA_CLASS)
    if not kaia_class:
        say("could not load %s - no Kaia placed." % KAIA_CLASS)
    else:
        forward = arrive_rot.get_forward_vector()
        spot = arrive_at + unreal.Vector(forward.x, forward.y, 0.0) * KAIA_AHEAD
        feet = ground_below(world, spot)
        facing = unreal.Rotator(0.0, 0.0, arrive_rot.yaw + 180.0)
        kaia = eas.spawn_actor_from_class(kaia_class, feet, facing)
        kaia.set_actor_label(KAIA_LABEL)
        kaia.tags = [HOME_TAG]
        say("Kaia placed at %s, facing the arrival point, tagged %s." % (kaia.get_actor_location(), HOME_TAG))

    home_class = unreal.load_class(None, HOME_CLASS)
    if not home_class:
        say("no %s class - build the game module first; no VirtualHome placed." % HOME_CLASS)
    else:
        home = eas.spawn_actor_from_class(home_class, arrive_at, arrive_rot)
        home.set_actor_label(HOME_LABEL)
        home.set_editor_property("agent_name", "Kaia")
        mpc = unreal.load_asset(APPEARANCE)
        if mpc:
            home.set_editor_property("appearance", mpc)
            home.set_editor_property("appearance_scalars", {"FakeLighting": FAKE_LIGHTING})
        influencer = unreal.load_class(None, INFLUENCER)
        if influencer:
            home.set_editor_property("foliage_influencer", influencer)
        for prop, path in (("mushroom_sound", MUSHROOM_SOUND), ("plant_sound", PLANT_SOUND)):
            sound = unreal.load_asset(path)
            if sound:
                home.set_editor_property(prop, sound)
            else:
                say("no %s - that slap will be silent." % path)
        say("VirtualHome placed: %s person, Refactor lent, wild refactor up to %.0f cm, FakeLighting %s, foliage influencer %s." % (
            "third" if home.get_editor_property("third_person") else "first",
            home.get_editor_property("wild_refactor_max_size"),
            FAKE_LIGHTING if mpc else "NOT SET - no " + APPEARANCE,
            "on" if influencer else "NOT SET - no " + INFLUENCER))

    les.save_current_level()
    say("built %s: arrive at %s (tag %s). Saved." % (HOME_MAP, arrive_at, ARRIVAL_TAG))

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    deps = ar.get_dependencies(HOME_MAP, unreal.AssetRegistryDependencyOptions(True, True, True, True, True)) or []
    if any(str(d) == COMMONS for d in deps):
        say("WARNING: the map still uses BP_EtherionLevelCommons - its level blueprint has Etherion's nodes "
            "again; delete them (blueprint_query get_nodes / blueprint_modify delete_node) or E will "
            "switch pawns and drop him in the lake.")
    else:
        say("checked: nothing in the map uses BP_EtherionLevelCommons.")


def main():
    here = ues.get_editor_world().get_path_name()
    if SOURCE_MAP in here:
        ok = unreal.EditorLoadingAndSavingUtils.save_map(ues.get_editor_world(), HOME_MAP)
        say("saved a copy as %s: %s. Now open L_KaiaHome (File > Open Level) and run this again." % (HOME_MAP, ok))
        return
    if HOME_MAP not in here:
        say("REFUSING: the open map is %s, not %s or %s. Nothing changed." % (here, SOURCE_MAP, HOME_MAP))
        return
    build()


main()
