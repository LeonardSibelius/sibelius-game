# Upload the v1.1.3 SHIPPING Win64 build to Steam — app 5249380, depot 5249381.
#
# ---------------------------------------------------------------------------
# THIS SCRIPT NEVER SEES YOUR PASSWORD, AND IT MUST STAY THAT WAY.
#
# It passes your Steam ACCOUNT NAME to steamcmd and nothing else. steamcmd prompts for
# the password and the Steam Guard code itself, in its own process, and caches the
# session afterwards so later uploads are non-interactive. Do not add a -Password
# parameter to this script, do not put credentials in a .vdf, and do not paste them onto
# a command line — command lines land in PowerShell history in plain text.
#
# ---------------------------------------------------------------------------
# RUN IT IN PREVIEW FIRST. ALWAYS.
#
#   .\Tools\Scripts\steam_upload_v113.ps1 -SteamUser wparkman676 -Preview
#
# Preview walks the whole content tree, prints what it WOULD send, and uploads nothing.
# It takes seconds. It is the only cheap way to find out that contentroot points at the
# wrong version, and the alternative is discovering it after 7.5 GB has gone up a home
# connection.
#
# ---------------------------------------------------------------------------
# WHAT THIS UPLOAD IS FOR, AND WHAT IT IS NOT.
#
# Uploading a build is NOT submitting it for review, and it is NOT releasing anything.
# app_build_5249380.vdf has "setlive" empty on purpose, so the build lands in Steamworks
# unattached to any branch. Three separate things have to happen after this script:
#
#   1. SteamPipe > Builds — find the build, set it live on the branch you want
#   2. App landing page   — "Mark build ready for review" (3-5 BUSINESS DAYS)
#   3. Release            — only once store page AND build are both approved
#
# The build review is the last unstarted gate on this release. It runs in parallel with
# the two-week 'Coming Soon' window, so starting it early costs nothing and starting it
# late costs a week.
#
# ---------------------------------------------------------------------------
# THE SDK IS NOT IN THIS REPO AND WILL NOT BE.
#
# Download steamworks_sdk.zip from partner.steamgames.com (it is behind the partner
# login), unzip it anywhere, and point -SdkPath at the folder containing
# sdk\tools\ContentBuilder\builder\steamcmd.exe. It is ~400 MB of Valve's binaries under
# their licence; it does not belong in git and it does not belong in LFS.

[CmdletBinding()]
param(
    # Your Steam ACCOUNT NAME. Never the password.
    [Parameter(Mandatory = $true)]
    [string] $SteamUser,

    # Folder containing sdk\tools\ContentBuilder\builder\steamcmd.exe
    [string] $SdkPath = 'C:\steamworks_sdk',

    # Dry run: report what would upload, send nothing.
    [switch] $Preview
)

$ErrorActionPreference = 'Stop'

$RepoRoot    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$AppVdf      = Join-Path $RepoRoot 'Tools\Steam\app_build_5249380.vdf'
$DepotVdf    = Join-Path $RepoRoot 'Tools\Steam\depot_build_5249381.vdf'
$ContentRoot = 'C:\Users\wpark\builds\sibelius-v1.1.3\Windows'
$BuildOutput = 'C:\Users\wpark\builds\steam_buildoutput'

# ---------------------------------------------------------------------------
# PRE-FLIGHT. Every one of these has cost someone a failed upload or a bad build.

if (-not (Test-Path $AppVdf))   { throw "No app build script at $AppVdf" }
if (-not (Test-Path $DepotVdf)) { throw "No depot script at $DepotVdf" }

if (-not (Test-Path $ContentRoot)) {
    throw "Content root missing: $ContentRoot`nRun Tools\Scripts\package_v113.ps1 first."
}

# The launcher Steam will run. If this name changes, Installation > Launch Options in
# Steamworks has to change with it, and nothing here will tell you.
$Exe = Join-Path $ContentRoot 'SibeliusGame.exe'
if (-not (Test-Path $Exe)) { throw "No SibeliusGame.exe at $ContentRoot - is this a staged build?" }

# The cook is the thing that fails silently. A pak that is present but far too small
# means the cook dropped content, and the build would upload perfectly and be broken.
$Ucas = Join-Path $ContentRoot 'SibeliusGame\Content\Paks\SibeliusGame-Windows.ucas'
if (-not (Test-Path $Ucas)) { throw "No .ucas at $Ucas - this build did not cook." }
$UcasGB = (Get-Item $Ucas).Length / 1GB
if ($UcasGB -lt 6.5) {
    throw ("SibeliusGame-Windows.ucas is only {0:N2} GB. v1.1.3 cooked to ~7.17 GB - content is missing." -f $UcasGB)
}

$SteamCmd = Join-Path $SdkPath 'sdk\tools\ContentBuilder\builder\steamcmd.exe'
if (-not (Test-Path $SteamCmd)) {
    throw "No steamcmd.exe at $SteamCmd`nDownload steamworks_sdk.zip from partner.steamgames.com and pass -SdkPath."
}

if (-not (Test-Path $BuildOutput)) {
    New-Item -ItemType Directory -Path $BuildOutput -Force | Out-Null
    Write-Host "Created build output cache at $BuildOutput"
}

# ---------------------------------------------------------------------------
# PREVIEW SWITCH. Rewrites the one line in the vdf rather than keeping two copies of it.

# Read and write through .NET with an explicit BOM-less UTF8, NOT Get-Content /
# Set-Content. PowerShell 5.1's defaults round-trip this file through the ANSI codepage
# and then back out as UTF-8, which silently mangles any non-ASCII character in the
# comments a little further on every single run - an em-dash became "a-tilde-euro-em"
# the first time this ran. Set-Content -Encoding utf8 also prepends a BOM.
$previewValue = if ($Preview) { '1' } else { '0' }
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
$vdf = [System.IO.File]::ReadAllText($AppVdf, $utf8NoBom)
$vdf = [regex]::Replace($vdf, '(?m)^(\s*"preview"\s*)"[01]"', ('${1}"' + $previewValue + '"'))
[System.IO.File]::WriteAllText($AppVdf, $vdf, $utf8NoBom)

$sizeGB = (Get-ChildItem $ContentRoot -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1GB
$pdb    = Get-ChildItem $ContentRoot -Recurse -Filter '*.pdb' -ErrorAction SilentlyContinue
$pdbGB  = 0
if ($pdb) { $pdbGB = ($pdb | Measure-Object -Property Length -Sum).Sum / 1GB }

Write-Host ''
Write-Host ("  app       5249380  Leonard Sibelius")
Write-Host ("  depot     5249381  Leonard Sibelius Content")
Write-Host ("  content   {0}" -f $ContentRoot)
Write-Host ("  on disk   {0:N2} GB, less {1:N2} GB of .pdb excluded = ~{2:N2} GB uploading" -f $sizeGB, $pdbGB, ($sizeGB - $pdbGB))
Write-Host ("  setlive   (none - you set the branch yourself in Steamworks afterwards)")
if ($Preview) {
    Write-Host '  MODE      PREVIEW - nothing will be uploaded' -ForegroundColor Yellow
} else {
    Write-Host '  MODE      LIVE UPLOAD' -ForegroundColor Green
}
Write-Host ''
Write-Host 'steamcmd will now ask for your password and Steam Guard code. This script does not see them.'
Write-Host ''

$started = Get-Date
& $SteamCmd +login $SteamUser +run_app_build $AppVdf +quit
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
