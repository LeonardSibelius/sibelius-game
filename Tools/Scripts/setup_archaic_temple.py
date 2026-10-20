# setup_archaic_temple.py - the Archaic Language Temple, made ready to walk into.
#
# Walt, 2026-10-19: a hidden door "next to poker made visible with V and call it the Archaic
# Language Temple", into Hivemind's Olympus temple (bought, "Allows usage with AI: No": nothing
# here looks at it - the level is read as numbers only, and Walt judges how it looks). Walt saved
# Olympus as /Game/Maps/L_Archaic_Temple (gitignored, the agents' homes rule).
#
# Read from the level as text on 2026-10-19: no GameMode override (the project's runs), no
# PlayerStart, no camera that takes the player's view, no sequence that plays itself, and nothing
# in the map but the pack's own props - so, unlike Etherion's planets, nothing to take out. The
# entrance is at the origin, looking down the nave along +X (where the pack's demo camera stood):
# a lower hall at floor height ~40 from x 0 to ~2700, stairs, an upper floor at ~510, and the
# pack's two eleven-metre figures at x ~7570.
#
# This adds:
#   - PlayerStart_ArchaicTemple, tagged ArchaicTemple (the door's arrival tag), at the entrance
#     facing down the nave;
#   - the five AArchaicStatue, one per language, in the lower hall's side aisles (below), each
#     standing on the floor measured under it. A first arrangement - Walt moves them as he likes;
#   - T_Sign_ArchaicTemple into /Game/Signs, for the living-room door (place_temple_door.py).
# The way home is [O], as from every level away from the office.
#
# Does NOT save: Walt looks, arranges and saves (Ctrl+S). Safe to re-run: it replaces what it made.
#
# *** RUN WITH L_Archaic_Temple OPEN (File > Open Level), PIE stopped ***

import unreal

TAG = "###TEMPLE###"
MAP = "/Game/Maps/L_Archaic_Temple"
SIGN_SRC = "C:/Users/wpark/projects/sibelius-game/SourceArt/ArchaicTemple/T_Sign_ArchaicTemple.png"
SIGN = "/Game/Signs/T_Sign_ArchaicTemple"
START = ("PlayerStart_ArchaicTemple", "ArchaicTemple", 250.0, 0.0)
# THE SIDE AISLES, measured 2026-10-19 (numbers only): the nave's walkway is y -250..250 at floor
# 40; a rail at y +-300; raised aisles y +-340..600 at floor 100, columns at x ~930, 1480, 2390,
# 2950 on both sides; an outer rail at +-720. The statues stand in the aisles between columns,
# facing across the nave - 3 m from the walkway's edge, inside UInteractorComponent's 450 cm reach
# when he aims at the statue over the rail. In the order they were born, walking toward the
# pack's two giants: FORTRAN 1957 and COBOL 1959 face each other, then BASIC 1964 and Python 1991,
# and Java 1995, the youngest, last.
# label, language, x, y, yaw - the statues face +Y in their own space: yaw 180 turns that to -Y.
STATUES = [
    ("Statue_FORTRAN", "FORTRAN", 1205.0, 540.0, 180.0),
    ("Statue_COBOL", "COBOL", 1205.0, -540.0, 0.0),
    ("Statue_BASIC", "BASIC", 1938.0, 540.0, 180.0),
    ("Statue_PYTHON", "PYTHON", 1938.0, -540.0, 0.0),
    ("Statue_JAVA", "JAVA", 2670.0, 540.0, 180.0),
]

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary


def say(msg):
    print("%s %s" % (TAG, msg))


def floor_at(world, x, y, ignore):
    """The floor under (x, y): traced down from 4 m, under the chandeliers and over the floor."""
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 400.0), unreal.Vector(x, y, -600.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple()
    return (t[4].z, t[9].get_actor_label() if t[9] else "?") if t[0] else (None, None)


def import_sign():
    if eal.does_asset_exist(SIGN):
        say("sign %s already there" % SIGN)
        return
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SIGN_SRC)
    task.set_editor_property("destination_path", "/Game/Signs")
    task.set_editor_property("destination_name", "T_Sign_ArchaicTemple")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    say("sign %s %s" % (SIGN, "imported" if eal.does_asset_exist(SIGN) else "FAILED to import"))


def main():
    world = ues.get_editor_world()
    if les.is_in_play_in_editor():
        say("PIE is running - stop it first; nothing done")
        return
    if not world or not world.get_path_name().startswith(MAP + "."):
        say("open %s first (File > Open Level) - this is %s; nothing done" % (MAP, world.get_path_name() if world else None))
        return

    import_sign()

    mine = {s[0] for s in STATUES} | {START[0]}
    for a in eas.get_all_level_actors():
        if a.get_actor_label() in mine:
            eas.destroy_actor(a)

    label, tag, x, y = START
    z, on = floor_at(world, x, y, [])
    if z is None:
        say("no floor under the entrance at (%.0f, %.0f) - no start placed" % (x, y))
    else:
        start = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, z + 100.0), unreal.Rotator(0.0, 0.0, 0.0))
        start.set_actor_label(label)
        start.set_editor_property("player_start_tag", tag)
        say("%s (tag %s) at (%.0f, %.0f, %.0f) on %s, facing down the nave" % (label, tag, x, y, z + 100.0, on))

    # The aisles' gaps between columns hold the pack's small pines in pots: look through both (and
    # the big pines' branches) for the floor. Whether a pine or a statue stands there is Walt's call.
    pines = [a for a in eas.get_all_level_actors() if any(k in a.get_actor_label().lower() for k in ("pine", "pot"))]
    for label, lang, x, y, yaw in STATUES:
        z, on = floor_at(world, x, y, pines)
        if z is None:
            say("%s: no floor under (%.0f, %.0f) - not placed" % (label, x, y))
            continue
        statue = eas.spawn_actor_from_class(unreal.ArchaicStatue, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        statue.set_actor_label(label)
        statue.set_editor_property("language", getattr(unreal.ArchaicLanguage, lang))
        # What else stands where it does - a column, a pine, a light beam.
        o, e = statue.get_actor_bounds(False)
        crowd = []
        for a in eas.get_all_level_actors():
            if a == statue or a.get_class().get_name() in ("InstancedFoliageActor", "PostProcessVolume", "ExponentialHeightFog",
                                                            "SkyAtmosphere", "SkyLight", "DirectionalLight"):
                continue
            ao, ae = a.get_actor_bounds(False)
            if max(ae.x, ae.y) > 1500.0:
                continue     # floors, walls, the hall itself
            if abs(ao.x - o.x) < ae.x + e.x and abs(ao.y - o.y) < ae.y + e.y and abs(ao.z - o.z) < ae.z + e.z:
                crowd.append(a.get_actor_label())
        mesh = statue.get_editor_property("statue").get_editor_property("static_mesh")
        say("%s (%s, %s) at (%.0f, %.0f, %.0f) on %s%s" % (
            label, lang, mesh.get_name() if mesh else "NO MESH", x, y, z, on,
            ("; overlaps " + ", ".join(crowd[:6])) if crowd else ""))

    say("done - not saved. Look, move the statues where they belong, then Ctrl+S.")


main()
