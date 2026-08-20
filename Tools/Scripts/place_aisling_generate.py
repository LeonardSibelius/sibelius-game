# place_aisling_generate.py — Aisling hands over GENERATE, and the last office sphere
# stands down.
#
# Kaia gives Refactor, Isla gives Deploy, Nyra gives Test-Drive (agent_grants_refactor.py,
# agents_grant_powers.py). Two shrines were still poles with a sphere on top:
#
#     PowerGrant_Compile    the library, the payoff of the 12-book hunt — STAYS a sphere.
#                           Walt: "Can that one stay there?" A reward you walked to is
#                           the opposite of an ambush.
#     PowerGrant_Generate   the study nook — THIS ONE. Walt, 2026-08-20.
#
# Generate is the right one to move to an agent even though it was never an ambush: it is
# the most AI-like power in the game ("ask for what you need and have it appear"), the one
# Mrs. Hall fights hardest, and docs/NARRATIVE.md's whole argument is that the powers ARE
# AI assistance made literal. Taking it from the hand of an actual AI agent IS the story.
# A sphere on a pole says none of that.
#
# MHC_Aisling was already in the project and never placed. She is the fourth dancer.
#
# WHY SHE HAS TO BE DANCING: UDancerAgentSubsystem's scan is behavioural — "an actor
# playing one of the ten Morro dances is a dancer" (DancerAgentSubsystem.h). A placed
# MetaHuman with no dance is never adopted, never grows a UDancerAgentComponent, and
# APowerGrant::BindToAgent gives up after 20 retries with GENERATE UNREACHABLE in the log
# — the shrine already hidden by then. So the dance is not decoration, it is the binding.
# She gets Slow_Rhythm_Dance_14, the one of the four slow dances nobody else uses
# (Kaia 12, Nyra 13, Isla 15).
#
# Position is TRACED, not guessed: a downward line trace finds the floor and a sphere
# overlap proves the spot is clear, so she cannot end up inside a bookshelf or standing in
# the office threshold. Idempotent — re-running moves the existing Aisling rather than
# spawning a second one.
#
# Editor-CLOSED (this is the whole point — it saves the level itself):
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
#       -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_aisling_generate.py"

import json
import unreal

MAP = "/Game/L_Office_v02"
AISLING_CLASS = "/Game/MetaHumans/MHC_Aisling/BP_MHC_Aisling.BP_MHC_Aisling_C"
DANCE = "/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_14_MH.Anim_Slow_Rhythm_Dance_14_MH"
LABEL = "BP_MHC_Aisling"
GRANT_LABEL = "PowerGrant_Generate"

# Her stand point, as an offset from the shrine. +X/-Y of it: open floor by the trace,
# on the side the player arrives from (PlayerStart is ~1150 units south at Y=9260), and
# ~470 from Nyra so the two of them do not read as one clump.
OFFSET = unreal.Vector(100.0, -200.0, 0.0)
YAW = -90.0   # face -Y, into the oncoming player

# False = if she is already in the level, leave her transform exactly as it is (a hand
# placement always wins over the traced one). True = re-place her at OFFSET/YAW.
MOVE_EXISTING = False

notes = []
ok = False

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

grant = None
aisling = None
for a in eas.get_all_level_actors():
    try:
        label = a.get_actor_label()
    except Exception:
        continue
    if label == GRANT_LABEL and a.get_class().get_name() == "PowerGrant":
        grant = a
    elif label == LABEL:
        aisling = a

anim = unreal.load_object(None, DANCE)
cls = unreal.load_class(None, AISLING_CLASS)

if grant is None:
    notes.append("no actor labelled %s in %s" % (GRANT_LABEL, MAP))
elif cls is None:
    notes.append("could not load %s" % AISLING_CLASS)
elif anim is None:
    notes.append("could not load the dance %s — is Characters/Retargeting present? "
                 "(it is gitignored)" % DANCE)
