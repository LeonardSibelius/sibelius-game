# make_virtual_home.py — build an agent's virtual home from a pack's demo level (Etherion's
# planets for Kaia, Elise and Aisling; Meshingun's Fafnir's Keep for Isla, who had a sci-fi kit's
# space station until 2026-10-24).
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
    {   # Aisling's: the pack's physics world (2026-10-05). Walt: "very empty and bleak with snow
        # covered mountains in the distance and lots of snow on the empty ground" - "I think I
        # will assign it to Aisling".
        "agent": "Aisling",
        "source": "/Game/AlienPhysicsWorld/Levels/Level_AlienPhysicsWorld",
        "home": "/Game/Maps/L_AislingHome",
        "arrival": "AislingHome",
        "agent_class": "/Game/MetaHumans/MHC_Aisling/BP_MHC_Aisling.BP_MHC_Aisling_C",
        "dance": DANCES + "Anim_Mid_Rhythm_Dance_10_MH.Anim_Mid_Rhythm_Dance_10_MH",
        "welcome": "Welcome to my virtual home, Leonard.",
        "scalars": {},   # its blueprint set no look dials either
    },
    {   # Isla's first: a space station, the first home indoors and the first not Etherion's (2026-10-05).
        # Walt: "ModularSciFiEnv_1_Demo is open - it is good enough"; her portal is in the attic.
        # Content/ModularSciFiEnv_1, gitignored like the other packs. RETIRED 2026-10-24 for the keep
        # below (Walt: "a terrible space station"); L_IslaHome stays on disk for the builds that have it.
        "agent": "Isla",
        "source": "/Game/ModularSciFiEnv_1/Maps/ModularSciFiEnv_1_Demo",
        "home": "/Game/Maps/L_IslaHome",
        "arrival": "IslaHome",
        "agent_class": "/Game/MetaHumans/MHC_IslaRowan/BP_MHC_IslaRowan.BP_MHC_IslaRowan_C",
        "dance": DANCES + "Anim_High_Rhythm_Dance_13_MH.Anim_High_Rhythm_Dance_13_MH",
        "welcome": "Welcome to my virtual home, Leonard.  Nothing stays here unless it has earned it.",
        "scalars": {},
        "plants": False,   # no foliage: no influencer, no boing
        "indoors": True,   # a ceiling 4 m up
        # The kit's walls, floors and ceilings come in pieces as small as its furniture (1.5-3.5 m
        # against 0.8-3.3 m), so the planet-sized limit would turn the station's walls to gold and
        # open them on the void. The house's 3.5 m plus Refactor's architecture words keep it to the
        # furniture; treasure the size of what it replaces, so a chest never blocks a corridor.
        "refactor_max": 350.0,
        "min_treasure": 0.0,
        # She dances under one of the kit's ceiling downlights. Walt: "is it possible to reduce
        # the light shining on Isla in Isla's home? She looks too bright".
        "dim_overhead": 0.35,
    },
    {   # Isla's now: Meshingun Studio's "Fafnir's Keep" (Fab; Content/Fafnir_Keep_Meshingun, gitignored),
        # 2026-10-24. Walt wanted somewhere "more attractive yet exotic" than the station: a gilded hall
        # with emerald fires, two dragons at the foot of a grand staircase, a magic mirror at its top.
        # A NEW MAP, not the station's: builds that have L_IslaHome keep it, and the chat server tells
        # the two apart by name. The first home that is World Partition - saving it writes its actors.
        "agent": "Isla",
        "source": "/Game/Fafnir_Keep_Meshingun/Levels/LV_Main_Night_Light",
        "home": "/Game/Maps/L_IslaKeep",
        "arrival": "IslaKeep",
        "agent_class": "/Game/MetaHumans/MHC_IslaRowan/BP_MHC_IslaRowan.BP_MHC_IslaRowan_C",
        "dance": DANCES + "Anim_High_Rhythm_Dance_13_MH.Anim_High_Rhythm_Dance_13_MH",
        "welcome": "Welcome to my virtual home, Leonard.  Nothing stays here unless it has earned it.",
        "scalars": {},
        "plants": False,
        "indoors": True,
        # The pack's own start is OUTSIDE the entrance, behind a door leaf whose collision closes the
        # doorway, facing a dark corridor. He arrives in the hall instead, on its axis 7 m in, facing
        # the staircase - and she dances 4.5 m on, at its foot, between the two dragons.
        "arrive": (15.0, -700.0, 90.0),   # x, y, yaw - the floor is found below
        # The seller's showroom machinery: the "enable Niagara Fluids" notes, the shots' cameras,
        # their rail and the sequences that drive them.
        "remove_classes": ("BP_Info_Slate_C", "CineCameraActor", "CameraRig_Rail", "LevelSequenceActor"),
        # Furniture only, as in the station. The hall is built partly of engine shapes and of pieces
        # whose names Refactor's architecture words miss (arches, platforms, ornaments, the mirror's
        # portal, the glow volumes): those are tagged NoWildRefactor, so R looks past them.
        "refactor_max": 350.0,
        "min_treasure": 0.0,
        "no_refactor": ("Cube", "Plane", "Cylinder", "Sphere", "SM_Portal_mesh_01", "SM_Main_ornament_piece_01",
                        "SM_Carpet_bar", "SM_Carpet", "SM_VFX_Phantoms", "SM_Single_stripe_01", "SM_Ornament_01",
                        "SM_Magic_mirror", "SM_Magic_Mirror_platform_01", "SM_Platform_01", "SM_Platform_02",
                        "SM_Platform_Corner", "SM_short_arch", "SM_Short_Arch_lower_Level_01", "SM_arch_entrance",
                        "SM_long_arch_01", "SM_medium_arch_01", "SM_medium_arch_01_destroyed", "SM_chain",
                        "SM_ornament_structure_platform"),
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


def ground_below(world, where, up=500.0):
    """Up 5 m first for a hillside; indoors only 1 m, or the ceiling is the ground."""
    start = unreal.Vector(where.x, where.y, where.z + up)
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


DIMMED_TAG = "DimmedOverAgent"   # a light already turned down, so a re-run never dims it twice


def dim_overhead(actors, feet, factor, agent):
    """Turn down the lamps above her head (within 2.5 m sideways), not the room's others."""
    dimmed = []
    for a in actors:
        if a.get_class().get_name() not in ("SpotLight", "PointLight", "RectLight"):
            continue
        p = a.get_actor_location()
        if p.z < feet.z + 150.0 or ((p.x - feet.x) ** 2 + (p.y - feet.y) ** 2) ** 0.5 > 250.0:
            continue
        light = a.get_component_by_class(unreal.LightComponent)
        if DIMMED_TAG in [str(t) for t in a.tags]:
            dimmed.append("%s (already %.1f)" % (a.get_actor_label(), light.get_editor_property("intensity")))
            continue
        was = light.get_editor_property("intensity")
        light.set_editor_property("intensity", was * factor)
        a.tags = list(a.tags) + [DIMMED_TAG]
        dimmed.append("%s %.1f -> %.1f (%s)" % (a.get_actor_label(), was, was * factor,
                                                light.get_editor_property("mobility")))
    say("over %s: %s." % (agent, ", ".join(dimmed) if dimmed else "no lamp within 2.5 m - nothing dimmed"))


def build(spec):
    world = ues.get_editor_world()
    actors = eas.get_all_level_actors()
    agent = spec["agent"]
    home_map = spec["home"]
    map_name = home_map.split("/")[-1]
    start_label = "PlayerStart_" + spec["arrival"]
    home_label = "VirtualHome_" + agent

    # The pack's start on the first build; ours (same place) on a re-run. Or a spot of our own,
    # where the pack's start is no place to arrive (the keep's stands outside a closed door).
    if spec.get("arrive"):
        x, y, yaw = spec["arrive"]
        arrive_at = unreal.Vector(x, y, 0.0)
        arrive_rot = unreal.Rotator(0.0, 0.0, yaw)
    else:
        starts = [a for a in actors if a.get_class().get_name() == "PlayerStart"]
        start = next((a for a in starts if a.get_actor_label() == start_label), None) or next(iter(starts), None)
        if not start:
            say("no PlayerStart to arrive at - nothing changed.")
            return
        arrive_at = start.get_actor_location()
        arrive_rot = start.get_actor_rotation()
    if spec.get("indoors"):
        # The station's start stood 42 cm above its floor, his capsule half in it: stand it on the
        # floor at a capsule's half height, as a PlayerStart's own capsule is.
        floor = ground_below(world, arrive_at, 100.0)
        if floor is not arrive_at:
            arrive_at = unreal.Vector(arrive_at.x, arrive_at.y, floor.z + 92.0)

    removed = []
    remove_classes = DEMO_CLASSES + tuple(spec.get("remove_classes", ()))
    for actor in actors:
        name = actor.get_class().get_name()
        if name in remove_classes or is_notice(actor) or actor.get_actor_label() in (agent, home_label):
            removed.append("%s (%s)" % (actor.get_actor_label(), name))
            eas.destroy_actor(actor)
    say("removed %d: %s" % (len(removed), ", ".join(removed)))

    if spec.get("no_refactor"):
        tagged = 0
        for actor in eas.get_all_level_actors():
            if "NoWildRefactor" in [str(t) for t in actor.tags]:
                continue
            meshes = [c.get_editor_property("static_mesh") for c in actor.get_components_by_class(unreal.StaticMeshComponent)]
            if any(m and m.get_name() in spec["no_refactor"] for m in meshes):
                actor.modify()
                actor.set_editor_property("tags", list(actor.tags) + ["NoWildRefactor"])
                tagged += 1
        say("tagged %d pieces of the building NoWildRefactor (%s)." % (tagged, ", ".join(spec["no_refactor"])))

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
        forward = unreal.Vector(forward.x, forward.y, 0.0)
        # Indoors a wall may come first: stand her a metre short of it.
        ahead = AGENT_AHEAD
        chest = arrive_at + unreal.Vector(0.0, 0.0, 50.0)
        wall = unreal.SystemLibrary.line_trace_single(world, chest, chest + forward * (AGENT_AHEAD + 100.0),
                                                      unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                      unreal.DrawDebugTrace.NONE, True)
        if wall:
            ahead = max(150.0, (wall.to_tuple()[4] - chest).length() - 100.0)
            say("a wall %.0f cm ahead - %s stands %.0f cm from the arrival point." % (
                (wall.to_tuple()[4] - chest).length(), agent, ahead))
        spot = arrive_at + forward * ahead
        feet = ground_below(world, spot, 100.0 if spec.get("indoors") else 500.0)
        facing = unreal.Rotator(0.0, 0.0, arrive_rot.yaw + 180.0)
        dancer = eas.spawn_actor_from_class(agent_class, feet, facing)
        dancer.set_actor_label(agent)
        dancer.tags = [HOME_TAG]
        dancing = set_dancing(dancer, spec["dance"]) if spec["dance"] else "dancing (her own blueprint)"
        say("%s placed at %s, facing the arrival point, tagged %s, %s." % (agent, dancer.get_actor_location(), HOME_TAG, dancing))
        if spec.get("dim_overhead"):
            dim_overhead(actors, feet, spec["dim_overhead"], agent)

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
        if spec["scalars"]:
            mpc = unreal.load_asset(APPEARANCE)
            if mpc:
                home.set_editor_property("appearance", mpc)
                home.set_editor_property("appearance_scalars", spec["scalars"])
        for prop, key in (("wild_refactor_max_size", "refactor_max"), ("min_treasure_size", "min_treasure")):
            if key in spec:
                home.set_editor_property(prop, spec[key])
        plants = spec.get("plants", True)
        influencer = unreal.load_class(None, INFLUENCER) if plants else None
        if influencer:
            home.set_editor_property("foliage_influencer", influencer)
        for prop, path in ((("mushroom_sound", MUSHROOM_SOUND), ("plant_sound", PLANT_SOUND)) if plants else ()):
            sound = unreal.load_asset(path)
            if sound:
                home.set_editor_property(prop, sound)
            else:
                say("no %s - that slap will be silent." % path)
        say("VirtualHome placed: %s person, Refactor lent up to %.0f cm, treasure at least %.0f cm, look dials %s, foliage influencer %s." % (
            "third" if home.get_editor_property("third_person") else "first",
            home.get_editor_property("wild_refactor_max_size"), home.get_editor_property("min_treasure_size"),
            spec["scalars"] or "untouched",
            ("on" if influencer else "NOT SET - no " + INFLUENCER) if plants else "none (no plants here)"))

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
