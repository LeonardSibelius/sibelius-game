# Package the v0.7.7 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v076.ps1 with the 0.7.7 archive dir + log.
# 0.7.7 = the temple sauce fountain (Walt's ritual: E pours 4s + summons
# Gideons, C compiles +40, 90s quiet recharge), temple navmesh so the
# summoned Gideons approach, and Greystone as the player's mirror body.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v077.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.7.7",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
