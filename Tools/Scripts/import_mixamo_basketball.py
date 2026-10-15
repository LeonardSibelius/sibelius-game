# import_mixamo_basketball.py - the three-point contest's bodies: Mixamo's Dribble and Throw In.
#
# Walt, 2026-10-15: Mixamo has one basketball animation ("Dribble", Dribbling Basketball With Right
# Arm) and no shot; Fab's one basketball pack was "too much money". So the shot is Mixamo's "Throw
# In" (Soccer Sideline Throw In): both hands up behind the head and over, which reads as an
# old-fashioned two-handed set shot. Downloaded FBX, Without Skin, 30 fps, to C:\mixamo\basketball
# and copied to SourceArt/Mixamo/Basketball (gitignored like the dances; its own folder so
# import_mixamo_dances.py, which reads SourceArt/Mixamo's top level, never takes them for dances).
#
# For each, skipped when its asset already exists (import_mixamo_driving.py's steps):
#   1. the FBX -> /Game/Mixamo/Basketball/Anim_<Name>, on Y Bot's skeleton.
#   2. retargeted with the dances' RTG_Mixamo_to_MetaHuman -> Anim_<Name>_MH, for the agents.
#   3. and with the kart's RTG_Mixamo_to_Manny -> Anim_<Name>_Manny, for Leonard.
# Then it MEASURES the shot on both bodies - its length, where the hands are, and the frame they
# let go - which is what ABasketballCourt needs to put the ball in her hands and release it.
#
# Run INSIDE the editor (UnrealClaude), PIE stopped, after import_mixamo_dances.py and
# import_mixamo_driving.py (Y Bot, both retargeters). Saves what it makes.

import math
import os
import unreal

TAG = "###HOOPANIMS###"
SRC_DIR = "C:/Users/wpark/projects/sibelius-game/SourceArt/Mixamo/Basketball"
DIR = "/Game/Mixamo/Basketball"
# The Throw In was measured first and runs 3 m (a running throw-in: its hips travel 295 cm), so
# Walt fetched Mixamo's standing throws too, and the reactions for after a round.
CLIPS = [("Dribble.fbx", "Anim_Dribble"), ("Throw In.fbx", "Anim_ThrowIn"),
         ("Throw.fbx", "Anim_Throw"), ("Throwing.fbx", "Anim_Throwing"), ("Goalie Throw.fbx", "Anim_GoalieThrow"),
         ("Cheering.fbx", "Anim_Cheering"), ("Disappointed.fbx", "Anim_Disappointed"),
         # Between racks (Walt: "can the players please just run from station to station?"):
         # Mixamo "Running", In Place. Skipped until it is downloaded.
         ("Running.fbx", "Anim_Running")]
OPTIONAL = {"Running.fbx"}
SHOTS = ["Anim_ThrowIn", "Anim_Throw", "Anim_Throwing", "Anim_GoalieThrow"]
DETAIL = ["Anim_Throw"]   # the one chosen (2026-10-15): it stands still and lets go highest
YBOT = "/Game/Mixamo/Character/SK_YBot.SK_YBot"
RTG_MH = "/Game/Mixamo/Retargeting/RTG_Mixamo_to_MetaHuman.RTG_Mixamo_to_MetaHuman"
RTG_MANNY = "/Game/Mixamo/Retargeting/RTG_Mixamo_to_Manny.RTG_Mixamo_to_Manny"
MH_MESH = "/Game/MetaHumans/MHC_Aisling/Body/SKM_MHC_Aisling_BodyMesh.SKM_MHC_Aisling_BodyMesh"
MANNY_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
W = unreal.AnimPoseSpaces.WORLD


def say(text):
    print("%s %s" % (TAG, text))


def anim(name):
    path = "%s/%s.%s" % (DIR, name, name)
    a = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    return a if isinstance(a, unreal.AnimSequence) else None


def retarget(source, target_mesh, rtg, suffix):
    name = source.get_name() + suffix
    if not anim(name):
        stray = "/Game/%s" % name   # the batch retarget writes to /Game's root (import_mixamo_dances.py)
        if not eal.does_asset_exist(stray):
            unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                [eal.find_asset_data(source.get_path_name())], ybot, target_mesh, rtg, "", "", "", suffix, False)
        if eal.does_asset_exist(stray):
            eal.rename_asset(stray, "%s/%s" % (DIR, name))
    eal.save_directory(DIR, only_if_is_dirty=True, recursive=True)
    out = anim(name)
    say("  %s: %s" % (name, "%.2f s on %s" % (unreal.AnimationLibrary.get_sequence_length(out),
                                                out.get_editor_property("skeleton").get_name()) if out else "MISSING"))
    return out


