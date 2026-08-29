# place_city_dancers.py - Kaia and Nyra, dancing on the street when he arrives.
#
# *** RUN FROM THE OPEN EDITOR WITH L_City ALREADY OPEN. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_city_dancers.py"
#
# It refuses to run on any other level and it never changes levels itself - Python that
# loads or deletes a .umap crashes this editor (FPyReferenceCollector holds the world
# alive; EditorServer.cpp:2524). See make_city_from_downtown.py for the full story.
#
# ===========================================================================
# WHY THIS IS SIX LINES OF PLACEMENT AND NOT A FEATURE.
#
# UDancerAgentSubsystem decides what a dancer is BEHAVIOURALLY: any actor with a
# skeletal mesh playing one of the ten Morro dances is an AI Agent, full stop. No tag to
# set, no component to add, no Blueprint to re-parent. So making Kaia and Nyra real
# agents on this street is entirely a matter of standing them there and starting the
# music - E to talk and F to reshuffle her dance come free, because the subsystem will
# find them within five seconds of BeginPlay and adopt them.
#
# That design was written to survive MetaHuman re-assembly wiping hand-added components.
# It also means a new level costs nothing, which is what is being spent here.
#
# ---------------------------------------------------------------------------
# THE NAMES COME FROM THE CLASS, NOT FROM A LABEL - AND THAT IS LOAD-BEARING.
#
# PrettyAgentName() reads GetActorNameOrLabel(), which returns the LABEL in the editor
# and PIE and the OBJECT NAME in a packaged build. So a tidy label like "City_Kaia"
# would introduce her correctly all through testing and then ship as "I am AI Agent
# City_Kaia" - the actor-label trap this project has already been bitten by once.
#
# So these actors keep their default labels (BP_Dancer_Kaia, BP_MHC_NyraSolmere), which
# PrettyAgentName already knows how to cut down to "Kaia" and "Nyra" - it even handles
# the CamelCase surname. Idempotency uses an ACTOR TAG instead, which is real data that
# cooks and does not touch the name.
#
# ---------------------------------------------------------------------------
# WHAT THIS COSTS THE DOWNLOAD: nothing measurable. Both MetaHumans and all ten dances
# are already in the pak - the office has them, and UDancerAgentComponent's CDO
# hard-references every dance. This adds references to assets that already ship.

import json
import traceback

import unreal

LEVEL = "/Game/Maps/L_City"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/place_city_dancers.json"
TAG = "CityDancer"          # actor tag - cooks, and does not disturb the agent name

# The same two dances they do in the office. Deliberate continuity: the man has met
# these two before, at his desk, and here they are doing the very same steps in the sun.
# THE FOURTH NUMBER IS A FACING FIX, and it was measured rather than reasoned about.
#
# First run: PlayerStart yaw 0, Kaia's actor yaw set to 180 to face back at him - and
# she rendered in profile, facing -Y. So this mesh points 90 degrees off actor-forward,
# and the yaw that actually faces the player is playerYaw + 90, not + 180.
#
# It is PER DANCER because there is no guarantee the two Blueprints agree: Kaia has a
# purpose-built BP_Dancer_Kaia and Nyra is a stock MetaHuman BP. If one of them ends up
# facing away, flip HER number by 180 and re-run - nothing else needs to change.
#
#   (class, dance, sideways offset in cm, yaw to add to the player's)
WHO = [
    ("/Game/MetaHumans/MHC_Kaia/BP_Dancer_Kaia.BP_Dancer_Kaia_C",
     "/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_10_MH.Anim_Mid_Rhythm_Dance_10_MH",
     -110.0, 90.0),
    ("/Game/MetaHumans/MHC_NyraSolmere/BP_MHC_NyraSolmere.BP_MHC_NyraSolmere_C",
     "/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_14_MH.Anim_Slow_Rhythm_Dance_14_MH",
     110.0, 90.0),
]

