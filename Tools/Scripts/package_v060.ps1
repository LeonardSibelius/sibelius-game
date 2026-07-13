# Package the v0.6.0 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v052.ps1 with the 0.6.0 archive dir + log.
# 0.6.0 = the FUN_PLAN: earned powers, Sauce economy, cauldron shop, the
# Carousel of Fates in its hidden library tower (L_Carousel now cooked),
# the Ch7 Synthesis finale, M/J menus, HOW_TO_PLAY journal.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v060.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.6.0",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
