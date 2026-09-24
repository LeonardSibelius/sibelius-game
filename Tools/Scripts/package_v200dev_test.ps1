# v2.0 DEV test package — the AI agent chat, the council and the Easter eggs, Development Win64.
#
# FOR WALT'S OWN PLAY, NOT A RELEASE. No version bump, no upload, nothing that touches Steam:
# v1.1.4 (BuildID 25471988) is still in Steam's review queue, and packaging to a local folder
# cannot reach it. The release steps are docs/THE_UNWRITTEN.md, "Shipping it — version 2.0".
#
# DEVELOPMENT, NOT SHIPPING, ON PURPOSE. sib.Chat.Enable and sib.Chat.Mode are 0 in Shipping
# (InteractorComponent.cpp, NpcChatSubsystem.cpp) and default on everywhere else, so this
# build talks to the live backend with no change to what ships. Built from feat/npc-chat.
#
# THIS IS PACKAGED ON THE FAILING PATRIOT P410, while the WD Green is still in transit
# (2026-09-23). The count of stornvme Event 129 resets during the run is printed at the end:
# a cook that crawls or fails with resets on the clock is the drive, not the project.
$root   = "C:\Users\wpark\projects\sibelius-game"
$outDir = "C:\Users\wpark\builds\sibelius-v2.0-dev"
$uat    = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log    = "$root\pkg-v200dev.log"
$start  = Get-Date

if (Test-Path $log) { Remove-Item $log -Force }

$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=$root\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=$outDir",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
$code = $LASTEXITCODE

$resets = @(Get-WinEvent -FilterHashtable @{LogName='System'; Id=129; StartTime=$start} -ErrorAction SilentlyContinue).Count
$minutes = [int]((Get-Date) - $start).TotalMinutes
Write-Output "EXITCODE=$code  minutes=$minutes  stornvme-129-resets=$resets"

if ($code -ne 0) {
  Write-Output "PACKAGE FAILED - see $log"
  exit $code
}

$exe = Get-ChildItem -Path $outDir -Recurse -Filter "SibeliusGame.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Write-Output "GAME: $($exe.FullName)" } else { Write-Output "WARNING: no SibeliusGame.exe under $outDir" }
