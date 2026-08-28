# Package the v0.9.8.0 SHIPPING Win64 build — "The Refuser Army of Arrogant Architects".
#
# Content of 0.9.8.0: the game has an ending. Mrs. Hall names the Architects at the
# second power and sneers at the cathedral machine; the machine meters what it has PAID
# OUT toward 5,000; at 5,000 a door appears beside it that was never there; through it is
# a meadow where four hundred of her Refusers stand, the agents grant a body that can
# reach them, and the crowd pins and overrules anyone who stops swinging. Clear the field
# and the game says "AI has set you free."
#
# *** THE CHECK THAT MATTERS FOR THIS RELEASE ***
#
# GREYSTONE HAS TO BE IN THE PAK. UBattleFormComponent points AvatarMesh and
# AvatarAnimClass at him with SOFT pointers set in C++, and USlapComponent loads his
# Attack_Primary montages by string path at runtime. Neither is a package reference, so
# the cooker follows neither. Before this release DefaultGame.ini gained
#
#   +DirectoriesToAlwaysCook=(Path="/Game/ParagonGreystone/Characters/Heroes/Greystone")
#
# ...and without it a packaged build enters battle form with no hero and no sword, logs
# "[BattleForm] avatar mesh failed to load", and looks precisely like a bug. Same trap
# ParagonGideon avoids by being hard-referenced through FObjectFinder.
#
# This script VERIFIES that after the cook rather than trusting the config, because the
# cook reporting success is not the same as the asset being in the build - a distinction
# this project paid for repeatedly on 2026-08-27.
#
# ALSO NEW IN THE COOK: /Game/Cinematics/L_Meadow in MapsToCook. It was never there, so
# no packaged build has ever contained the battlefield at all.

$root    = "C:\Users\wpark\projects\sibelius-game"
$version = "0.9.8.0"
$outDir  = "C:\Users\wpark\builds\sibelius-v$version"
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-v0980.log"

if (Test-Path $log) { Remove-Item $log -Force }

$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=$root\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=$outDir",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
$code = $LASTEXITCODE
Write-Output "EXITCODE=$code"

if ($code -ne 0) {
  Write-Output "PACKAGE FAILED - see $log"
  exit $code
}

# ---- verify the things this release cannot ship without ----------------------
$cooked = "$root\Saved\Cooked\Windows\SibeliusGame\Content"
$checks = @{
  "Greystone mesh"     = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Meshes\Greystone.uasset"
  "Greystone AnimBP"   = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Greystone_AnimBlueprint.uasset"
  "Attack_PrimaryA"    = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Animations\Attack_PrimaryA_Montage.uasset"
  "the meadow"         = "$cooked\Cinematics\L_Meadow.umap"
  "Mrs. Hall's lines"  = "$cooked\Data\MrsHallStory.uasset"
}
$missing = 0
foreach ($k in $checks.Keys) {
  if (Test-Path $checks[$k]) { Write-Output "OK      $k" }
  else { Write-Output "MISSING $k  -> $($checks[$k])"; $missing++ }
}
if ($missing -gt 0) {
  Write-Output "COOK IS INCOMPLETE - $missing required asset(s) absent. Do NOT push."
  exit 1
}
Write-Output "COOK VERIFIED. Archive: $outDir"
