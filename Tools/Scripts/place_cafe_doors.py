# place_cafe_doors.py - the way into Jacob's Downtown Deli, and the way out.
#
# *** RUN TWICE, ONCE PER LEVEL, FROM THE OPEN EDITOR ***
#   open L_City, run it  -> an invisible doorway trigger at the deli's front door
#   open L_Cafe, run it  -> the return door back to the street
#
# It acts on whichever level is loaded and never changes levels itself - Python that
# calls load_level crashes this editor. See make_city_from_downtown.py.
#
# ===========================================================================
# WHY THE DOOR IS INVISIBLE.
#
# Downtown West's deli ALREADY HAS A DOOR - build_d_mod_lvl1_window_door4, part of the
# building, with glass and a wooden frame and a sign over it reading "Jacob's Downtown
# Deli, surprisingly adequate". Bolting a cathedral door onto that shopfront would look
# exactly as stupid as it sounds.
#
# So this places an ACathedralDoor whose mesh is HIDDEN but still answers a trace. The
# pack's own door is what the player sees; this is only what the interactor finds.
#
# That works because ApplyRevealed - the thing that drives mesh visibility - is called
# ONLY on gated doors (bRequireGenerateUse / bRequireBattleToll). This one is ungated, so
# nothing at BeginPlay ever touches the mesh and hidden_in_game survives into the game.
# Checked in CathedralDoor.cpp before relying on it.
#
# COLLISION IS QUERY-ONLY, not blocking. The focus trace has to hit it or E finds
# nothing; the player must NOT walk into an invisible wall on a public pavement. Those
# are different things and UE lets you have one without the other.
#
# ---------------------------------------------------------------------------
# WHERE "OUTSIDE" IS, WITHOUT GUESSING.
#
# The trigger has to sit on the STREET side of the door, not inside the shop. Rather than
# reason about the door mesh's yaw of 270 and which way its forward points - which is the
# same guess that had Kaia dancing in profile - this offsets toward the PlayerStart. The
# player arrives on the plaza, so the plaza is outside. Geometry the level already knows.

import json
import traceback

import unreal

CITY = "/Game/Maps/L_City"
CAFE = "/Game/Maps/L_Cafe"
CUBE = "/Game/LevelPrototyping/Meshes/SM_Cube"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/place_cafe_doors.json"

# build_d_mod_lvl1_window_door4 - read off the Details panel, standing in front of it.
DELI_DOOR = unreal.Vector(2374.075928, 1475.929565, 0.0)

STEP_OUT = 90.0     # cm from the shop door toward the plaza
TRIGGER_H = 110.0   # centre height, so a 2.2 m box spans floor to lintel

r = {}


def make_doorway(eas, label, where, target, prompt, tag):
    """An ACathedralDoor that is felt but not seen."""
    cls = unreal.load_class(None, "/Script/SibeliusGame.CathedralDoor")
    if not cls:
        raise Exception("CathedralDoor class not found - editor on an old build?")

    # Idempotent by TAG, never by label: labels do not cook, and this one has to survive
    # being re-run without piling up duplicates.
    cleared = 0
    for a in eas.get_all_level_actors():
        if tag in [str(t) for t in a.get_editor_property("tags")]:
            eas.destroy_actor(a)
            cleared += 1

    door = eas.spawn_actor_from_class(cls, where)
    door.set_actor_label(label)
    door.set_editor_property("tags", [unreal.Name(tag)])
    door.set_editor_property("TargetLevelName", unreal.Name(target))
    door.set_editor_property("bInteractive", True)
    door.set_editor_property("bRequireBattleToll", False)
    door.set_editor_property("bRequireGenerateUse", False)
    door.set_editor_property("PromptText", unreal.Text(prompt))

    mesh = door.get_component_by_class(unreal.StaticMeshComponent)
    body = {"had_component": mesh is not None}
    if mesh:
        cube = unreal.EditorAssetLibrary.load_asset(CUBE)
        if cube:
            mesh.set_editor_property("static_mesh", cube)
        # A metre square and 2.2 m tall - a person-sized volume in the doorway.
        door.set_actor_scale3d(unreal.Vector(1.0, 1.0, 2.2))
        mesh.set_editor_property("hidden_in_game", True)
        # QUERY ONLY: findable by the interactor's trace, walk-through-able by the player.
        mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        body["hidden_in_game"] = bool(mesh.get_editor_property("hidden_in_game"))
        got = mesh.get_editor_property("static_mesh")
        body["mesh"] = got.get_name() if got else None

    return door, cleared, body


try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level

    start = next((a for a in eas.get_all_level_actors()
                  if isinstance(a, unreal.PlayerStart)), None)
    if not start:
        raise Exception("no PlayerStart in this level - nothing to orient against")
    origin = start.get_actor_location()

    # ------------------------------------------------- the city half
    if open_level == CITY:
        # Toward the plaza, because that is where the player is standing when they read
        # the sign. Normalised by hand so a zero-length vector cannot divide by zero.
        dx, dy = origin.x - DELI_DOOR.x, origin.y - DELI_DOOR.y
        length = max(1.0, (dx * dx + dy * dy) ** 0.5)
        where = unreal.Vector(DELI_DOOR.x + dx / length * STEP_OUT,
                              DELI_DOOR.y + dy / length * STEP_OUT,
                              DELI_DOOR.z + TRIGGER_H)

        door, cleared, body = make_doorway(
            eas, "Door_ToCafe", where, "L_Cafe",
            "Jacob's Downtown Deli [E]", "CafeDoor")

        r["half"] = "city"
        r["cleared"] = cleared
        r["door_at"] = [round(where.x, 1), round(where.y, 1), round(where.z, 1)]
        r["stepped_toward_plaza"] = [round(dx / length, 2), round(dy / length, 2)]
        r["body"] = body
        r["readback"] = {
            "target": str(door.get_editor_property("TargetLevelName")),
            "interactive": bool(door.get_editor_property("bInteractive")),
        }
        r["next"] = ("Now open L_Cafe and run this again for the way back. And add "
                     "L_Cafe to MapsToCook before packaging, or [E] finds nothing.")

    # ------------------------------------------------- the cafe half
    elif open_level == CAFE:
        # Beside where he arrives, not on top of him - a door you spawn inside is a door
        # you press E on by accident before you have looked at the room.
        where = unreal.Vector(origin.x, origin.y, origin.z) + \
            start.get_actor_rotation().get_forward_vector() * -150.0

        door, cleared, body = make_doorway(
            eas, "Door_CafeReturn", where, "L_City",
            "Back to the street [E]", "CafeDoor")

        r["half"] = "cafe"
        r["cleared"] = cleared
        r["door_at"] = [round(where.x, 1), round(where.y, 1), round(where.z, 1)]
        r["body"] = body
        r["readback"] = {
            "target": str(door.get_editor_property("TargetLevelName")),
            "interactive": bool(door.get_editor_property("bInteractive")),
        }
        r["next"] = "Play it: walk to the deli, press E, look around, press E to come back."

    else:
        raise Exception("open %s or %s first - this level is %s"
                        % (CITY, CAFE, open_level))

    les.save_current_level()
    r["saved"] = True

    del start
    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###CAFEDOOR### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
