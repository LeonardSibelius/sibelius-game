# Package the v0.8.0 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v078.ps1 with the 0.8.0 archive dir + log.
# 0.8.0 = ALPHA (Walt's call, 2026-07-18): the casino floor is real. Since
# v0.7.8: the Carousel comprehension pass (plain-words dialog panel,
# self-explaining shop), VIDEO POKER complete (9/6 model + gate, genuine
# 52-card deck, kitchen door + four-suits sign, HOW TO PLAY + the-house-
# suggests trainer), machine-aware E routing (proximity gates, no consumed
# keys), curio cabinet retired. 15 gates green pre-cook.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v080.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.0",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
