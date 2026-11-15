# retarget_wrestling.py - put the wrestling pack's moves on the agents.
#
# *** RUN FROM THE OPEN EDITOR (Output Log, Cmd box) ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/retarget_wrestling.py"
#
# Safe to re-run: skips anything already retargeted.
#
# Walt bought RamsterZ's "Wrestler Finishers Volume 2" (Fab) on 2026-11-15 after physics
# wrestling (2.7.0) read as convulsions and fondling. That pack is hand-animated PAIRED
# moves: both wrestlers play their half from the same spot, so grips land where they should.
# The pack is on the UE4 mannequin; all five agents share metahuman_base_skel, so one batch
# through RTG_UE4_to_MetaHuman (the dance set's retargeter) serves all of them. The API
# notes are in retarget_greystone_combat.py.
#
# WHAT IS TAKEN, AND WHAT IS NOT (Walt: no hitting):
#   - paired grappling: test of strength, headlock, back control, Boston crab, and four throws
#     (hip throw, low scoop slam, underarm flip toss, headlock flip-over)
#   - the pin with a referee's count, kick-outs, lying on the mat and getting up
#   - taunts, crowd work, idles
#   NOT the pack's strikes, choke hold, DDT, stunner, power bomb or spear.
#
# The output is vendor motion on our skeleton, so it goes under Characters/Retargeting/Anims/
# (git-ignored; the repo is public). The match loads it by path, so the folder must be in
# DirectoriesToAlwaysCook - the lesson of 2.7.1.
#
# It also measures each move (length, where the root and pelvis start and end) into
# Saved/retarget_wrestling.json, so the match code places the wrestlers from numbers.

import json
import traceback

import unreal

RTG = "/Game/Characters/Retargeting/RTG_UE4_to_MetaHuman"
PACK = "/Game/Wrestler_Finishers_Volume2"
SOURCE_MESH = PACK + "/Demo/Mannequin/Character/Mesh/SK_Mannequin"
TARGET_MESH = "/Game/MetaHumans/MHC_Aisling/Body/SKM_MHC_Aisling_BodyMesh"
DEST_DIR = "/Game/Characters/Retargeting/Anims/Wrestling"
SUFFIX = "_MH"

PAIRED = [
    "TestOfStrength_Start", "TestOfStrength_Loop", "TestOfStrength_End",
    "HeadLock_Start", "HeadLock_Loop", "HeadLock_End", "HeadLockFlipOver",
    "BackControl_Start", "BackControl_Loop", "BackControl_End", "BackControl_DragDown",
    "BostonCrab_Start", "BostonCrab_Loop", "BostonCrab_End",
    "HipThrow", "LowScoopSlam", "Underarm_FlipToss",
]

WANTED = []   # (folder, name)
for move in PAIRED:
    for side in ("Att", "Vic"):
        WANTED.append(("Wrestling_Paired", "Wrestling_Paired_%s_%s" % (move, side)))
for part in ("Start", "Loop01", "Resist", "Failure", "End"):
    for side in ("Attacker", "Victim"):
        WANTED.append(("Wrestling_Pin", "Wrestling_Pin_%s_%s" % (part, side)))
for part in ("Start", "Loop", "Tap01", "Tap02", "End"):
    WANTED.append(("Wrestling_Pin", "Wrestling_Pin_Referee_%s" % part))
for name in ("Idle", "GetUp01", "GetUp02"):
    WANTED.append(("Wrestling_LayOnBack", "Wrestling_LayOnBack_" + name))
for name in ("Idle", "GetUp01"):
    WANTED.append(("Wrestling_LayOnStomach", "Wrestling_LayOnStomach_" + name))
for n in range(1, 6):
    WANTED.append(("Wrestling_Taunt", "Wrestling_Taunt_%02d" % n))
for n in (1, 2):
    WANTED.append(("Wrestling_Crowdwork", "Wrestling_Crowdwork_%02d" % n))
