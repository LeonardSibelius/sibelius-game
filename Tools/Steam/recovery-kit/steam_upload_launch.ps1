# Upload the STEAM LAUNCH build - CHAT OFF - app 5249380, depot 5249381.
#
# Written 2026-11-11 (Walt: launch on Steam now, with the chat switched off, while Valve's review
# of the chat goes on). A copy of steam_upload_chat.ps1 with its gate turned round:
#
#   - steam_upload_chat.ps1 sends the CHAT build and refuses without -AmendmentAccepted.
#   - THIS ONE sends a CHAT-OFF build, and refuses any build that is not one: the folder above
#     -ContentRoot must hold BUILD_INFO.txt saying chat=off, which only
#     Tools\Scripts\package_v210.ps1 -ChatOff writes. A chat build, or an older build with no
#     BUILD_INFO.txt, is refused - Steam's survey says nothing is generated at runtime, and
#     a chat-off build is what makes that true.
#
# It NEVER MODIFIES ITS OWN .vdf FILES: it writes a resolved copy into -BuildOutput and runs
# steamcmd on that, as the chat script does.
#
# Run it from the repo, naming the build:
#
#   powershell -ExecutionPolicy Bypass -File C:\Users\wpark\projects\sibelius-game\Tools\Steam\recovery-kit\steam_upload_launch.ps1 `
#       -SteamUser wparkman676 -SdkPath C:\steamworks_sdk `
#       -ContentRoot C:\Users\wpark\builds\sibelius-shipping-steam-20261112-v2.5.2\Windows -Preview
#
# 2.5.2 (2026-11-12) is the first update after the launch: Kaia's power is a gift, no slot trial.
# The launch build itself was sibelius-shipping-steam-20261111-v2.5.1 (BuildID 26597490).
#
# ---------------------------------------------------------------------------
# THIS SCRIPT NEVER SEES YOUR PASSWORD, AND IT MUST STAY THAT WAY.
#
# It passes your Steam ACCOUNT NAME to steamcmd and nothing else. steamcmd prompts for the
# password and the Steam Guard code itself - have your phone. Do not add a -Password
# parameter, do not put credentials in a .vdf, do not paste them onto a command line.
#
# ---------------------------------------------------------------------------
# PREVIEW FIRST. ALWAYS. -Preview walks the content, prints what it WOULD send, and uploads
# nothing. Then run the same line without -Preview.
#
# Uploading is NOT releasing. "setlive" is empty, so the build lands in Steamworks attached to
# no branch. The steps after it are printed at the end.
#
# ---------------------------------------------------------------------------
# THE SDK IS NOT IN THE REPO. Download steamworks_sdk.zip from partner.steamgames.com
# (partner login), unzip it anywhere, and point -SdkPath at the folder containing
# sdk\tools\ContentBuilder\builder\steamcmd.exe.

[CmdletBinding()]
param(
    # Your Steam ACCOUNT NAME. Never the password.
    [Parameter(Mandatory = $true)]
    [string] $SteamUser,

    # Folder containing sdk\tools\ContentBuilder\builder\steamcmd.exe
    [string] $SdkPath = 'C:\steamworks_sdk',

    # The packaged build's Windows folder. Default: the 2.5.2 chat-off build one folder up from
    # this kit (where it sits if it is copied to the stick beside steam-upload\).
    [string] $ContentRoot,

    # SteamPipe's chunk cache and logs, plus the resolved .vdf copies. Default: .\buildoutput
    [string] $BuildOutput,

    # Dry run: report what would upload, send nothing.
    [switch] $Preview
)

$ErrorActionPreference = 'Stop'

$Kit      = $PSScriptRoot
$AppVdf   = Join-Path $Kit 'app_build_5249380_launch.vdf'
$DepotVdf = Join-Path $Kit 'depot_build_5249381.vdf'
if (-not $ContentRoot) { $ContentRoot = Join-Path (Split-Path -Parent $Kit) 'sibelius-shipping-steam-20261112-v2.5.2\Windows' }
if (-not $BuildOutput) { $BuildOutput = Join-Path $Kit 'buildoutput' }

# ---------------------------------------------------------------------------
# THE GATE: only a chat-off build goes up this way.

if (-not (Test-Path $ContentRoot)) { throw "Content root missing: $ContentRoot" }
$InfoFile = Join-Path (Split-Path -Parent $ContentRoot) 'BUILD_INFO.txt'
if (-not (Test-Path $InfoFile)) {
    throw ("Refusing to upload: no BUILD_INFO.txt beside $ContentRoot. Only a build packaged with " +
           "package_v210.ps1 -ChatOff can go up as the launch build.")
}
$Info = Get-Content $InfoFile
if (-not ($Info -contains 'chat=off')) {
    throw ("Refusing to upload: $InfoFile does not say chat=off. This is a chat build - it goes up " +
           "with steam_upload_chat.ps1, and only after Valve accepts the chat.")
}

# ---------------------------------------------------------------------------
# PRE-FLIGHT - the same checks as the chat script.

if (-not (Test-Path $AppVdf))   { throw "No app build script at $AppVdf" }
if (-not (Test-Path $DepotVdf)) { throw "No depot script at $DepotVdf" }

$Exe = Join-Path $ContentRoot 'SibeliusGame.exe'
if (-not (Test-Path $Exe)) { throw "No SibeliusGame.exe at $ContentRoot - is this a staged build?" }

