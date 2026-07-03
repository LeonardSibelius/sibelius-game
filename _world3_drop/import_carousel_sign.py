# import_carousel_sign.py — import the Carousel-of-Fates door sign (June 13, 2026).
# Clone of import_sauce_sign.py. Run NATIVELY via the editor Cmd box (NOT the UnrealClaude bridge):
#     py "C:/Users/wpark/Claude/import_carousel_sign.py"
# (Cmd-box mode, not Python mode.)
#
# IMPORTANT DIFFERENCE from the sauce import: this script IMPORTS ONLY — it does NOT auto-assign to every
# AHiddenDoor in the level (that would clobber the office obelisk's Sauce sign). Assign T_sign_carousel to the
# ATTIC HiddenDoor's SignTexture BY HAND on the placed instance (cook-safe, PK16).

import unreal

# Source PNG must already be robocopied into the game repo's Tools/Art.
SRC  = r"C:/Users/wpark/projects/sibelius-game/Tools/Art/T_sign_carousel.png"
DEST = "/Game/Signs"
NAME = "T_sign_carousel"

task = unreal.AssetImportTask()
task.filename         = SRC
task.destination_path = DEST
task.destination_name = NAME
task.automated        = True
task.replace_existing = True
task.save             = True

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

asset_path = f"{DEST}/{NAME}"
tex = unreal.EditorAssetLibrary.load_asset(asset_path)
if tex:
    # UI/sign texture group (matches the sauce sign treatment).
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"[carousel] Imported {asset_path}")
else:
    unreal.log_error(f"[carousel] Import FAILED — check SRC path: {SRC}")