for n in (1, 2):
    WANTED.append(("Wrestling_Idle", "Wrestling_Idle%02d" % n))

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/retarget_wrestling.json"

eal = unreal.EditorAssetLibrary
ar = unreal.AssetRegistryHelpers.get_asset_registry()
r = {"moved": [], "skipped": [], "missing": [], "notes": [], "measure": {}}


def vec(t):
    v = t.translation
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1), round(t.rotation.rotator().yaw, 1)]


def measure(path, key):
    """Root and pelvis (local) at the first and last frame, and the length."""
    anim = eal.load_asset(path)
    if not anim:
        return
    length = anim.get_editor_property("sequence_length") if hasattr(anim, "get_editor_property") else 0.0
    try:
        length = unreal.AnimationLibrary.get_sequence_length(anim)
    except Exception:
        pass
    m = {"len": round(length, 2)}
    try:
        for label, t in (("t0", 0.0), ("t1", max(0.0, length - 0.001))):
            poses = unreal.AnimationLibrary.get_bone_poses_for_time(anim, ["root", "pelvis"], t, False)
            m[label] = {"root": vec(poses[0]), "pelvis": vec(poses[1])}
    except Exception as e:
        m["err"] = str(e)[:200]
    r["measure"][key] = m


try:
    if not eal.does_directory_exist(DEST_DIR):
        eal.make_directory(DEST_DIR)

    rtg = eal.load_asset(RTG)
    src_mesh = eal.load_asset(SOURCE_MESH)
    tgt_mesh = eal.load_asset(TARGET_MESH)
    for label, obj, path in (("retargeter", rtg, RTG),
                             ("source mesh", src_mesh, SOURCE_MESH),
                             ("target mesh", tgt_mesh, TARGET_MESH)):
        if not obj:
            raise Exception("could not load %s: %s (pack missing?)" % (label, path))

    assets = []
    for folder, name in WANTED:
        src = "%s/Anim_Seq/%s/%s" % (PACK, folder, name)
        dest = "%s/%s%s" % (DEST_DIR, name, SUFFIX)
        if eal.does_asset_exist(dest):
            r["skipped"].append(name)
            continue
        if not eal.does_asset_exist(src):
            r["missing"].append(src)
            continue
        ad = ar.get_asset_by_object_path("%s.%s" % (src, name))
        if ad and ad.is_valid():
            assets.append(ad)
        else:
            r["notes"].append("no AssetData for %s" % src)

    if assets:
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            assets_to_retarget=assets,
            source_mesh=src_mesh,
            target_mesh=tgt_mesh,
            ik_retarget_asset=rtg,
            search="",
            replace="",
            prefix="",
            suffix=SUFFIX,
            include_referenced_assets=False,
            overwrite_existing_files=True,
        )
        made = list(made or [])
        r["notes"].append("retargeted %d of %d requested" % (len(made), len(assets)))
        for ad in made:
            born = str(ad.package_name)
            dest = "%s/%s" % (DEST_DIR, born.rsplit("/", 1)[-1])
            if born == dest or eal.rename_asset(born, dest):
                r["moved"].append(dest)
            else:
                r["notes"].append("FAILED to move %s -> %s" % (born, dest))
        eal.save_directory(DEST_DIR, only_if_is_dirty=False, recursive=True)

    # Measure the source moves (centimetres on the mannequin) and the retargeted ones (on her).
    for folder, name in WANTED:
        measure("%s/Anim_Seq/%s/%s" % (PACK, folder, name), name)
        measure("%s/%s%s" % (DEST_DIR, name, SUFFIX), name + SUFFIX)

    r["counts"] = {"wanted": len(WANTED), "moved": len(r["moved"]),
                   "already_present": len(r["skipped"]), "missing_from_pack": len(r["missing"])}
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###RETARGET WRESTLING### %s" % json.dumps(r.get("counts", r.get("traceback", ""))))
