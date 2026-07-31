# Package the v0.8.4 SHIPPING Win64 RELEASE build (butler -> itch).
# Content of 0.8.4: Aisling and Elise, two MetaHuman figures flanking The
# Presence in the AI Temple. L_AI_Temple is already in MapsToCook, and the
# characters are hard-referenced by the level -> the BPs -> their meshes, so
# they cook by reference (no DirectoriesToAlwaysCook entry needed).
#
# Watch item for this cook: the MetaHuman content under /Game/MetaHumans and
# the outfit under /Game/Paid/Outfits are new to the shipped set. Verify they
# reached the pak before pushing (grep the cook log / check the archive size
# against 0.8.1) - PIE proves nothing about a package.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v084.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.4",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
