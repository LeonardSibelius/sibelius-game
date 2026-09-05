# make_grok_from_alien.py - Grok, duplicated out of the vendor pack into our own Maps.
#
# *** RUN FROM THE OPEN EDITOR ***
#   Cmd box:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_grok_from_alien.py"
#
# Output: /Game/Maps/L_Grok  (about 500 MB - the landscape lives inside the .umap)
#
# ===========================================================================
# WHY DUPLICATE INSTEAD OF JUST USING THE VENDOR MAP.
#
# Content/Elite_AlienPack_04/ is GITIGNORED - it is a 3.2 GB purchased pack and this
# repo is public. So anything authored into the vendor's own Alien_IV_01 would live in
# an ignored folder: never committed, gone on a clean checkout, invisible to any machine
# but this one. Walt would decorate Grok, and the repo would not contain Grok.
#
# Duplicating into /Game/Maps puts it beside L_City, L_Cafe and L_uFoods, where every
# other level this game ships already lives, and where git can see it.
#
# THIS IS THE SAME MOVE AS L_uFoods AND L_Cafe: take the vendor's dressed showcase and
# make it ours, rather than authoring into content we do not own. The ART_PIPELINE rule
# in the sibling project states it outright - duplicate-out-and-rename to reuse, never
# author into a vendor folder.
#
# ---------------------------------------------------------------------------
# WHY THIS IS SAFE TO RUN WITH A LEVEL OPEN.
#
# The standing rule is that editor Python must not load_levels or delete a .umap - both
# fatal the editor, and that lesson cost a session. duplicate_asset does NEITHER. It
# copies a package on disk and registers it; the open level is untouched. It is also
# idempotent: if /Game/Maps/L_Grok already exists this reports and stops rather than
# overwriting work.
#
# It is SLOW - half a gigabyte of landscape heightmap and weightmap data - so give it a
# minute and do not assume it has hung.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/make_grok_report.json"

SRC = "/Game/Elite_AlienPack_04/Maps/Alien_IV_01"
DST = "/Game/Maps/L_Grok"

r = {"src": SRC, "dst": DST}


def main():
    if not unreal.EditorAssetLibrary.does_asset_exist(SRC):
        r["error"] = "source map not found - is Elite_AlienPack_04 added to the project?"
        return

    # NEVER OVERWRITE. Once Nyra and her stage-4 line are placed in L_Grok, a second run
    # of this script would throw that away and hand back the vendor's bare landscape.
    if unreal.EditorAssetLibrary.does_asset_exist(DST):
        r["skipped"] = "L_Grok already exists - refusing to overwrite it"
        return

    r["duplicated"] = bool(unreal.EditorAssetLibrary.duplicate_asset(SRC, DST))
    if r["duplicated"]:
        unreal.EditorAssetLibrary.save_asset(DST)
        r["saved"] = True


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)

unreal.log("###GROK### %s" % json.dumps(r))
