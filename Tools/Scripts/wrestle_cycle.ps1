# Build, then run the lab: cycle.ps1 <tag> <seconds> <pair> <every>
param([string]$Tag = "run", [int]$Seconds = 80, [int]$Pair = 0, [string]$Every = "0.25")
$proj = "C:\Users\wpark\projects\sibelius-game"
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" SibeliusGameEditor Win64 Development "-Project=$proj\SibeliusGame.uproject" -WaitMutex *> "$proj\build-wrestle.log"
if ($LASTEXITCODE -ne 0) {
    "BUILD FAILED"
    Select-String -Path "$proj\build-wrestle.log" -Pattern "error |warning C" | Select-Object -First 20 | ForEach-Object { $_.Line }
    exit 1
}
"build ok"
& "$PSScriptRoot\wrestle_lab.ps1" -Tag $Tag -Seconds $Seconds -Pair $Pair -Every $Every
