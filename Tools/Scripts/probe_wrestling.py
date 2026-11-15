# probe_wrestling.py - measure the wrestling pack's paired moves, headless.
#
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript -script=".../probe_wrestling.py"
#
# Read-only. Writes Saved/probe_wrestling.json: per move and time, the root (position, yaw) and
# the component-space positions of the pelvis, both hands and the head, for the pack's source
# animation and our retargeted copy. Two questions it answers:
#   1. How far apart the pack stands the two wrestlers (hands must meet in the test of strength).
#   2. Whether the retargeted copies kept the root motion (a throw carries her 1-2.6 m).

import json
import traceback

import unreal

PACK = "/Game/Wrestler_Finishers_Volume2/Anim_Seq"
MH = "/Game/Characters/Retargeting/Anims/Wrestling"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/probe_wrestling.json"

SRC_SPINE = ["spine_01", "spine_02", "spine_03"]
MH_SPINE = ["spine_01", "spine_02", "spine_03", "spine_04", "spine_05"]


def chains(spine):
    top = spine[-1]
    return {
        "pelvis": ["root", "pelvis"],
        "hand_l": ["root", "pelvis"] + spine + ["clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"],
        "hand_r": ["root", "pelvis"] + spine + ["clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"],
        "head": ["root", "pelvis"] + spine + (["neck_01", "neck_02", "head"] if top == "spine_05" else ["neck_01", "head"]),
    }


def at(anim, t, spine):
    out = {}
    for key, chain in chains(spine).items():
        poses = unreal.AnimationLibrary.get_bone_poses_for_time(anim, chain, t, False)
        world = poses[0]
        if key == "pelvis":
            out["root"] = [round(world.translation.x, 1), round(world.translation.y, 1),
                           round(world.rotation.rotator().yaw, 1)]
        for local in poses[1:]:
            world = unreal.MathLibrary.compose_transforms(local, world)
        v = world.translation
        out[key] = [round(v.x, 1), round(v.y, 1), round(v.z, 1)]
    return out


r = {"moves": {}}
try:
    eal = unreal.EditorAssetLibrary
    names = []
    for move in ("TestOfStrength_Loop", "HeadLock_Loop", "HipThrow", "BostonCrab_Loop", "HeadLockFlipOver"):
        for side in ("Att", "Vic"):
            names.append(("Wrestling_Paired", "Wrestling_Paired_%s_%s" % (move, side)))
    for side in ("Attacker", "Victim"):
        names.append(("Wrestling_Pin", "Wrestling_Pin_Loop01_" + side))
    names.append(("Wrestling_Pin", "Wrestling_Pin_Referee_Loop"))
    for folder, name in names:
        for label, path, spine in (("src", "%s/%s/%s" % (PACK, folder, name), SRC_SPINE),
                                   ("mh", "%s/%s_MH" % (MH, name), MH_SPINE)):
            anim = eal.load_asset(path)
            if not anim:
                r["moves"]["%s %s" % (label, name)] = "missing"
                continue
            length = unreal.AnimationLibrary.get_sequence_length(anim)
            entry = {"len": round(length, 2)}
            for frac in (0.0, 0.5, 1.0):
                t = min(length * frac, max(0.0, length - 0.001))
                try:
                    entry["t%.1f" % frac] = at(anim, t, spine)
                except Exception as e:
                    entry["t%.1f" % frac] = "err " + str(e)[:150]
            r["moves"]["%s %s" % (label, name)] = entry
    # control: a dance retarget that is known to play
    bow = eal.load_asset("/Game/Characters/Retargeting/Anim_Bow_MH")
    if bow:
        r["moves"]["mh Anim_Bow_MH"] = {"t0.5": at(bow, unreal.AnimationLibrary.get_sequence_length(bow) * 0.5, MH_SPINE)}
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=1)
unreal.log("###PROBE WRESTLING### done")
