# bake_kart_driving.py — Mixamo's car-driving pose, re-seated for the Grand Prix's kart.
#
# Anim_Driving_MH (import_mixamo_driving.py) sits like a car driver: thighs level, knees at 90
# degrees, feet down on the floor 40 cm below the hips, hands 27 cm in front of the chest. The race
# kart (Polygonal Assets' Race kart, measured from its OBJ export, Saved/KartSurvey/race_kart.obj -
# never looked at: the pack forbids AI use) wants the opposite: hips on a pan 26 cm off the street,
# legs out along the floor under the nose cowl, arms out to a wheel 80 cm ahead of the seat back.
#
# So this copies it to Anim_Driving_Kart_MH and re-aims both arms and both legs every frame with a
# two-bone IK - shoulder-elbow-wrist onto the wheel's grips, hip-knee-ankle onto the footwell -
# keeping everything else Mixamo's (the head, the breathing, the small corrections at the wheel).
# Only the four bones of each limb are rewritten; twist bones and fingers follow their parents.
# The same again for Leonard's driver, Manny (Anim_Driving_Manny -> Anim_Driving_Kart_Manny), whose
# root is shifted too, so his hips land on the same HIP from the same ORIGIN at his own scale.
#
# The numbers are the kart's, in its own frame (cm, +X forward, +Y right, +Z up, KartModelScale
# 0.85 applied), and must match AKartRace's driver placement: ORIGIN and each job's scale are
# KartShapes::DriverOrigin, DriverScale and MannyScale in KartRace.h. Run INSIDE the editor, PIE
# stopped.

import math
import unreal

TAG = "###KARTDRIVE###"
DIR = "/Game/Mixamo/Driving"
JOBS = [  # (source, the mesh it is measured on, result, scale)
    # the kart is ~1.36x a real one; the agents grow to fill it
    (DIR + "/Anim_Driving_MH.Anim_Driving_MH",
     "/Game/MetaHumans/MHC_Aisling/Body/SKM_MHC_Aisling_BodyMesh.SKM_MHC_Aisling_BodyMesh", "Anim_Driving_Kart_MH", 1.3),
    # Manny stands taller than the agents: grown less
    (DIR + "/Anim_Driving_Manny.Anim_Driving_Manny",
     "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple", "Anim_Driving_Kart_Manny", 1.2),
]
ORIGIN = (-15.6, -1.0, -35.2) # every driver's mesh origin in the kart (the agents' own, measured first)
HIP = (-15.0, 0.0, 38.0)      # hip joints' midpoint: 12 cm (scaled flesh) above the 26 cm seat pan
WHEEL = (40.0, 0.0, 71.0)     # wheel centre; its face looks up and back along FACE
FACE = (-0.78, 0.0, 0.62)
GRIP = 19.0                   # quarter-to-three: the grips' distance either side of the centre
WRIST_BACK = 7.0              # the wrist sits this far behind the rim, toward the driver
ANKLE = (78.0, 16.0, 24.0)    # footwell, under the cowl (its top 55-65 cm from x 60 to 100);
                             # far enough that the knees stay under and beside the wheel

LIMBS = [  # (upper, lower, end, side) - side +1 is the kart's right (+Y)
    ("upperarm_l", "lowerarm_l", "hand_l", -1), ("upperarm_r", "lowerarm_r", "hand_r", 1),
    ("thigh_l", "calf_l", "foot_l", -1), ("thigh_r", "calf_r", "foot_r", 1),
]

eal = unreal.EditorAssetLibrary
W, L = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL


def say(text):
    print("%s %s" % (TAG, text))


# --- plain vector and quaternion maths (x, y, z) / (x, y, z, w), Hamilton, as FQuat ------------
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def norm(a): return mul(a, 1.0 / max(length(a), 1e-9))


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(q): return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q))
    return (r[0], r[1], r[2])


