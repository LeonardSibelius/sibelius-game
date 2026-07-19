# Package the v0.8.1 SHIPPING Win64 RELEASE build (butler -> itch).
# FIRST SHIPPING-CONFIG SHIP (Walt, 2026-07-19): players saw debug text
# (VSM warnings, on-screen messages) in the 0.8.0 Development build.
# Shipping strips all on-screen debug output and the console. Note:
# GEngine->AddOnScreenDebugMessage is compiled out in Shipping — every
# player-facing message must live on a HUD/panel (they all do since the
# carousel comprehension pass).
# Also in 0.8.1: the slap fall-lane fix (Death_Back lays into open floor).
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v081.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.1",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