ybot, rtg_mh, rtg_manny, mh_mesh, manny_mesh = (unreal.load_asset(p) for p in (YBOT, RTG_MH, RTG_MANNY, MH_MESH, MANNY_MESH))
missing = [p for p, a in ((YBOT, ybot), (RTG_MH, rtg_mh), (RTG_MANNY, rtg_manny), (MH_MESH, mh_mesh), (MANNY_MESH, manny_mesh)) if not a]
if missing:
    raise RuntimeError("run import_mixamo_dances.py and import_mixamo_driving.py first: missing %s" % missing)

made = {}
for fbx, name in CLIPS:
    src = os.path.join(SRC_DIR, fbx)
    if not anim(name):
        if not os.path.exists(src):
            if fbx in OPTIONAL:
                say("SKIPPED %s: not downloaded yet" % fbx)
                continue
            raise RuntimeError("no %s" % src)
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("create_physics_asset", False)
        ui.set_editor_property("import_mesh", False)
        ui.set_editor_property("import_animations", True)
        ui.set_editor_property("skeleton", ybot.get_editor_property("skeleton"))
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", src)
        task.set_editor_property("destination_path", DIR)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
    source = anim(name)
    if not source:
        raise RuntimeError("the import made no %s/%s" % (DIR, name))
    say("%s: %.2f s, %d frames" % (source.get_path_name(), unreal.AnimationLibrary.get_sequence_length(source),
                                   unreal.AnimationLibrary.get_num_frames(source)))
    made[name + "_MH"] = retarget(source, mh_mesh, rtg_mh, "_MH")
    made[name + "_Manny"] = retarget(source, manny_mesh, rtg_manny, "_Manny")


# --- the measure: where the hands are, and when they let go ------------------------------------
# Component frame of a MetaHuman and of Manny: +Y forward, +X to her right (bake_kart_driving.py),
# +Z up from her feet. "Release" is the frame after the hands' highest point where they move
# forward fastest - the ball leaves on the way over, not at the top.
def vec(t):
    p = t.translation
    return (p.x, p.y, p.z)


def flat(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def measure(clip, mesh, label, detail):
    if not clip:
        return
    opts = unreal.AnimPoseEvaluationOptions()
    opts.set_editor_property("optional_skeletal_mesh", mesh)
    frames = unreal.AnimationLibrary.get_num_frames(clip) + 1
    length = unreal.AnimationLibrary.get_sequence_length(clip)
    rows = []
    for f in range(frames):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(clip, f, opts)
        at = {b: vec(unreal.AnimPoseExtensions.get_bone_pose(pose, b, W))
              for b in ("hand_l", "hand_r", "head", "pelvis", "foot_l", "foot_r")}
        rows.append((f, at))
    # How far she goes: the hips' and each foot's farthest from where they started, on the floor.
    hips = max(flat(r[1]["pelvis"], rows[0][1]["pelvis"]) for r in rows)
    feet = max(max(flat(r[1][k], rows[0][1][k]) for r in rows) for k in ("foot_l", "foot_r"))
    # Which hand throws: the one that goes highest. Its top, and where it lets go - the frame after
    # the top where it moves forward fastest, relative to her hips (her run-up is not the throw).
    hand = max(("hand_l", "hand_r"), key=lambda k: max(r[1][k][2] for r in rows))
    top = max(range(frames), key=lambda i: rows[i][1][hand][2])
    def fwd(i):
        return (rows[i][1][hand][1] - rows[i][1]["pelvis"][1]) - (rows[i - 1][1][hand][1] - rows[i - 1][1]["pelvis"][1])
    release = max(range(max(top, 1), frames), key=fwd) if top < frames - 1 else top
    both = abs(rows[top][1]["hand_l"][2] - rows[top][1]["hand_r"][2]) < 15.0
    say("%s %-22s %.2f s | hips travel %4.0f cm, feet %4.0f cm | %s highest %5.1f cm at %.2f s; lets go at %.2f s, %5.1f cm up, %5.1f cm ahead of her hips%s" % (
        label, clip.get_name(), length, hips, feet, "both hands" if both else hand, rows[top][1][hand][2], top / 30.0,
        release / 30.0, rows[release][1][hand][2], rows[release][1][hand][1] - rows[release][1]["pelvis"][1],
        "" if not detail else ":"))
    if detail:
        for f, at in rows[::3]:
            say("  f%3d %s (%6.1f, %6.1f, %6.1f) | pelvis (%6.1f, %6.1f, %6.1f) | feet %5.1f %5.1f" % (
                f, hand, at[hand][0], at[hand][1], at[hand][2], at["pelvis"][0], at["pelvis"][1], at["pelvis"][2],
                flat(at["foot_l"], rows[0][1]["foot_l"]), flat(at["foot_r"], rows[0][1]["foot_r"])))


for name in SHOTS + ["Anim_Dribble", "Anim_Cheering", "Anim_Disappointed"]:
    measure(made.get(name + "_MH"), mh_mesh, "agent", name in DETAIL)
for name in SHOTS:
    measure(made.get(name + "_Manny"), manny_mesh, "Manny", name in DETAIL)
say("DONE")
