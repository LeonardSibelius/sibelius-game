# Package the v0.8.5 SHIPPING Win64 RELEASE build (butler -> itch).
# Content of 0.8.5: two more dancers in L_Office_v02 - Nyra Solmere and Isla
# Rowan - joining Kaia, plus the PlayerStart turned to face the room instead
# of the computer desk.
#
# Cook note: L_Office_v02 is already in MapsToCook and hard-references both new
# BPs -> their meshes, so they cook by reference (no DirectoriesToAlwaysCook
# entry needed). Same path Aisling/Elise/Kaia took in 0.8.2-0.8.4.
#
# Watch item: two more MetaHumans is roughly +250 MB of source. Compare the
# archive size against 0.8.4's 5.41 GB - if it did not move, they did not cook.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v085.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.5",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
