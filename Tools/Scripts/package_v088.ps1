# Package the v0.8.8 SHIPPING Win64 RELEASE build (butler -> itch).
# Content of 0.8.8: the dancing girls are AI Agents (E to talk, F to reshuffle her
# dance), the dancer aim assist, the bigger outlined reticle, and F renamed "Fight".
#
# *** THE CHECK THAT MATTERS FOR THIS RELEASE ***
# The ten Morro dances are gitignored TWICE OVER:
#     .gitignore:119  Content/Characters/Retargeting/*_MH.uasset   <- the _MH retargets
#     .gitignore:114  Content/MorroMotion/                          <- the raw pack
# and neither folder is in MapsToCook or DirectoriesToAlwaysCook. They reach the pak
# ONLY through UDancerAgentComponent's CDO hard references (ConstructorHelpers::
# FObjectFinder over DancePaths). That is a real reference so the cooker should follow
# it - but VERIFY, because nothing fails if it does not:
#
#   Saved\Cooked\Windows\SibeliusGame\Content\Characters\Retargeting\
#       Anim_High_Rhythm_Dance_13_MH.*      (10 files expected, *_MH only)
#
# A missing dance does not fail the build, does not fail the cook, and logs nothing at
# startup. F would simply stop changing the dance in the shipped game while working
# perfectly in PIE. Same shape as the v0.7.4 soft-ref miss and the ParagonGideon
# montage check in package_v087.ps1. See docs/VENDOR_PACKS.md.
#
# Also still true from 0.8.7: ParagonGideon reaches the pak only via FObjectFinder
# (SlapComponent Death_Back + RefuserController Primary_Attack_A_Medium_Montage).
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v088.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.8",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
