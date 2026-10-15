# bake_basketball_run.py - Mixamo's "Running", made to run on the spot, for the three-point contest.
#
# Walt, 2026-10-15: "can the players please just run from station to station?". The Running he
# downloaded is not In Place: its hips travel 300 cm in the 0.7 s loop (import_mixamo_basketball.py
# measured it), and ABasketballCourt moves the shooter itself - she would run 3 m ahead of where
# the court puts her, and snap back every loop. So this copies Anim_Running_MH and _Manny to
# Anim_RunInPlace_MH and _Manny and takes the steady travel out: whichever of root and pelvis
# carries it, its floor-plane drift is subtracted frame by frame (linearly - a run is a constant
# speed), so the last frame lands on the first and the loop closes. Everything else is Mixamo's.
#
# The travel it removes is the ground speed the stride is made for: RunStrideSpeed in
# BasketballCourt.cpp (430 cm/s, both bodies).
#
# Run INSIDE the editor, PIE stopped, after import_mixamo_basketball.py. Re-running rewrites the
# copies from the originals.

import unreal

TAG = "###RUNINPLACE###"
DIR = "/Game/Mixamo/Basketball"
JOBS = [("Anim_Running_MH", "Anim_RunInPlace_MH", "/Game/MetaHumans/MHC_Aisling/Body/SKM_MHC_Aisling_BodyMesh"),
        ("Anim_Running_Manny", "Anim_RunInPlace_Manny", "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")]
BONES = ("root", "pelvis")

eal = unreal.EditorAssetLibrary
W, L = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL


def say(text):
    print("%s %s" % (TAG, text))


def travel(clip, opts, frames):
    first = unreal.AnimPoseExtensions.get_anim_pose_at_frame(clip, 0, opts)
    last = unreal.AnimPoseExtensions.get_anim_pose_at_frame(clip, frames - 1, opts)
    a = unreal.AnimPoseExtensions.get_bone_pose(first, "pelvis", W).translation
    b = unreal.AnimPoseExtensions.get_bone_pose(last, "pelvis", W).translation
    return b.x - a.x, b.y - a.y


for src_name, out_name, mesh_path in JOBS:
    src_path = "%s/%s" % (DIR, src_name)
    out_path = "%s/%s" % (DIR, out_name)
    source = unreal.load_asset(src_path)
    mesh = unreal.load_asset(mesh_path)
    if not source or not mesh:
        say("SKIPPED %s: run import_mixamo_basketball.py first" % out_name)
        continue
    if eal.does_asset_exist(out_path):
        eal.delete_asset(out_path)   # a copy this script made; the original is untouched
    out = eal.duplicate_asset(src_path, out_path)
    opts = unreal.AnimPoseEvaluationOptions()
    opts.set_editor_property("optional_skeletal_mesh", mesh)
    frames = unreal.AnimationLibrary.get_num_frames(source) + 1
    length = unreal.AnimationLibrary.get_sequence_length(source)
    dx, dy = travel(source, opts, frames)

    # Which bone carries the travel, in its own parent's space.
    locals_ = {b: [unreal.AnimPoseExtensions.get_bone_pose(unreal.AnimPoseExtensions.get_anim_pose_at_frame(source, f, opts), b, L)
                   for f in range(frames)] for b in BONES}
    carrier = max(BONES, key=lambda b: abs(locals_[b][-1].translation.x - locals_[b][0].translation.x)
                  + abs(locals_[b][-1].translation.y - locals_[b][0].translation.y)
                  + abs(locals_[b][-1].translation.z - locals_[b][0].translation.z))
    t0, t1 = locals_[carrier][0].translation, locals_[carrier][-1].translation
    drift = (t1.x - t0.x, t1.y - t0.y, t1.z - t0.z)
    # Only the floor-plane part: in the carrier's parent space that is whatever is not up. The
    # skeleton's up is component +Z for root; for pelvis (under an identity root) the same.
    pos, rots, scl = [], [], []
    for f, t in enumerate(locals_[carrier]):
        k = f / float(frames - 1)
        p = t.translation
        pos.append(unreal.Vector(p.x - drift[0] * k, p.y - drift[1] * k, p.z))
        rots.append(t.rotation)
        scl.append(t.scale3d)
    ctrl = out.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("Run on the spot"), True)
    try:
        ctrl.set_bone_track_keys(carrier, pos, rots, scl, True)
    finally:
        ctrl.close_bracket(True)
    eal.save_loaded_asset(out)
    ax, ay = travel(out, opts, frames)
    say("%s: %.2f s, travel %.0f cm (%.0f cm/s) carried by %s; after: %.1f cm" % (
        out_name, length, (dx * dx + dy * dy) ** 0.5, (dx * dx + dy * dy) ** 0.5 / length, carrier, (ax * ax + ay * ay) ** 0.5))
say("DONE")
