# retarget_greystone_combat.py — turn five bystanders into five fighters.
#
# *** RUN FROM THE OPEN EDITOR ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/retarget_greystone_combat.py"
#
# Safe to re-run: skips anything already retargeted.
#
# ===========================================================================
# THE API, WRITTEN DOWN, because two guesses at it failed before a probe got the truth:
#
#   IKRetargetBatchOperation.duplicate_and_retarget(
#       assets_to_retarget,          # Array[AssetData] - NOT loaded UAnimSequence objects
#       source_mesh, target_mesh, ik_retarget_asset,
#       search="", replace="", prefix="", suffix="",
#       include_referenced_assets=True, overwrite_existing_files=False
#   ) -> Array[AssetData]
#
# The two traps in that line, both of which cost a run:
#
#   1. It wants ASSETDATA. Passing loaded assets fails with "Cannot nativize 'AnimSequence'
#      as 'AssetData'". Build them from the asset registry: get_asset_by_object_path()
#      with the "/Game/Path/Name.Name" form.
#   2. The flag is include_referenced_assets, not remap_referenced_assets. That name is
#      from an older 5.x and is what the first attempt died on.
#
# It RETURNS the AssetData it created, so there is no need to guess where the new files
# landed - ask them. That matters here, see "where they land" below.
#
# ===========================================================================
# WHAT THIS DOES, AND WHY IT IS ONE RETARGET AND NOT FIVE.
#
# All five MetaHumans - Aisling, Elise, Isla, Kaia, Nyra - share ONE skeleton,
# metahuman_base_skel. Checked, not assumed. So an animation retargeted onto that skeleton
# plays on all five, and RTG_UE4_to_MetaHuman already exists in this project and has
# already been used for the dance set. This is one batch operation, not a project.
#
# Greystone's own skeleton uses Epic's standard bone names - pelvis, spine_01, clavicle_l,
# upperarm_l, thigh_l, ball_l - which is why IK_UE4_Mannequin's mappings resolve against
# him at all. That was checked too. A Paragon hero with a bespoke rig would not work here.
#
# ---------------------------------------------------------------------------
# WHY 23 ANIMATIONS AND NOT 174.
#
# Greystone ships 174. Most are MOBA traversal - TravelMode_*, Spin_Jog_*, Jog_Uphill_*,
# Turn_Left_90 - for a game where a hero walks a lane for twenty minutes. This game has a
# fight in a meadow. Retargeting all 174 would put ~150 assets into the cook that nothing
# will ever play, in a project that has already been through one size diet (v0.7.2, which
# deleted whole packs to get the download down).
#
# So: everything a fight needs to READ, and nothing a MOBA needs. Stand, move four ways,
# three swings, get hit from four sides, die, four abilities, and an arrival. If the fight
# later wants a dodge or a parry, add the name to the list below and re-run.
#
# LevelStart is here for a reason that is not combat. It is Greystone's hero-arrival
# animation, and BattleFormComponent.h is explicit that the grant of the body has to be
# FRAMED on screen or the whole thing is a costume change. That is the animation for it.
#
# include_referenced_assets is FALSE on purpose: the point of the list above is that it is
# exactly 23. Letting the batch drag in whatever these reference would quietly undo that.
#
# ---------------------------------------------------------------------------
# WHERE THEY LAND, AND WHY IT MATTERS MORE THAN IT SOUNDS.
#
# duplicate_and_retarget writes next to the SOURCE, which is inside /Game/ParagonGreystone
# - a 2.2 GB vendor pack that is GIT-IGNORED (docs/VENDOR_PACKS.md). Assets written there
# are invisible to the repo: they would work on this machine, vanish on a fresh clone, and
# the failure would be silent, because that is how missing vendor content always fails in
# this project.
#
# So every result is moved to /Game/Characters/Retargeting/Combat/, which is tracked. 23
# animations that survive a clone even though the 2.2 GB pack they came from does not.
#
# ONE THING THIS DOES NOT DO: nothing hard-references these yet, so they will not cook.
# That is the soft-reference trap this project has already been bitten by once - PIE will
# play them happily and the packaged build will not have them. They become real when an
# AnimBlueprint plays them: step 4 in docs/SWARM_PLAN.md, the fight logic.
#
# Montages are deliberately not in the list: a montage belongs to its source skeleton and
# does not transfer. Attack_PrimaryA_Montage has to be rebuilt on the MetaHuman side.

