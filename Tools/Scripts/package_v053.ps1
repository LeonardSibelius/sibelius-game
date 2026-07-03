# Package the v0.5.3 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v052.ps1 with the 0.5.3 archive dir + log.
# 0.5.3 = FIX: world actually re-rolls per visit. The random seed range was
# 0..2e9, whose low bits quantized to 0, so WorldSeed % 4 (the row-rotation
# offset) was always 0 -> the 4 biome looks were always dealt to the same 4
# regions. Shrunk the range to 0..100000 so low bits vary and % 4 distributes.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v053.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.5.3",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
