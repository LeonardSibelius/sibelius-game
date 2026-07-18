# Package the v0.7.8 Development Win64 RELEASE build (butler -> itch).
# Mirror of package_v077.ps1 with the 0.7.8 archive dir + log.
# 0.7.8 = the big one: the forest cut (~3.5 GB download, was ~7), the memoir
# voice (six shrine messages + two placards), shrine glow + slot TRIALS with
# spinning reels / win presentation / procedural sound, Gideon crowds on
# trial open, the temple fountain, and the wild-refactor menagerie fixes.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v078.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.7.8",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