# How far down the street they stand, and how far apart. Four metres is close enough to
# read a face and far enough not to be standing on him the instant the fade lifts.
AHEAD = 400.0
FEET_BELOW_START = 90.0   # PlayerStart's origin is capsule-centre; a MetaHuman's is feet

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level
    if open_level != LEVEL:
        raise Exception("open %s first (File > Open Level). This script never changes "
                        "levels - Python that does crashes the editor." % LEVEL)

    actors = eas.get_all_level_actors()

    # ---- where he arrives -------------------------------------------------
    start = next((a for a in actors if isinstance(a, unreal.PlayerStart)), None)
    if not start:
        raise Exception("no PlayerStart in L_City - nothing to dance in front of")
    origin = start.get_actor_location()
    rot = start.get_actor_rotation()
    fwd = rot.get_forward_vector()
    right = rot.get_right_vector()
    r["player_start"] = [round(origin.x, 1), round(origin.y, 1), round(origin.z, 1)]
    r["player_yaw"] = round(rot.yaw, 1)

    # ---- clear any previous run (by TAG, never by label) -------------------
    cleared = 0
    for a in actors:
        if TAG in [str(t) for t in a.get_editor_property("tags")]:
            eas.destroy_actor(a)
            cleared += 1
    r["cleared"] = cleared

    # ---- stand them up ----------------------------------------------------
    def start_the_music(actor, anim):
        """Play a dance on the BODY only.

        NOT the face. DancerAgentComponent.h is explicit that driving the MetaHuman face
        rig from code wrecked the portrait once and the rig is to be left as MetaHuman
        assembled it - the talk close-up depends on that. Body is also the mesh the
        dancer scan looks at, so it is the only one that needs to be playing.
        """
        meshes = actor.get_components_by_class(unreal.SkeletalMeshComponent)
        body = next((m for m in meshes if "body" in m.get_name().lower()), None)
        if not body and meshes:
            body = meshes[0]
        if not body:
            return {"danced": False, "why": "no skeletal mesh on the actor"}

        # THE FIELD NAMES ARE NOT WHAT THEY LOOK LIKE. FSingleAnimationPlayData stores
        # bSavedLooping / bSavedPlaying / SavedPlayRate, so Python wants saved_looping
        # and friends - "looping" threw and took the whole script down with it before
        # Nyra was ever spawned. Every set is tried and RECORDED here rather than
        # assumed, so a rename in a future engine version degrades to a report instead
        # of a stack trace: the dance is what matters and the rest are refinements.
        data = unreal.SingleAnimationPlayData()
        applied = {}

        def put(candidates, value):
            for name in candidates:
                try:
                    data.set_editor_property(name, value)
                    applied[candidates[0]] = name
                    return
                except Exception:
                    continue
            applied[candidates[0]] = None

        put(["anim_to_play"], anim)
        put(["saved_looping", "looping", "b_saved_looping"], True)
        put(["saved_playing", "playing", "b_saved_playing"], True)
        put(["saved_play_rate", "play_rate"], 1.0)

        body.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        body.set_editor_property("animation_data", data)

        back = body.get_editor_property("animation_data").get_editor_property("anim_to_play")
        return {
            "mesh": body.get_name(),
            "anim": back.get_name() if back else None,
            "danced": back is not None,
            "fields": applied,
        }

    placed = []
    for class_path, anim_path, sideways, yaw_add in WHO:
        cls = unreal.load_class(None, class_path)
        anim = unreal.EditorAssetLibrary.load_asset(anim_path)
        if not cls:
            placed.append({"class": class_path, "error": "class not found"})
            continue
        if not anim:
            placed.append({"class": class_path, "error": "dance not found: %s" % anim_path})
            continue

        where = origin + fwd * AHEAD + right * sideways
        where.z = origin.z - FEET_BELOW_START

        a = eas.spawn_actor_from_class(cls, where)
        # FACING HIM - they are dancing TO the man who just walked out of a battle, not
        # past him. See WHO for why the number is 90 and not the obvious 180.
        facing = rot.yaw + yaw_add
        a.set_actor_rotation(unreal.Rotator(0.0, 0.0, facing), False)
        a.set_editor_property("tags", [unreal.Name(TAG)])
        try:
            a.set_folder_path("Dancers")
        except Exception:
            pass

        # Each dancer is her own transaction. The first version let an exception inside
        # start_the_music abort the loop, so a bad property name on Kaia meant Nyra was
        # never placed at all - one failure, two missing people.
        try:
            music = start_the_music(a, anim)
        except Exception as e:
            music = {"danced": False, "why": str(e)}

        placed.append({
            "label": a.get_actor_label(),          # must stay the default - see the header
            "at": [round(where.x, 1), round(where.y, 1), round(where.z, 1)],
            "yaw": round(facing, 1),
            "music": music,
        })

    r["placed"] = placed
    r["all_dancing"] = all(p.get("music", {}).get("danced") for p in placed) and len(placed) == 2

    les.save_current_level()
    r["saved"] = True
    r["next"] = ("Play L_City. They should be four metres ahead, facing you, dancing. "
                 "E to talk to one, F to reshuffle her dance. If they stand still, the "
                 "scan did not adopt them - check the log for '[Dancer] first scan'.")

    del actors, start, placed
    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###DANCERS### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