import json
import traceback

import unreal

RTG = "/Game/Characters/Retargeting/RTG_UE4_to_MetaHuman"
SOURCE_MESH = "/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone"
TARGET_MESH = "/Game/MetaHumans/MHC_Aisling/Body/SKM_MHC_Aisling_BodyMesh"

SRC_DIR = "/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations"
DEST_DIR = "/Game/Characters/Retargeting/Combat"

PREFIX = "GS_"      # says which hero it came from
SUFFIX = "_MH"      # matches the dance set's convention: retargeted onto MetaHuman

WANTED = [
    # standing and moving - without these he slides
    "Idle", "Jog_Fwd", "Jog_Bwd", "Jog_Left", "Jog_Right",
    # the three-swing combo. THE fight.
    "Attack_PrimaryA", "Attack_PrimaryB", "Attack_PrimaryC",
    # taking it, from four sides
    "HitReact_Front", "HitReact_Back", "HitReact_Left", "HitReact_Right",
    "Death",
    # the four powers, for when the fight logic wants somewhere to hang them
    "Ability_Q", "Ability_E", "Ability_R", "Ability_Ultimate",
    "Cast",
    # the arrival - see the header. This is the grant, not a combat move.
    "LevelStart",
    # the player wears this body, and the player can jump
    "Jump_Start", "Jump_Apex", "Jump_Fall", "Jump_Land",
]

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/retarget_greystone.json"

eal = unreal.EditorAssetLibrary
ar = unreal.AssetRegistryHelpers.get_asset_registry()
r = {"moved": [], "skipped": [], "missing": [], "notes": []}

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
            raise Exception("could not load %s: %s (vendor pack missing?)" % (label, path))

    # ---- gather only what is wanted, and only what is not already done -----
    assets = []
    for name in WANTED:
        src = "%s/%s" % (SRC_DIR, name)
        dest = "%s/%s%s%s" % (DEST_DIR, PREFIX, name, SUFFIX)
        if eal.does_asset_exist(dest):
            r["skipped"].append(name)
            continue
        if not eal.does_asset_exist(src):
            r["missing"].append(src)
            continue
        # AssetData, not the loaded object - see the API note at the top.
        ad = ar.get_asset_by_object_path("%s.%s" % (src, name))
        if ad and ad.is_valid():
            assets.append(ad)
        else:
            r["notes"].append("no AssetData for %s" % src)

    if not assets:
        r["notes"].append("nothing to do - %d already present in %s"
                          % (len(r["skipped"]), DEST_DIR))
    else:
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            assets_to_retarget=assets,
            source_mesh=src_mesh,
            target_mesh=tgt_mesh,
            ik_retarget_asset=rtg,
            search="",
            replace="",
            prefix=PREFIX,
            suffix=SUFFIX,
            include_referenced_assets=False,
            overwrite_existing_files=True,
        )
        made = list(made or [])
        r["notes"].append("retargeted %d of %d requested" % (len(made), len(assets)))

        # ---- out of the ignored pack, into the repo ------------------------
        # Move by the path the engine REPORTS, not one reconstructed from the name.
        # overwrite_existing_files can rename on collision, and a reconstructed guess
        # would then silently move nothing.
        for ad in made:
            born = str(ad.package_name)
            leaf = born.rsplit("/", 1)[-1]
            dest = "%s/%s" % (DEST_DIR, leaf)
            if born == dest:
                r["moved"].append(dest)          # already in the right place
            elif eal.rename_asset(born, dest):
                r["moved"].append(dest)
            else:
                r["notes"].append("FAILED to move %s -> %s" % (born, dest))

        eal.save_directory(DEST_DIR, only_if_is_dirty=False, recursive=True)
        r["notes"].append("saved %s" % DEST_DIR)

    r["counts"] = {
        "wanted": len(WANTED), "moved": len(r["moved"]),
        "already_present": len(r["skipped"]), "missing_from_pack": len(r["missing"]),
    }

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###RETARGET### %s" % json.dumps(r.get("counts", r)))
print(json.dumps(r, indent=2))
