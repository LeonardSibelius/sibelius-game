# prepare_hoard.py — give Refactor's treasure a body R can find again.
#
# The hoard is Dekogon's "Treasures VOL.1 - Ancient Treasures" (Content/AncientTreasures,
# gitignored; URefactorComponent's constructor names the meshes). As shipped, not one of its
# gems, coins or coin heaps has simple collision (measured 2026-10-03: 0 of 25). The R ray is
# a simple-collision trace, so it went straight through every treasure - Walt: "sometimes when
# i "R" the object vanishes and there is no treasure" - and a treasure R cannot hit can never
# be turned back. Slapping one could not launch it either: physics needs a simple body.
#
# So each gets one box, the shape of its bounds. Only meshes with NO simple collision are
# touched, so it is safe to re-run - and it must be re-run if the pack is ever added again
# (URefactorComponent::BeginPlay drops, and logs, any treasure still without a body).
#
# RUN with the editor open, PIE stopped (any level).

import unreal

TAG = "###HOARD###"
FOLDER = "/Game/AncientTreasures/Meshes"
# Exactly the names URefactorComponent's constructor uses (Walt chose the chalices, gold bar
# and chests, and dropped the grey SM_Coin_02a, the bangles and the coin heaps, 2026-10-03).
HOARD = ("SM_Gems_01a", "SM_Gems_01b", "SM_Gems_01c", "SM_Gems_01d",
         "SM_Coin_01a",
         "SM_Chalice_01a", "SM_Chalice_02b", "SM_GoldBar_01a", "SM_OrnateChest_02d", "SM_Chest_02c",
         "SM_ChestCoins_01a")

sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
done, kept = [], []
for name in HOARD:
    mesh = unreal.load_asset("%s/%s.%s" % (FOLDER, name, name))
    if not isinstance(mesh, unreal.StaticMesh):
        print("%s %s: NOT FOUND" % (TAG, name))
        continue
    if sms.get_simple_collision_count(mesh) > 0:
        kept.append(name)
        continue
    sms.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    if sms.get_simple_collision_count(mesh) > 0 and unreal.EditorAssetLibrary.save_loaded_asset(mesh, False):
        done.append(name)
    else:
        print("%s %s: could NOT be given a box" % (TAG, name))

print("%s boxed %d: %s" % (TAG, len(done), ", ".join(done)))
print("%s already had collision: %d" % (TAG, len(kept)))
