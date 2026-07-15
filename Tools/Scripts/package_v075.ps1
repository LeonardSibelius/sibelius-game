# Package the v0.7.5 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v074.ps1 with the 0.7.5 archive dir + log.
# 0.7.5 = the Death_Back cook fix (hard CDO ref - the soft path shipped
# in no pak, so itch builds froze instead of collapsing) + the Refuser
# trickle (office keeps supplying slap targets after the corkboard
# waves; any level with a spawner joins in once the alarm has fired).
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v075.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.7.5",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
