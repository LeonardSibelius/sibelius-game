# Package the v0.9.4 SHIPPING Win64 RELEASE build (butler -> itch).
#
# Content of 0.9.4: the spine. Mrs. Hall speaks from the first minute, the AI agents hand
# over the powers instead of floating spheres, the memoir becomes a record the player
# keeps, and the finale reads forty years of it back. Plus the ambush fixes.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. MRS. HALL'S STORY TABLE MUST COOK.  <-- NEW, AND THE ONE THAT NEARLY SHIPPED BROKEN
#
#    Content/Data/MrsHallStory.csv has NO CSV fallback in a packaged build:
#    LoadTableAssetOrCsv's CSV branch is #if WITH_EDITOR, and no .csv reaches the package
#    at all (there is not one in the whole v0.9.1 build). Everything she says comes from
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Data\MrsHallStory.uasset
#
#    Without it she is SILENT for every player -- no opening ticket, no reaction to
#    Vision, no last word at the finale -- while working perfectly in PIE. GenerateSmokeTest
#    now asserts the asset exists, but the editor sees the whole disk; VERIFY IT COOKED.
#
#    AND: re-run Tools/Scripts/import_mrshall_story.py after ANY edit to the CSV. Editing
#    the CSV alone updates the editor and not the shipped game.
#
# 2. THE PANEL BODY FONT (from 0.9.0). USlotTechPanelWidget hard-references
#    /Engine/EngineFonts/DroidSansMono. Verify BOTH halves -- a UFont carries no glyphs:
#      Saved\Cooked\Windows\Engine\Content\EngineFonts\DroidSansMono.uasset
#      Saved\Cooked\Windows\Engine\Content\EngineFonts\Faces\DroidSansMono.ufont
#
# 3. RTP MUST READ 95.567% and SlotSmokeTest 34 PASS / 0 FAIL.
#
# 4. Still true: the 9 symbol sprites under Content\SlotFactory\SymbolSprites, and
#    ParagonGideon + the ten Morro _MH retargets, reach the pak ONLY through C++ hard
#    references. Both folders are gitignored. See docs/VENDOR_PACKS.md.
#
# 5. SAVE COMPATIBILITY: a v0.9.3 save should load. FProgressionState gained
#    SlotLifetimeMeters (0.9.0) and its Sauce default moved 0 -> 50 (0.9.3); both are
#    additive and old saves deserialize over them. The Hall.* one-time grants are new keys
#    and simply unclaimed on an old save, so she introduces herself to a returning player.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v094.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.4",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