else:
    shrine = grant.get_actor_location()
    spot = unreal.Vector(shrine.x + OFFSET.x, shrine.y + OFFSET.y, shrine.z)

    # FLOOR, MEASURED. A MetaHuman's origin is at her feet; drop a trace and stand her on
    # whatever it hits rather than assuming the office floor is at 320 everywhere.
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(spot.x, spot.y, shrine.z + 200.0),
        unreal.Vector(spot.x, spot.y, shrine.z - 400.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
        unreal.DrawDebugTrace.NONE, True)

    if not hit:
        notes.append("no floor under (%.0f, %.0f) — refusing to drop her into the void"
                     % (spot.x, spot.y))
    else:
        floor = hit.to_tuple()[4]
        stand = unreal.Vector(spot.x, spot.y, round(floor.z, 1))

        # CLEAR, MEASURED. Ignore herself so a re-run is not blocked by last run's Aisling.
        ignore = [aisling] if aisling else []
        blockers = unreal.SystemLibrary.sphere_overlap_actors(
            world, unreal.Vector(stand.x, stand.y, stand.z + 95.0), 60.0,
            [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1,
             unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2], None, ignore) or []
        if blockers:
            notes.append("spot is blocked by %s — pick another OFFSET"
                         % [b.get_actor_label() for b in blockers][:4])
        else:
            if aisling is None:
                aisling = eas.spawn_actor_from_class(cls, stand, unreal.Rotator(0.0, 0.0, YAW))
                aisling.set_actor_label(LABEL)
                notes.append("spawned %s" % LABEL)
            elif MOVE_EXISTING:
                aisling.set_actor_location_and_rotation(
                    stand, unreal.Rotator(0.0, 0.0, YAW), False, True)
                notes.append("moved the existing %s to the traced spot" % LABEL)
            else:
                # HER TRANSFORM IS WALT'S, NOT THIS SCRIPT'S.
                #
                # The traced spot put her in a corner with her back to the room: the trace
                # proves the ACTOR ORIGIN stands on clear floor, and that is not the same
                # question as whether she fits. Her dance travels — the same thing that
                # made Nyra's body spend most of her dance outside her own collision
                # capsule (DancerAgentComponent::GetAimPoint) — so a spot that measures
                # clear at the origin can still walk her mesh into a wall.
                #
                # Eyeballing that is a human job, so once she exists this script does not
                # touch where she stands. Re-running it after a hand-placement would
                # otherwise silently drag her back into the corner, and the fix would look
                # like it had come undone by itself. Set MOVE_EXISTING to re-place her.
                loc = aisling.get_actor_location()
                notes.append("%s already placed at (%.0f, %.0f, %.0f) — left where she is "
                             "(MOVE_EXISTING is False)" % (LABEL, loc.x, loc.y, loc.z))

            # THE DANCE — this is what makes the scan adopt her (see the header).
            body = None
            for c in aisling.get_components_by_class(unreal.SkeletalMeshComponent):
                if c.get_name() == "Body":
                    body = c
                    break
            if body is None:
                notes.append("no 'Body' skeletal mesh on her — the scan will never adopt "
                             "her and GENERATE would be unreachable")
            else:
                body.set_editor_property("animation_mode",
                                         unreal.AnimationMode.ANIMATION_SINGLE_NODE)
                data = unreal.SingleAnimationPlayData()
                data.set_editor_property("anim_to_play", anim)
                # FSingleAnimationPlayData's flags are bSavedLooping/bSavedPlaying, and
                # Unreal's Python bindings strip the leading 'b' and snake_case the rest —
                # "looping" throws outright. Same trap dump_power_grants.py hit with
                # bGrantsPower. Try both spellings and record which one answered, so this
                # does not have to be rediscovered if the struct is ever renamed.
                for key in ("saved_looping", "looping"):
                    try:
                        data.set_editor_property(key, True)
                        notes.append("loop flag set via '%s'" % key)
                        break
                    except Exception:
                        continue
                for key in ("saved_playing", "playing"):
                    try:
                        data.set_editor_property(key, True)
                        notes.append("play flag set via '%s'" % key)
                        break
                    except Exception:
                        continue
                body.set_editor_property("animation_data", data)
                notes.append("Body dancing %s" % anim.get_name())

                grant.set_editor_property("granted_by_agent", aisling)
                notes.append("%s.GrantedByAgent -> %s" % (GRANT_LABEL, LABEL))

                les.save_current_level()
                ok = True
                notes.append("level saved")

# Report every grant afterwards, bound or not, so what is still a live sphere is visible.
final = []
for a in eas.get_all_level_actors():
    try:
        if a.get_class().get_name() != "PowerGrant":
            continue
        g = a.get_editor_property("granted_by_agent")
        final.append({"grant": a.get_actor_label(),
                      "power": str(a.get_editor_property("power")),
                      "granted_by_agent": g.get_actor_label() if g else None})
    except Exception:
        continue

payload = {
    "ok": ok,
    "map": MAP,
    "notes": notes,
    "all_grants_after": final,
    "expected_to_stay_a_sphere": "PowerGrant_Compile — the library, earned by the book hunt",
    "verify_in_PIE": "the log must say [Dancer] adopted 'BP_MHC_Aisling' as AI Agent Aisling, "
                     "then [PowerGrant] GENERATE is now given by BP_MHC_Aisling",
}

text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "place_aisling_generate.json"
try:
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)
    unreal.log("[place_aisling_generate] wrote " + out)
except Exception as e:
    unreal.log_error("[place_aisling_generate] could not write %s: %s" % (out, e))

print(text)
