# Package the v0.7.3 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v072.ps1 with the 0.7.3 archive dir + log.
# 0.7.3 = the "worth coming back" patch: RECORDS tab (lifetime stats),
# curio treetop beacons, temple blend wired (books actually fill the
# cauldron; one-time +100 bounty), Carousel HUD 1.8x + explainers,
# marble Carousel machine, library piano muted.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v073.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.7.3",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
