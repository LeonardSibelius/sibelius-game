"""Import only Nyra's protein invitation. Editor closed; use -AllowCommandletAudio."""
import unreal

SOURCE = "C:/Users/wpark/projects/sibelius-game/Tools/Audio/dancer_protein_nyra.mp3"
DEST = "/Game/Audio/Dancers"
NAME = "dancer_protein_nyra"
task = unreal.AssetImportTask()
task.filename = SOURCE
task.destination_path = DEST
task.destination_name = NAME
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.EditorAssetLibrary.load_asset(DEST + "/" + NAME)
assert isinstance(sound, unreal.SoundWave), "Protein invitation did not import as a SoundWave"
sound.set_editor_property("looping", False)
duration = sound.get_editor_property("duration")
assert 1.0 < duration < 60.0, "Missing or invalid voice duration: " + str(duration)
assert unreal.EditorAssetLibrary.save_loaded_asset(sound)
assert not sound.get_editor_property("looping")
print("PROTEIN_VOICE_OK " + sound.get_path_name() + " duration=" + str(duration))
