# import_cinematic_audio.py — voice clips for CUTSCENES, not the dancer talk close-up.
#
# Imports Tools/Audio/*_intro.mp3 (and friends listed in CLIPS) into
# /Game/Audio/Cinematics/. These are the recordings a MetaHuman Animator
# performance is solved from, and that Sequencer plays under the shot.
#
# WHY A SEPARATE SCRIPT FROM import_dancer_voice.py. That one globs
# dancer_power* and drops the results in /Game/Audio/Dancers, which is a
# runtime lookup folder cooked via DirectoriesToAlwaysCook. Cinematic audio has
# a different job: it is consumed in the EDITOR (face solve + Sequencer) and
# ends up baked into a rendered MP4, so it does not need to reach the pak at
# all. Mixing the two would either cook audio nobody plays or, worse, tempt
# someone to name a cutscene clip dancer_power_something and have an agent
# recite a monologue when you press E.
#
# RUN HEADLESS, EDITOR CLOSED:
#
#   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
#     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" `
#     -run=pythonscript -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_cinematic_audio.py" `
#     -AllowCommandletAudio -unattended -nopause -nosplash -stdout
#
# -AllowCommandletAudio IS LOAD-BEARING — without it every SoundWave import
# dies on "Decoder for AudioFormat 'BINKA' not found" and leaves an EMPTY
# folder that looks exactly like success. See docs/DANCER_VOICE.md.
#
# IDEMPOTENT: re-running replaces the assets in place.

import os
import unreal

TAG = "###CINEAUDIO###"
PKG_DIR = "/Game/Audio/Cinematics"
AUDIO_DIR = "C:/Users/wpark/projects/sibelius-game/Tools/Audio"

# Source stem -> asset name. Add a line per cutscene clip.
CLIPS = {
    "kaia_intro": "kaia_intro",
}
EXTS = (".mp3", ".wav", ".flac", ".ogg", ".opus", ".aif", ".aiff")


def find_source(stem):
    for ext in EXTS:
        path = os.path.join(AUDIO_DIR, stem + ext)
        if os.path.isfile(path):
            return path
    return None


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary

    if not eal.does_directory_exist(PKG_DIR):
        eal.make_directory(PKG_DIR)

    for stem, asset_name in CLIPS.items():
        src = find_source(stem)
        if not src:
            unreal.log_error("%s no %s.* in %s — skipping." % (TAG, stem, AUDIO_DIR))
            continue

        full = "%s/%s" % (PKG_DIR, asset_name)
        if eal.does_asset_exist(full):
            eal.delete_asset(full)

        task = unreal.AssetImportTask()
        task.filename = src
        task.destination_path = PKG_DIR
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True

        asset_tools.import_asset_tasks([task])

        if eal.does_asset_exist(full):
            sound = eal.load_asset(full)
            # Never loop a spoken line: a looping wave reports an absurd
            # duration, which would pin a Sequencer shot open forever.
            sound.set_editor_property("looping", False)
            eal.save_asset(full)
            unreal.log("%s imported %s  <-  %s" % (TAG, full, src))
        else:
            unreal.log_error(
                "%s import FAILED for %s — check the Output Log. Did you pass "
                "-AllowCommandletAudio?" % (TAG, src))


main()
