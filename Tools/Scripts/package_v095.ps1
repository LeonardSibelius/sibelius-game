# Package the v0.9.5 SHIPPING Win64 RELEASE build (butler -> itch).
#
# Content of 0.9.5: Mrs. Hall reacts to every power instead of only Vision; Aisling is the
# fourth AI agent and hands over Generate; the cathedral machine becomes an upright cabinet
# with a lit CELESTIAL FORTUNE marquee; the apse altar is restored.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. MRS. HALL'S STORY TABLE MUST COOK -- AND IT NOW HAS EIGHT ROWS, NOT THREE.
#
#    Content/Data/MrsHallStory.csv has NO CSV fallback in a packaged build:
#    LoadTableAssetOrCsv's CSV branch is #if WITH_EDITOR, and no .csv reaches the package.
#    Everything she says comes from
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Data\MrsHallStory.uasset
#
#    0.9.5 adds the five power beats to that table. They were re-imported with
#    Tools/Scripts/import_mrshall_story.py -- editing the CSV alone updates the EDITOR and
#    not the shipped game, so if that step were ever skipped she would answer Refactor,
#    Compile, Test-Drive, Deploy and Generate with SILENCE for every player while working
#    perfectly in PIE. GenerateSmokeTest asserts all eight Reasons; verify the asset cooked.
#
# 2. THE NEW MARQUEE MATERIALS MUST COOK.
#
#      Saved\Cooked\Windows\SibeliusGame\Content\SlotFactory\M_MarqueeField.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\SlotFactory\M_MarqueeText.uasset
#
#    These are referenced only from actors placed in L_Cathedral, which is a hard reference
#    and should be enough -- but the soft-reference lesson in docs/VENDOR_PACKS.md is that
#    PIE sees the whole disk and the pak does not. If M_MarqueeText is missing, the sign's
#    lettering falls back and the last object in the game reads wrong.
#
# 3. AISLING MUST REACH THE PAK. BP_MHC_Aisling is placed in L_Office_v02 (a hard
#    reference, so it cooks), but her DANCE comes from Content/Characters/Retargeting,
#    which is GITIGNORED and reaches the build only through UDancerAgentComponent's CDO
#    hard-references. Anim_Slow_Rhythm_Dance_14_MH is in that list. If it does not cook,
#    the scan never adopts her, BindToAgent gives up after 20 retries, and GENERATE IS
#    UNOBTAINABLE with the shrine already hidden -- the power simply cannot be got.
#
# 4. THE PANEL BODY FONT (from 0.9.0). USlotTechPanelWidget hard-references
#    /Engine/EngineFonts/DroidSansMono. Verify BOTH halves -- a UFont carries no glyphs:
#      Saved\Cooked\Windows\Engine\Content\EngineFonts\DroidSansMono.uasset
#      Saved\Cooked\Windows\Engine\Content\EngineFonts\Faces\DroidSansMono.ufont
#
#    NEW SIBLING RISK: the marquee's text uses RobotoDistanceField the same way. Its atlas
#    is what cuts the glyph shapes out; without it the sign renders as blank quads.
#
# 5. RTP MUST READ 95.567% and SlotSmokeTest 34 PASS / 0 FAIL.
#
# 6. Still true: the 9 symbol sprites under Content\SlotFactory\SymbolSprites, and
#    ParagonGideon + the ten Morro _MH retargets, reach the pak ONLY through C++ hard
#    references. Both folders are gitignored. See docs/VENDOR_PACKS.md.
#
# 7. SAVE COMPATIBILITY: a v0.9.4 save should load. Nothing was added to
#    FProgressionState this release. The new Hall.FirstUse.* keys for the five powers are
#    simply unclaimed on an old save, so a returning player gets her reactions from
#    whichever powers they use next -- which is the intended behaviour, not a bug.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v095.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.5",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
