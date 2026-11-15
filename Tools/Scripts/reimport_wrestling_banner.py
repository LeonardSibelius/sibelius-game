# reimport_wrestling_banner.py - re-import the living-room wrestling banner's picture, nothing else.
# For the open editor (place_wrestling_banner.py would switch levels to hang it):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/reimport_wrestling_banner.py"
# Make the PNG first: python Tools/Scripts/make_wrestling_banner.py

import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
PNG = os.path.normpath(os.path.join(HERE, "..", "..", "SourceArt", "Wrestling", "T_Sign_Wrestling.png"))
ASSET = "/Game/Signs/T_Sign_Wrestling"

task = unreal.AssetImportTask()
task.set_editor_property("filename", PNG)
task.set_editor_property("destination_path", "/Game/Signs")
task.set_editor_property("destination_name", "T_Sign_Wrestling")
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex = unreal.load_asset(ASSET)
if tex:
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    unreal.EditorAssetLibrary.save_asset(ASSET)
unreal.log("###WRESTLEBANNER### re-imported: %s" % bool(tex))
