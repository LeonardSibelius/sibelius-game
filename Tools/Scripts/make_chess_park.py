# make_chess_park.py — L_Chess, the Transhuman Chess Park: a quiet copy of Transhuman City
# with a chess table on its plaza.
#
# Walt, 2026-09-28, choosing where the living room's CHESS door leads: "a chess park of its
# own". The city itself is the best corner of the city there is - its sky, its sun, its
# buildings, its plaza - so L_Chess is L_City SAVED AS a new map, with everything that
# belongs to the story taken out: the dancers and Nyra, the doors to the deli and uFoods,
# the spaceport site, the hint volumes, the power grants, the player starts. What is left is
# scenery, and a chess table on one of the plaza's own cafe tables.
#
# Why not simply send the door to L_City: Nyra's guide stages read his progress, and
# arriving in the city early would skip the spaceport and uFoods story.
#
# RUN with L_City open, PIE stopped:
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_chess_park.py"
# DRY_RUN = True (the default) only lists what it would remove and where the table would
# go. Set it False to build. Building NEVER edits L_City: it saves the copy, reopens it,
# and refuses to touch anything unless the open map is L_Chess.

import re
import unreal

TAG = "###CHESSPARK###"
DRY_RUN = True
SOURCE_MAP = "/Game/Maps/L_City"
PARK_MAP = "/Game/Maps/L_Chess"
TABLE_MESH = "SM_prop_dining_table_a"
ARRIVAL_TAG = "ChessPark"

# Story actors, by class name. StaticMeshActors are never matched: a mesh called
# "..._door_..." is a building's door, not a travel door.
STORY_CLASSES = re.compile(r"Door|BuildSite|Spaceport|Hint|Grant|Shrine|Travel|Guide|Nyra|Dancer|Agent|Portal|"
                           r"Altar|Placard|Refuser|Pickup|Shop|Purchase|PlayerStart|Trigger|Volume_Story|Chess",
                           re.IGNORECASE)
# Any actor carrying one of these components is a character of the story, whatever its class.
STORY_COMPONENTS = ("DancerAgentComponent", "NpcChatComponent", "InteractorComponent")
KEEP_CLASSES = ("StaticMeshActor", "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog",
                "VolumetricCloud", "PostProcessVolume", "WorldSettings", "Brush", "LightmassImportanceVolume")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def say(text):
    print("%s %s" % (TAG, text))


def world_path():
    return ues.get_editor_world().get_path_name()


def story_actors():
    found = []
    for actor in eas.get_all_level_actors():
        cls = actor.get_class().get_name()
        if cls in KEEP_CLASSES:
            continue
        comps = [c.get_class().get_name() for c in actor.get_components_by_class(unreal.ActorComponent)]
        if STORY_CLASSES.search(cls) or any(c in STORY_COMPONENTS for c in comps):
            found.append(actor)
    return found


def pick_table():
    """One of the plaza's cafe tables: the one with the most open ground around it, so a
    seated player and the camera are not inside a neighbour's chair."""
    tables = [a for a in eas.get_all_level_actors()
              if a.get_class().get_name() == "StaticMeshActor"
              and a.static_mesh_component.static_mesh
              and a.static_mesh_component.static_mesh.get_name() == TABLE_MESH]
    if not tables:
        return None, []
    others = [a for a in eas.get_all_level_actors() if a.get_class().get_name() == "StaticMeshActor"]

    def crowding(table):
        here = table.get_actor_location()
        n = 0
        for o in others:
            if o == table:
                continue
            d = o.get_actor_location() - here
            if abs(d.z) < 200 and (d.x * d.x + d.y * d.y) < 180.0 * 180.0:
                n += 1
        return n

    ranked = sorted(tables, key=crowding)
    return ranked[0], [(t.get_actor_label(), crowding(t), t.get_actor_location()) for t in ranked[:6]]


def main():
    here = world_path()
    if DRY_RUN:
        if SOURCE_MAP not in here:
            say("open L_City first (open: %s)" % here)
            return
        story = story_actors()
        say("DRY RUN in %s: %d story actors would go:" % (here, len(story)))
        for a in story:
            loc = a.get_actor_location()
            say("  %-30s %-32s (%.0f, %.0f, %.0f)" % (a.get_class().get_name(), a.get_actor_label(), loc.x, loc.y, loc.z))
        table, ranked = pick_table()
        say("cafe tables, least crowded first: %s" % ranked)
        if table:
            say("the chess table would go on %s at %s" % (table.get_actor_label(), table.get_actor_location()))
        return

    # BUILD, in two runs. NEVER load a level from here: the UnrealClaude plugin counts the
    # level's actors after every script, and a level swapped out mid-script crashed the
    # editor that way (2026-09-28). So run 1 saves the copy and stops; Walt opens L_Chess
    # (File > Open Level); run 2, finding L_Chess open, clears it and sets the table.
    if SOURCE_MAP in here:
        ok = unreal.EditorLoadingAndSavingUtils.save_map(ues.get_editor_world(), PARK_MAP)
        say("saved a copy as %s: %s. Now open L_Chess (File > Open Level) and run this again." % (PARK_MAP, ok))
        return
    if PARK_MAP not in here:
        say("REFUSING: the open map is %s, not %s. Nothing changed." % (here, PARK_MAP))
        return

    for a in story_actors():
        eas.destroy_actor(a)

    table, _ = pick_table()
    if not table:
        say("no %s found - the table needs placing by hand." % TABLE_MESH)
    else:
        origin, ext = table.get_actor_bounds(False)
        top = unreal.Vector(origin.x, origin.y, origin.z + ext.z)
        chess_class = unreal.load_class(None, "/Script/SibeliusGame.ChessTable")
        if chess_class:
            chess = eas.spawn_actor_from_class(chess_class, top, unreal.Rotator(0.0, 0.0, 0.0))
            chess.set_actor_label("ChessTable")
            say("ChessTable on %s, top at %s" % (table.get_actor_label(), top))
        else:
            say("ChessTable class not built yet - rebuild, then place it at %s" % top)
        # He arrives standing at White's side, a step back, facing the board (+X is toward Black).
        start = unreal.Vector(top.x - 160.0, top.y, top.z + 20.0)
        ps = eas.spawn_actor_from_class(unreal.PlayerStart, start, unreal.Rotator(0.0, 0.0, 0.0))
        ps.set_editor_property("player_start_tag", ARRIVAL_TAG)
        ps.set_actor_label("PlayerStart_ChessPark")

    les.save_current_level()
    say("built %s and saved." % PARK_MAP)


main()
