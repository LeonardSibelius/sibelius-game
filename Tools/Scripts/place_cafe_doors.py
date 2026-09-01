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

# The arrival PlayerStart sits further out than the door trigger, so stepping back into
# the city does not put him standing inside the volume he just used.
ARRIVE_OUT = 200.0  # cm from the shop door toward the plaza
STAND_H = 100.0     # capsule centre above the pavement
ARRIVE_TAG = "DeliDoor"   # must match UDancerAgentComponent::GuideStage1StartTag

r = {}


def make_doorway(eas, label, where, target, prompt, tag, arrival=""):
    """An ACathedralDoor that is felt but not seen.

    `arrival` names a PlayerStartTag in the TARGET level. Empty (the default, and every
    other door in the game) means the target's ordinary spawn.
    """
    cls = unreal.load_class(None, "/Script/SibeliusGame.CathedralDoor")
    if not cls:
        raise Exception("CathedralDoor class not found - editor on an old build?")

    # Idempotent by TAG, never by label: labels do not cook, and this one has to survive
    # being re-run without piling up duplicates.
    #
    # A HAND-MOVED DOOR STAYS WHERE THE HUMAN PUT IT. The computed position is a first
    # guess from the PlayerStart, and the cafe's guess was wrong - Walt dragged that door
    # to where it actually belongs and it worked. Re-running this used to throw that away
    # and silently put it back in the wall, which is the worst kind of tool: one that
    # undoes your work while reporting success. So the old transform is inherited, and
    # only the PROPERTIES are refreshed.
    cleared = 0
    kept = None
    for a in eas.get_all_level_actors():
        if tag in [str(t) for t in a.get_editor_property("tags")]:
            if kept is None:
                kept = (a.get_actor_location(), a.get_actor_rotation())
            eas.destroy_actor(a)
            cleared += 1

    if kept is not None:
        where = kept[0]

    door = eas.spawn_actor_from_class(cls, where)
    if kept is not None:
        door.set_actor_rotation(kept[1], False)
    door.set_actor_label(label)
    door.set_editor_property("tags", [unreal.Name(tag)])
    door.set_editor_property("TargetLevelName", unreal.Name(target))
    door.set_editor_property("bInteractive", True)
    door.set_editor_property("bRequireBattleToll", False)
    door.set_editor_property("bRequireGenerateUse", False)
    door.set_editor_property("PromptText", unreal.Text(prompt))
    door.set_editor_property("ArrivalTag", unreal.Name(arrival))

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

    body["kept_hand_placed_transform"] = kept is not None
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
        # ---- and the spot he arrives on when he comes back out -----------
        #
        # WHY A SECOND PLAYERSTART. Coming out of the deli used to drop him at the city's
        # default spawn, a street away on the plaza - which is fine when [>] from the
        # meadow is the only way in, and wrong the moment there are two doors. Nyra
        # "waiting for him outside" means nothing if outside is somewhere else.
        #
        # ONE MARKER DEFINES THE WHOLE MEETING. The cafe's return door names the tag,
        # ASibeliusGameGameMode::ChoosePlayerStart finds this actor by it, and
        # UDancerAgentComponent::ApplyGuideStage stands Nyra in front of the same actor.
        # Drag it in the editor and the arrival, the facing and the guide all move
        # together - there is no second copy of the position to forget.
        start_where = unreal.Vector(DELI_DOOR.x + dx / length * ARRIVE_OUT,
                                    DELI_DOOR.y + dy / length * ARRIVE_OUT,
                                    DELI_DOOR.z + STAND_H)
        # Looking the way he stepped: out of the shop, toward the plaza. Nyra reads this
        # rotation to place herself in his view and turn back to face him.
        facing = unreal.Rotator(0.0, 0.0,
                                unreal.MathLibrary.find_look_at_rotation(
                                    DELI_DOOR, start_where).yaw)

        cleared_starts = 0
        for a in eas.get_all_level_actors():
            if ARRIVE_TAG in [str(t) for t in a.get_editor_property("tags")]:
                eas.destroy_actor(a)
                cleared_starts += 1

        ps = eas.spawn_actor_from_class(unreal.PlayerStart, start_where, facing)
        ps.set_actor_label("Start_DeliDoor")
        ps.set_editor_property("tags", [unreal.Name(ARRIVE_TAG)])
        # PlayerStartTag is the property the GameMode matches - NOT the actor tag above,
        # which only exists so re-running this does not pile up duplicates.
        ps.set_editor_property("player_start_tag", unreal.Name(ARRIVE_TAG))

        r["arrival"] = {
            "cleared": cleared_starts,
            "at": [round(start_where.x, 1), round(start_where.y, 1), round(start_where.z, 1)],
            "yaw": round(facing.yaw, 1),
            # Read back, not assumed: a tag that silently failed to set would send him to
            # the plaza and leave Nyra talking to an empty pavement.
            "player_start_tag": str(ps.get_editor_property("player_start_tag")),
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
            "Back to the street [E]", "CafeDoor", arrival=ARRIVE_TAG)

        r["half"] = "cafe"
        r["cleared"] = cleared
        r["door_at"] = [round(where.x, 1), round(where.y, 1), round(where.z, 1)]
        r["body"] = body
        r["readback"] = {
            "target": str(door.get_editor_property("TargetLevelName")),
            "interactive": bool(door.get_editor_property("bInteractive")),
            # The whole point of the cafe half now. If this reads None, coming out of the
            # deli lands on the plaza and Nyra is waiting round the corner for nobody.
            "arrival_tag": str(door.get_editor_property("ArrivalTag")),
        }
        r["next"] = ("Play it: walk to the deli, press E, look around, press E to come "
                     "back - and check you come out on the pavement, not the plaza.")

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
