# Build and run the standalone Go engine tests - no Unreal, no editor, no cook.
#
#   powershell -ExecutionPolicy Bypass -File Tools\Tests\run_go_engine_tests.ps1
#
# Compiles Source\SibeliusGame\GoEngine.cpp and Tools\Tests\go_engine_test.cpp with the
# Visual Studio compiler that UE 5.7 already requires, at /W4 /WX, so a warning here is a
# warning Unreal would likely complain about too. Output goes to %TEMP%, never the repo.
#
# WHY THIS EXISTS: the same reason as run_pinball_sim_tests.ps1. The Go engine does not
# touch Unreal, so the whole of Go - the rules, the count, the search - is
# checked here in under a minute, before a table actor exists.

$ErrorActionPreference = 'Stop'

$repo    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$simDir  = Join-Path $repo 'Source\SibeliusGame'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found at $vswhere - is Visual Studio installed?" }

$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "No Visual Studio with the C++ x64 tools found." }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

$out = Join-Path $env:TEMP 'sibelius-go-tests'
New-Item -ItemType Directory -Force -Path $out | Out-Null

# A .bat rather than cmd /c "...": PowerShell 5.1 re-quotes arguments to native programs,
# and a command line full of quoted paths does not survive the trip.
$bat = Join-Path $out 'build.bat'
@"
@echo off
rem vcvars64 looks for vswhere on PATH and prints a harmless error if it is not there.
set "PATH=$(Split-Path -Parent $vswhere);%PATH%"
call "$vcvars" >nul || exit /b 1
cd /d "$out" || exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /W4 /WX /I"$simDir" "$simDir\GoEngine.cpp" "$PSScriptRoot\go_engine_test.cpp" /Fe:go_engine_test.exe
"@ | Set-Content -Path $bat -Encoding ASCII

& cmd.exe /c $bat
if ($LASTEXITCODE -ne 0) { Write-Host 'COMPILE FAILED' -ForegroundColor Red; exit $LASTEXITCODE }

Write-Host ''
& (Join-Path $out 'go_engine_test.exe')
exit $LASTEXITCODE
