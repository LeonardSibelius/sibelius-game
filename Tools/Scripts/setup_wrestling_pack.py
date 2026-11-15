# setup_wrestling_pack.py - let the wrestling pack's clips play on the agents. Run once after
# installing (or re-installing) "Wrestler Finishers Volume 2" from Fab. Editor CLOSED:
#
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript -script=".../setup_wrestling_pack.py"
#
# The pack animates the UE4 mannequin. The agents' bodies are on metahuman_base_skel. MorroMotion's
# mannequin skeleton lists metahuman_base_skel as COMPATIBLE, which is why the gestures play straight
# on the agents; this does the same for the pack's own skeleton, with MorroMotion's retarget-mode
# setting copied across, so the wrestling clips play the same way - root motion and hips included.
# (A retargeted copy through RTG_UE4_to_MetaHuman came out with the hips frozen: 2026-11-15.)
#
# The pack is git-ignored vendor content; this edit lives only on this machine, hence the script.

import json

import unreal

PACK_SKEL = "/Game/Wrestler_Finishers_Volume2/Demo/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton"
MORRO_SKEL = "/Game/MorroMotion/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton"
MH_SKEL = "/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/setup_wrestling_pack.json"

eal = unreal.EditorAssetLibrary
r = {}
try:
    skel = eal.load_asset(PACK_SKEL)
    morro = eal.load_asset(MORRO_SKEL)
    mh = eal.load_asset(MH_SKEL)
    r["loaded"] = [bool(skel), bool(morro), bool(mh)]
    r["morro_compatible"] = [str(x) for x in morro.get_editor_property("compatible_skeletons")]
    r["morro_use_modes"] = bool(morro.get_editor_property("use_retarget_modes_from_compatible_skeleton"))
    r["before"] = [str(x) for x in skel.get_editor_property("compatible_skeletons")]
    if not any("metahuman_base_skel" in x for x in r["before"]):
        for name in ("add_compatible_skeleton", "add_compatible_skeleton_soft"):
            if hasattr(skel, name):
                getattr(skel, name)(mh)
                r["added_with"] = name
                break
    skel.set_editor_property("use_retarget_modes_from_compatible_skeleton", r["morro_use_modes"])
    r["after"] = [str(x) for x in skel.get_editor_property("compatible_skeletons")]
    r["saved"] = bool(eal.save_asset(PACK_SKEL, only_if_is_dirty=False))
except Exception as e:
    import traceback
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=1)
unreal.log("###SETUP WRESTLING PACK### %s" % json.dumps(r))
