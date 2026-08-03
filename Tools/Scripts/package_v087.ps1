# Package the v0.8.7 SHIPPING Win64 RELEASE build (butler -> itch).
# Content of 0.8.7: Refusers face the player and swing (Gideon's own
# Primary_Attack montage), and the FinaleAltar names the key for each verb.
#
# *** THE CHECK THAT MATTERS FOR THIS RELEASE ***
# ParagonGideon is git-ignored AND absent from both MapsToCook and
# DirectoriesToAlwaysCook. It reaches the pak ONLY through C++ FObjectFinder
# hard references - SlapComponent (Death_Back) and now RefuserController
# (Primary_Attack_A_Medium_Montage). That is a real CDO reference so the cooker
# should follow it, but VERIFY IT LANDED before pushing:
#
#   Saved\Cooked\Windows\SibeliusGame\Content\ParagonGideon\Characters\Heroes\
#       Gideon\Animations\Primary_Attack_A_Medium_Montage.*
#
# A missing montage does not fail the build, does not fail the cook, and logs
# nothing at runtime - the Refusers simply stop swinging. See
# docs/VENDOR_PACKS.md.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v087.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.7",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