def from_to(a, b):
    """The shortest rotation taking direction a onto direction b."""
    a, b = norm(a), norm(b)
    c = dot(a, b)
    if c < -0.9999:
        axis = norm(cross(a, (1.0, 0.0, 0.0) if abs(a[0]) < 0.9 else (0.0, 1.0, 0.0)))
        return (axis[0], axis[1], axis[2], 0.0)
    x = cross(a, b)
    q = (x[0], x[1], x[2], 1.0 + c)
    n = math.sqrt(sum(t * t for t in q))
    return tuple(t / n for t in q)


def two_bone(root, mid, end, target, pole):
    """Where the middle joint goes so the end lands on target (clamped to 98% of reach), bending
    toward pole - the elbow out and down, the knee up and out."""
    l1, l2 = length(sub(mid, root)), length(sub(end, mid))
    to = sub(target, root)
    d = min(max(length(to), abs(l1 - l2) + 0.1), 0.98 * (l1 + l2))
    axis = norm(to)
    end_new = add(root, mul(axis, d))
    cos_a = (l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d)
    a = math.acos(max(-1.0, min(1.0, cos_a)))
    bend = norm(sub(pole, mul(axis, dot(pole, axis))))
    mid_new = add(root, add(mul(axis, l1 * math.cos(a)), mul(bend, l1 * math.sin(a))))
    return mid_new, end_new, length(to) / (l1 + l2)


# The kart frame and the mesh's component frame: a MetaHuman (and Manny) faces +Y with its left
# at +X, the kart faces +X with its left at -Y (the -90 yaw AKartRace seats them with), then scale.
def to_kart(p, origin, scale):
    return add(origin, mul((p[1], -p[0], p[2]), scale))


def to_mesh(k, origin, scale):
    d = mul(sub(k, origin), 1.0 / scale)
    return (-d[1], d[0], d[2])


def vec(t): return (t.translation.x, t.translation.y, t.translation.z)
def quat(t): return (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w)


