# Package the v0.7.0 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v060.ps1 with the 0.7.0 archive dir + log.
# 0.7.0 = the road-battle update: stretch-proof rigid-knockback slap
# (Paragon cloth/ragdoll fix), Shinbi companions, invoker-bubble navmesh,
# Gideon-vs-Shinbi battles on the road in all 8 forests.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v070.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.7.0",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
