# import_dancer_voice.py — the AI agents' spoken line (see docs/DANCER_VOICE.md).
#
# Takes whatever ElevenLabs handed Walt in Tools/Audio/ and imports it as
# /Game/Audio/Dancers/dancer_power, which is what UDancerAgentComponent looks
# for when the player presses E on a dancer with nothing left to give.
#
# UE 5.7 IMPORTS MP3 DIRECTLY (Engine SoundFactory: wav, aif, aiff, ogg, flac,
# opus, mp3). The old ai_intro note said to convert to WAV first — that is no
# longer necessary, and there is no ffmpeg on this machine anyway. Download the
# MP3 from ElevenLabs and point this at it.
#
# RUN HEADLESS, EDITOR CLOSED (preferred - one command, no clicking):
#
#   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
#     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" `
#     -run=pythonscript -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_dancer_voice.py" `
#     -AllowCommandletAudio -unattended -nopause -nosplash -stdout
#
# -AllowCommandletAudio IS LOAD-BEARING. Without it the import dies on
#
#     Ensure condition failed: Factory ... SoundWave.cpp:248
#     Decoder for AudioFormat 'BINKA' not found
#
# and leaves an EMPTY /Game/Audio/Dancers folder - it looks like it worked
# until you go looking for the asset. A python commandlet does not register
# the audio format modules, and every SoundWave import needs the BINKA
# encoder to build its compressed data. The switch registers them.
#
# OR RUN NATIVELY (editor Cmd box, never the bridge):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/import_dancer_voice.py"
#
# PER-AGENT TAKES. The component prefers dancer_power_<agent> over the shared
# dancer_power, so a later recording in Kaia's own voice is a file drop, not a
# code change. Name the source file dancer_power_kaia.mp3 and this imports it
# under that name automatically — every matching file in Tools/Audio is taken.
#
# IDEMPOTENT: re-running replaces the assets in place.

import os
import unreal

TAG = "###DANCERVOICE###"
PKG_DIR = "/Game/Audio/Dancers"
AUDIO_DIR = "C:/Users/wpark/projects/sibelius-game/Tools/Audio"

# Anything named dancer_power* in Tools/Audio gets imported under its own name.
# dancer_power.mp3        -> every agent
# dancer_power_kaia.mp3   -> Kaia only, overriding the shared one
PREFIX = "dancer_power"
EXTS = (".mp3", ".wav", ".flac", ".ogg", ".opus", ".aif", ".aiff")


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary

    if not os.path.isdir(AUDIO_DIR):
        unreal.log_error("%s no %s — nothing to import." % (TAG, AUDIO_DIR))
        return

    sources = []
    for name in sorted(os.listdir(AUDIO_DIR)):
        stem, ext = os.path.splitext(name)
        if stem.lower().startswith(PREFIX) and ext.lower() in EXTS:
            sources.append((stem.lower(), os.path.join(AUDIO_DIR, name)))

    if not sources:
        unreal.log_error(
            "%s found no %s*.mp3 (or .wav/.flac/.ogg) in %s. Download the take from "
            "ElevenLabs, save it as %s.mp3 there, and run this again. See "
            "docs/DANCER_VOICE.md." % (TAG, PREFIX, AUDIO_DIR, PREFIX))
        return

    if not eal.does_directory_exist(PKG_DIR):
        eal.make_directory(PKG_DIR)

    for asset_name, path in sources:
        full = "%s/%s" % (PKG_DIR, asset_name)
        if eal.does_asset_exist(full):
            eal.delete_asset(full)

        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = PKG_DIR
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True

        asset_tools.import_asset_tasks([task])

        if eal.does_asset_exist(full):
            sound = eal.load_asset(full)
            # A voice line must never loop: the close-up holds for the clip's
            # length, and a looping wave would report an absurd duration and
            # pin the camera on her face (the component clamps at 60s, but the
            # right fix is here).
            sound.set_editor_property("looping", False)
            eal.save_asset(full)
            unreal.log("%s imported %s  <-  %s" % (TAG, full, path))
        else:
            unreal.log_error(
                "%s import FAILED for %s — check the Output Log for the "
                "importer's complaint." % (TAG, path))

    unreal.log("%s done. DefaultGame.ini already cooks %s." % (TAG, PKG_DIR))


main()