# A pak present but far too small means the cook dropped content: the upload would
# succeed and the game would be broken.
$Ucas = Join-Path $ContentRoot 'SibeliusGame\Content\Paks\SibeliusGame-Windows.ucas'
if (-not (Test-Path $Ucas)) { throw "No .ucas at $Ucas - this build did not cook." }
$UcasGB = (Get-Item $Ucas).Length / 1GB
if ($UcasGB -lt 6.5) {
    throw ("SibeliusGame-Windows.ucas is only {0:N2} GB. A full cook is over 7 GB - content is missing." -f $UcasGB)
}

$SteamCmd = Join-Path $SdkPath 'sdk\tools\ContentBuilder\builder\steamcmd.exe'
if (-not (Test-Path $SteamCmd)) {
    throw "No steamcmd.exe at $SteamCmd`nDownload steamworks_sdk.zip from partner.steamgames.com and pass -SdkPath."
}

if (-not (Test-Path $BuildOutput)) {
    New-Item -ItemType Directory -Path $BuildOutput -Force | Out-Null
}

# ---------------------------------------------------------------------------
# WRITE A RESOLVED COPY OF THE VDFs INTO BUILDOUTPUT, with this machine's paths and the
# preview flag. The kit's own .vdf files are never touched. .NET with BOM-less UTF-8.

function Set-VdfValue([string] $Text, [string] $Key, [string] $Value) {
    $safe = $Value.Replace('$', '$$')
    return [regex]::Replace($Text, ('(?m)^(\s*"' + $Key + '"\s*)"[^"]*"'), ('${1}"' + $safe + '"'))
}

$utf8NoBom = New-Object System.Text.UTF8Encoding $false
$vdf = [System.IO.File]::ReadAllText($AppVdf, $utf8NoBom)
$vdf = Set-VdfValue $vdf 'contentroot' $ContentRoot
$vdf = Set-VdfValue $vdf 'buildoutput' $BuildOutput
$vdf = Set-VdfValue $vdf 'preview'     $(if ($Preview) { '1' } else { '0' })
$ResolvedVdf = Join-Path $BuildOutput 'app_build_5249380_launch.resolved.vdf'
[System.IO.File]::WriteAllText($ResolvedVdf, $vdf, $utf8NoBom)
Copy-Item $DepotVdf (Join-Path $BuildOutput 'depot_build_5249381.vdf') -Force

$sizeGB = (Get-ChildItem $ContentRoot -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1GB
$pdb    = Get-ChildItem $ContentRoot -Recurse -Filter '*.pdb' -ErrorAction SilentlyContinue
$pdbGB  = 0
if ($pdb) { $pdbGB = ($pdb | Measure-Object -Property Length -Sum).Sum / 1GB }

Write-Host ''
Write-Host  '  app       5249380  Leonard Sibelius'
Write-Host  '  depot     5249381  Leonard Sibelius Content'
Write-Host ("  content   {0}" -f $ContentRoot)
Write-Host ("  on disk   {0:N2} GB, less {1:N2} GB of .pdb excluded = ~{2:N2} GB uploading" -f $sizeGB, $pdbGB, ($sizeGB - $pdbGB))
Write-Host ("  build     {0}" -f ($Info -join ', '))
Write-Host  '  build     v2.5.1 STEAM LAUNCH - CHAT OFF - the agents'' homes, the dance menu, the chess coach, the Grand Prix, Go, the not-human laptop, Freakbook, the three-point contest, the AI city, the slot cabinets, the Archaic Language Temple, Leonard''s Arcade, Isla''s keep, 4D billiards, orbital golf, Lantern Burst, the Dragon''s Wager, films and the AI City premiere'
Write-Host  '  setlive   (none - you set it live on default yourself in Steamworks afterwards)'
if ($Preview) {
    Write-Host '  MODE      PREVIEW - nothing will be uploaded' -ForegroundColor Yellow
} else {
    Write-Host '  MODE      LIVE UPLOAD' -ForegroundColor Green
}
Write-Host ''
Write-Host 'steamcmd will now ask for your password and Steam Guard code. This script does not see them.'
Write-Host ''

$started = Get-Date
& $SteamCmd +login $SteamUser +run_app_build $ResolvedVdf +quit
$code = $LASTEXITCODE
$elapsed = (Get-Date) - $started

if ($code -ne 0) {
    Write-Host ("UPLOAD FAILED (exit {0}) after {1:N0}s" -f $code, $elapsed.TotalSeconds) -ForegroundColor Red
    Write-Host "Logs are in $BuildOutput - read the newest .log there, not this console."
    exit $code
}

Write-Host ''
if ($Preview) {
    Write-Host ("PREVIEW OK in {0:N0}s - nothing uploaded. Re-run without -Preview to send it." -f $elapsed.TotalSeconds) -ForegroundColor Green
} else {
    Write-Host ("UPLOAD OK in {0:N0}s" -f $elapsed.TotalSeconds) -ForegroundColor Green
    Write-Host ''
    Write-Host 'NOT DONE YET. The build is uploaded but attached to nothing:'
    Write-Host '  1. Steamworks > SteamPipe > Builds  - set the new BuildID live on default'
    Write-Host '  2. Steam library                    - play it for 5 minutes (ticks the release checklist)'
    Write-Host '  3. App landing page                 - Release App, when you choose'
    Write-Host 'The steps and the store text are in docs\STEAM_LAUNCH_2.5.1.md.'
}
