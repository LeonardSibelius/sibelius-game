# measure_dance_hands.py — how far a dance bends the agents' hands away from a relaxed MetaHuman hand.
#
# Walt, 2026-10-07: "the hands distort terribly on the 44 new dances" (the Mixamo ones, retargeted
# by import_mixamo_dances.py). The Morro dances retarget cleanly - they are the yardstick. For each
# hand bone, the largest angle (degrees) between its local rotation in the dance and in the
# MetaHuman reference pose, over a spread of frames: a relaxed hand moves its fingers tens of
# degrees; a mangled one turns them through 90 or more.
#
# Run with the editor open (UnrealClaude), or headless: -run=pythonscript -script=<this file>.

import math
import unreal

TAG = "###HANDS###"
DANCES = {
    "Morro (good)": "/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_12_MH",
    "Mixamo YMCA (bad)": "/Game/Mixamo/Dances/Anim_YMCA_MH",        # the first set, hands wrenched
    "Mixamo YMCA": "/Game/Mixamo/Retargeted/Anim_YMCA_MH",          # after the metacarpal fix
    "Mixamo Chicken": "/Game/Mixamo/Retargeted/Anim_Chicken_Dance_MH",
    "Mixamo Hip Hop Tut": "/Game/Mixamo/Retargeted/Anim_Hip_Hop_Tut_MH",
}
# bone -> parent, left hand (the right mirrors it)
BONES = {
    "hand_l": "lowerarm_l",
    "thumb_01_l": "hand_l", "thumb_02_l": "thumb_01_l", "thumb_03_l": "thumb_02_l",
    "index_metacarpal_l": "hand_l", "index_01_l": "index_metacarpal_l", "index_02_l": "index_01_l",
    "middle_metacarpal_l": "hand_l", "middle_01_l": "middle_metacarpal_l", "middle_02_l": "middle_01_l",
    "pinky_metacarpal_l": "hand_l", "pinky_01_l": "pinky_metacarpal_l", "pinky_02_l": "pinky_01_l",
}

mh = unreal.IKRigController.get_controller(unreal.load_asset("/Game/Characters/Retargeting/IK_Metahuman"))


def ref_local(bone):
    child = mh.get_ref_pose_transform_of_bone(bone)
    parent = mh.get_ref_pose_transform_of_bone(BONES[bone])
    return parent.inverse() * child   # parent's frame


def angle(qa, qb):
    d = abs(qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w)
    return math.degrees(2.0 * math.acos(min(1.0, d)))


ref = {b: ref_local(b).rotation for b in BONES}
for label, path in DANCES.items():
    anim = unreal.load_asset(path)
    if not anim:
        print(TAG, label, "MISSING", path)
        continue
    frames = anim.get_editor_property("number_of_sampled_frames") if hasattr(anim, "number_of_sampled_frames") else unreal.AnimationLibrary.get_num_frames(anim)
    picks = sorted({int(frames * f) for f in (0.1, 0.3, 0.5, 0.7, 0.9)})
    worst = {}
    for b in BONES:
        worst[b] = max(angle(unreal.AnimationLibrary.get_bone_pose_for_frame(anim, b, f, False).rotation, ref[b]) for f in picks)
    fingers = [v for b, v in worst.items() if b != "hand_l"]
    print(TAG, "%-20s wrist %5.0f | fingers: worst %5.0f, mean %5.0f | %s" % (
        label, worst["hand_l"], max(fingers), sum(fingers) / len(fingers),
        ", ".join("%s %.0f" % (b.replace("_l", ""), v) for b, v in worst.items() if v > 60)))
