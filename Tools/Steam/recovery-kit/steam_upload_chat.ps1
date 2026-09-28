# Upload the v2.0 AI-chat SHIPPING build from the E: recovery kit - app 5249380, depot 5249381.
#
# Adapted 2026-09-26 from Tools\Scripts\steam_upload_v113.ps1 (the untouched originals are in
# .\original\). Two differences, both deliberate:
#
#   1. Every path is found RELATIVE TO THIS FOLDER. The stick may not be E: on another PC,
#      and nothing here should care which letter it gets.
#   2. It REFUSES TO RUN without -AmendmentAccepted.
#
# It also NEVER MODIFIES ITS OWN .vdf FILES. The original rewrote the tracked app vdf on
# every run; this one writes a resolved copy into -BuildOutput and runs steamcmd on that.
#
# TWO PLACES IT LIVES, ONE SCRIPT:
#   - On the recovery stick, E:\builds\steam-upload\  - the defaults find the chat build
#     one folder up. No path arguments needed.
#   - In the repo, Tools\Steam\recovery-kit\  - pass -ContentRoot, e.g.
#     -ContentRoot C:\Users\wpark\builds\sibelius-shipping-chat-20260927\Windows
#
# ---------------------------------------------------------------------------
# DO NOT RUN THIS UNTIL VALVE HAS ACCEPTED THE 2.0 CONTENT-SURVEY AMENDMENT.
#
# The build it uploads has live AI chat compiled in. Steam's current survey answer says
# nothing is generated at runtime. Uploading this first puts a build in front of Valve that
# contradicts its own survey. See ..\README.md, "The one rule".
#
# ---------------------------------------------------------------------------
# THIS SCRIPT NEVER SEES YOUR PASSWORD, AND IT MUST STAY THAT WAY.
#
# It passes your Steam ACCOUNT NAME to steamcmd and nothing else. steamcmd prompts for the
# password and the Steam Guard code itself - have your phone. Do not add a -Password
# parameter, do not put credentials in a .vdf, do not paste them onto a command line.
#
# ---------------------------------------------------------------------------
# PREVIEW FIRST. ALWAYS.
#
#   powershell -ExecutionPolicy Bypass -File .\steam_upload_chat.ps1 `
#       -SteamUser wparkman676 -SdkPath C:\steamworks_sdk -AmendmentAccepted -Preview
#
# Preview walks the content, prints what it WOULD send, and uploads nothing. Then run the
# same line without -Preview.
#
# Uploading is NOT submitting and NOT releasing. "setlive" is empty, so the build lands in
# Steamworks attached to no branch. The two steps after it are printed at the end.
#
# ---------------------------------------------------------------------------
# THE SDK IS NOT ON THIS STICK. Download steamworks_sdk.zip from partner.steamgames.com
# (partner login), unzip it anywhere, and point -SdkPath at the folder containing
# sdk\tools\ContentBuilder\builder\steamcmd.exe.

[CmdletBinding()]
param(
    # Your Steam ACCOUNT NAME. Never the password.
    [Parameter(Mandatory = $true)]
    [string] $SteamUser,

    # Folder containing sdk\tools\ContentBuilder\builder\steamcmd.exe
    [string] $SdkPath = 'C:\steamworks_sdk',

    # The packaged build's Windows folder. Default: the chat build beside this kit.
    [string] $ContentRoot,

    # SteamPipe's chunk cache and logs, plus the resolved .vdf copies. Default: .\buildoutput
    [string] $BuildOutput,

    # Dry run: report what would upload, send nothing.
    [switch] $Preview,

    # Say it out loud: Valve accepted the 2.0 amendment. Without this the script stops.
    [switch] $AmendmentAccepted
)

$ErrorActionPreference = 'Stop'

if (-not $AmendmentAccepted) {
    throw ("Refusing to upload. This build has live AI chat, and Steam's survey still says nothing " +
           "is generated at runtime. Re-run with -AmendmentAccepted ONLY after Valve accepts the " +
           "2.0 amendment (see ..\README.md).")
}

$Kit      = $PSScriptRoot
$AppVdf   = Join-Path $Kit 'app_build_5249380.vdf'
$DepotVdf = Join-Path $Kit 'depot_build_5249381.vdf'
if (-not $ContentRoot) { $ContentRoot = Join-Path (Split-Path -Parent $Kit) 'sibelius-shipping-chat-20260927\Windows' }
if (-not $BuildOutput) { $BuildOutput = Join-Path $Kit 'buildoutput' }

# ---------------------------------------------------------------------------
# PRE-FLIGHT - the same checks as the original.

if (-not (Test-Path $AppVdf))   { throw "No app build script at $AppVdf" }
if (-not (Test-Path $DepotVdf)) { throw "No depot script at $DepotVdf" }
if (-not (Test-Path $ContentRoot)) { throw "Content root missing: $ContentRoot" }

$Exe = Join-Path $ContentRoot 'SibeliusGame.exe'
if (-not (Test-Path $Exe)) { throw "No SibeliusGame.exe at $ContentRoot - is this a staged build?" }

# A pak present but far too small means the cook dropped content: the upload would
# succeed and the game would be broken.
$Ucas = Join-Path $ContentRoot 'SibeliusGame\Content\Paks\SibeliusGame-Windows.ucas'
if (-not (Test-Path $Ucas)) { throw "No .ucas at $Ucas - this build did not cook." }
$UcasGB = (Get-Item $Ucas).Length / 1GB
if ($UcasGB -lt 6.5) {
    throw ("SibeliusGame-Windows.ucas is only {0:N2} GB. A full cook is ~7.2 GB - content is missing." -f $UcasGB)
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
# preview flag. The kit's own .vdf files are never touched. Absolute paths computed here
# rather than trusting SteamPipe's relative-path rules. The depot vdf is copied alongside
# because the app vdf names it by bare filename.
# .NET with BOM-less UTF-8, not Set-Content - see the original for why.

function Set-VdfValue([string] $Text, [string] $Key, [string] $Value) {
    $safe = $Value.Replace('$', '$$')
    return [regex]::Replace($Text, ('(?m)^(\s*"' + $Key + '"\s*)"[^"]*"'), ('${1}"' + $safe + '"'))
}

$utf8NoBom = New-Object System.Text.UTF8Encoding $false
$vdf = [System.IO.File]::ReadAllText($AppVdf, $utf8NoBom)
$vdf = Set-VdfValue $vdf 'contentroot' $ContentRoot
$vdf = Set-VdfValue $vdf 'buildoutput' $BuildOutput
$vdf = Set-VdfValue $vdf 'preview'     $(if ($Preview) { '1' } else { '0' })
$ResolvedVdf = Join-Path $BuildOutput 'app_build_5249380.resolved.vdf'
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
Write-Host  '  build     v2.0 - AI CHAT COMPILED IN'
Write-Host  '  setlive   (none - you set the branch yourself in Steamworks afterwards)'
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
    Write-Host '  1. Steamworks > SteamPipe > Builds  - set the build live on a branch'
    Write-Host '  2. App landing page                 - Mark build ready for review (3-5 business days)'
}
