# fix_ism_usage.py - every material the AI city puts on an instanced mesh must say so.
#
# Walt, 2026-10-18, in the packaged 2.2.8: "the little galaxies inside the spheres are no longer
# made of stars, they are made of little dark pebbles" - and the gallery rail was the engine's
# grey checker. A material drawn on an InstancedStaticMeshComponent needs its "Used with
# Instanced Static Meshes" flag. The editor sets it quietly the first time it meets the use, so
# Play in the editor looks right; a packaged build has no editor to do that, and draws the
# engine's default material instead. M_GenesisGlow and M_GenesisGlass (made 2026-10-15) never had
# it; the lattice's materials set it from the start (make_genesis_assets.py).
#
# Sets the flag where it is missing and saves; touches nothing else. Safe to re-run.
#
#   UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
#       -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/fix_ism_usage.py"

import unreal

TAG = "###ISMUSAGE###"
# Everything AAICity, AUniverseForge and AUniverseExhibit wear on their instanced parts - the
# shells, rings, sky, rail, gallery lines, the universes' stars, the forge's grains and machines,
# the exhibits' stars - and the rest of /Game/Genesis's materials for good measure.
MATERIALS = [
    "/Game/Genesis/M_GenesisGlow",
    "/Game/Genesis/M_GenesisGlass",
    "/Game/Genesis/M_GenesisStar",
    "/Game/Genesis/M_GenesisThought",
    "/Game/Genesis/M_GenesisStrut",
    "/Game/Genesis/M_GenesisNode",
    "/Game/Go/M_GoSurface",
]

eal = unreal.EditorAssetLibrary
changed = 0
for path in MATERIALS:
    mat = unreal.load_asset(path)
    if not isinstance(mat, unreal.Material):
        unreal.log_warning("%s %s: not found or not a material" % (TAG, path))
        continue
    before = mat.get_editor_property("used_with_instanced_static_meshes")
    if not before:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
        eal.save_asset(path, only_if_is_dirty=False)
        changed += 1
    unreal.log("%s %s: was %s, now %s" % (TAG, path, before, mat.get_editor_property("used_with_instanced_static_meshes")))
unreal.log("%s DONE - %d material(s) changed" % (TAG, changed))