def bake(source_path, mesh_path, out_name, scale):
    source = unreal.load_asset(source_path)
    mesh = unreal.load_asset(mesh_path)
    if not source or not mesh:
        say("SKIPPED %s: no %s - run import_mixamo_driving.py first" % (out_name, source_path if not source else mesh_path))
        return
    out_path = "%s/%s" % (DIR, out_name)
    # A re-run rewrites the same keys from the source - never deletes and remakes the asset.
    out = unreal.load_asset(out_path) if eal.does_asset_exist(out_path) else eal.duplicate_asset(source_path.split(".")[0], out_path)

    opts = unreal.AnimPoseEvaluationOptions()
    opts.set_editor_property("optional_skeletal_mesh", mesh)
    frames = unreal.AnimationLibrary.get_num_frames(source) + 1   # keys: one more than the frames between them

    # Where this mesh's origin would have to be for its hips to sit on HIP - and the root shift that
    # brings it to the shared ORIGIN instead (zero, near enough, for the agents).
    pose0 = unreal.AnimPoseExtensions.get_anim_pose_at_frame(source, 0, opts)
    hip_mid = mul(add(vec(unreal.AnimPoseExtensions.get_bone_pose(pose0, "thigh_l", W)),
                      vec(unreal.AnimPoseExtensions.get_bone_pose(pose0, "thigh_r", W))), 0.5)
    origin = sub(HIP, mul((hip_mid[1], -hip_mid[0], hip_mid[2]), scale))
    shift = to_mesh(origin, ORIGIN, scale)
    say("%s: its own origin (%.1f, %.1f, %.1f) at scale %.2f, root shifted (%.1f, %.1f, %.1f) to the shared one, %d keys" % (
        (out_name,) + origin + (scale,) + shift + (frames,)))

    face = norm(FACE)
    side_axis = (0.0, 1.0, 0.0)
    keys = {b: ([], [], []) for limb in LIMBS for b in limb[:2]}   # bone: (positions, rotations, scales), local
    keys["root"] = ([], [], [])
    reach = {}
    report = {}
    for f in range(frames):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(source, f, opts)
        root = unreal.AnimPoseExtensions.get_bone_pose(pose, "root", L)
        keys["root"][0].append(unreal.Vector(*add(vec(root), shift)))
        keys["root"][1].append(root.rotation)
        keys["root"][2].append(root.scale3d)
        for upper, lower, end, side in LIMBS:
            wu = unreal.AnimPoseExtensions.get_bone_pose(pose, upper, W)
            wl = unreal.AnimPoseExtensions.get_bone_pose(pose, lower, W)
            we = unreal.AnimPoseExtensions.get_bone_pose(pose, end, W)
            pu, pl, pe = vec(wu), vec(wl), vec(we)
            if end.startswith("hand"):
                grip = add(WHEEL, mul(side_axis, GRIP * side))
                target_k = add(grip, mul(face, WRIST_BACK))
                pole_k = (0.0, 0.8 * side, -1.0)                 # elbows out and down
            else:
                target_k = (ANKLE[0], ANKLE[1] * side, ANKLE[2])
                pole_k = (0.0, 0.6 * side, 1.0)                  # knees up, apart round the column
            target = to_mesh(target_k, origin, scale)
            pole = to_mesh(pole_k, (0.0, 0.0, 0.0), 1.0)
            mid_new, end_new, r = two_bone(pu, pl, pe, target, pole)
            reach[end] = max(reach.get(end, 0.0), r)
            q1 = from_to(sub(pl, pu), sub(mid_new, pu))
            q2 = from_to(qrot(q1, sub(pe, pl)), sub(end_new, mid_new))
            # Back to local: upper' = P^-1 q1 P upper ; lower' = U^-1 q1^-1 q2 q1 U lower (U: upper's world).
            lu = quat(unreal.AnimPoseExtensions.get_bone_pose(pose, upper, L))
            ll = quat(unreal.AnimPoseExtensions.get_bone_pose(pose, lower, L))
            uw = quat(wu)
            pw = qmul(uw, qinv(lu))                               # the upper bone's parent, in world
            upper_local = qmul(qmul(qmul(qinv(pw), q1), pw), lu)
            lower_local = qmul(qmul(qmul(qmul(qmul(qinv(uw), qinv(q1)), q2), q1), uw), ll)
            for bone, rot in ((upper, upper_local), (lower, lower_local)):
                local = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, L)
                keys[bone][0].append(local.translation)
                keys[bone][1].append(unreal.Quat(*rot))
                keys[bone][2].append(local.scale3d)
            if f == 0:
                report[end] = to_kart(end_new, origin, scale)

    ctrl = out.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("Seat the driver in the kart"), True)
    try:
        for bone, (pos, rots, scl) in keys.items():
            ctrl.set_bone_track_keys(bone, pos, rots, scl, True)
    finally:
        ctrl.close_bracket(True)
    eal.save_loaded_asset(out)

    # The check, on the saved asset and from the shared ORIGIN, as the game seats it: does each
    # wrist and ankle land where it was aimed?
    check = unreal.AnimPoseExtensions.get_anim_pose_at_frame(out, 0, opts)
    at = lambda b: to_kart(vec(unreal.AnimPoseExtensions.get_bone_pose(check, b, W)), ORIGIN, scale)
    for upper, lower, end, side in LIMBS:
        got, mid, want = at(end), at(lower), report[end]
        say("  %-7s at (%.0f, %.0f, %.0f), aimed (%.0f, %.0f, %.0f), off %.1f cm; %s at (%.0f, %.0f, %.0f); reach used up to %.0f%%" % (
            end, got[0], got[1], got[2], want[0], want[1], want[2], length(sub(got, want)), lower, mid[0], mid[1], mid[2],
            100.0 * reach[end]))
    for b in ("pelvis", "head", "ball_l", "ball_r"):
        say("  %-7s at (%.0f, %.0f, %.0f)" % ((b,) + at(b)))


for job in JOBS:
    bake(*job)
