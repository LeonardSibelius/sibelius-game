# import_wayhome_sign.py — SIB-47 return door: import the "THE WAY HOME" sign art as a
# Texture2D at /Game/Signs/T_Sign_TheWayHome, the same way build_kitchen_loop.py imports the
# kitchen door's "Many Worlds" sign. AReturnDoor loads this by path (runtime-spawned door), and
# /Game/Signs is already in DirectoriesToAlwaysCook so it ships. Idempotent; run editor CLOSED:
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript -script="...import_wayhome_sign.py"
import unreal

SIGN_SRC = "C:/Users/wpark/projects/walt-cowork-memory/sibelius-art/signs/T_Sign_TheWayHome.png"
SIGN_ASSET = "/Game/Signs/T_Sign_TheWayHome"   # "THE WAY HOME", 1024x640 (1.6:1), RGBA
TAG = "###WAYHOME-SIGN###"

if unreal.EditorAssetLibrary.does_asset_exist(SIGN_ASSET):
    unreal.log("%s already present: %s" % (TAG, SIGN_ASSET))
else:
    task = unreal.AssetImportTask()
    task.filename = SIGN_SRC
    task.destination_path = "/Game/Signs"
    task.destination_name = "T_Sign_TheWayHome"
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    ok = unreal.EditorAssetLibrary.does_asset_exist(SIGN_ASSET)
    unreal.log("%s import %s: %s" % (TAG, "OK" if ok else "FAILED", SIGN_ASSET))
