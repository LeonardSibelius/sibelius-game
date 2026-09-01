# Package the v1.0.0 SHIPPING Win64 build — "The City".
#
# Content of 1.0.0: the game has somewhere to go after it ends. Clear the Refuser army
# and the meadow offers two doors — [O] back to the office he has earned the right to
# leave, and [>] to a city he has never seen. [>] is a real hotkey now, listed in the
# CONTROLS menu from the first minute, greyed out and saying "a city is waiting - it
# opens when the Architects fall". Press it early and it names the next thing to do
# instead of just refusing. The city is Downtown West's demo street, duplicated to
# L_City: a lit shopping street with awnings, trees and a hot-dog cart.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. L_City HAS TO BE IN THE PAK. It reached MapsToCook for this release; without that
#    entry OpenLevel finds nothing and [>] fades to black until the travel watchdog
#    rescues the player sixty seconds later — which looks exactly like a hang.
#
# 2. THE LIGHTING SUBLEVEL HAS TO COME WITH IT. L_City streams
#    Downtown_West/Maps/Sub-Levels/Daytime_Lighting. It is a package reference so the
#    cooker should follow it, but "should" is how the Greystone soft-pointer trap read
#    right up until a packaged build entered battle form with no hero. Verified, not
#    assumed.
#
# 3. EVERYTHING 0.9.8.0 NEEDED IS STILL THERE. Greystone is still reached by SOFT
#    pointers from UBattleFormComponent and by string path from USlapComponent, so he is
#    still in the pak only because DirectoriesToAlwaysCook says so. A regression here
#    would be silent.
#
# SIZE IS NOT A FAILURE CONDITION HERE. Walt, 2026-08-29: "i actually don't mind the
# size, a beautiful open world like that is worth the download cost to me." Downtown West
# is 12 GB on disk with 6.8 GB of textures and L_City places 6,609 actors, so this build
# is expected to be much larger than 0.9.8.0's 5.88 GB pak. The script REPORTS the delta
# and does not judge it.

$root    = "C:\Users\wpark\projects\sibelius-game"
$version = "1.0.0"
$outDir  = "C:\Users\wpark\builds\sibelius-v$version"
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-vv100.log"

# The pak we are growing from, so the cost of the city is a number and not a feeling.
$baselinePaks = "C:\Users\wpark\builds\sibelius-v0.9.9.3\Windows\SibeliusGame\Content\Paks"

if (Test-Path $log) { Remove-Item $log -Force }

$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=$root\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=$outDir",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
$code = $LASTEXITCODE
Write-Output "EXITCODE=$code"

if ($code -ne 0) {
  Write-Output "PACKAGE FAILED - see $log"
  exit $code
}

# ---- verify the things this release cannot ship without ----------------------
$cooked = "$root\Saved\Cooked\Windows\SibeliusGame\Content"
$checks = [ordered]@{
  "the city"            = "$cooked\Maps\L_City.umap"
  "the cafe"            = "$cooked\Maps\L_Cafe.umap"
  "the burger"          = "$cooked\Fab\Burger\burger\StaticMeshes\burger.uasset"
  "the ghosts' glow"    = "$cooked\Phantom\Manny_Glow.uasset"
  "Nyra's guide voice"  = "$cooked\Audio\Dancers\dancer_guide_nyra.uasset"
  # 1.0.0 — the faces. Eight baked performances turn five agents from voices over still
  # portraits into people saying their own words, and they cook ONLY because
  # /Game/Audio/Dancers is in DirectoriesToAlwaysCook. Two spot-checks, one per kind.
  "Nyra's guide face"   = "$cooked\Audio\Dancers\dancer_guide_nyra_face.uasset"
  "Kaia's power face"   = "$cooked\Audio\Dancers\dancer_power_kaia_face.uasset"
  # Her second speech, outside the deli - the newest thing in the build and therefore
  # the likeliest to be missing.
  "Nyra's 2nd speech"   = "$cooked\Audio\Dancers\dancer_guide2_nyra.uasset"
  "Nyra's 2nd face"     = "$cooked\Audio\Dancers\dancer_guide2_nyra_face.uasset"
  # THE HIGHEST-RISK ASSET IN THIS RELEASE. Every other animation is hard-referenced on
  # a CDO; this one cannot be, because an FObjectFinder on it crashes the editor at
  # startup. It reaches the pak by DirectoriesToAlwaysCook alone - the exact arrangement
  # that shipped v0.7.4 with missing content and looked fine in PIE. Without it Nyra
  # stands outside the deli in her bind pose.
  "the guide's idle"    = "$cooked\Characters\Retargeting\Combat\GS_Idle_MH.uasset"
  "the city's daylight" = "$cooked\Downtown_West\Maps\Sub-Levels\Daytime_Lighting.umap"
  "the meadow"          = "$cooked\Cinematics\L_Meadow.umap"
  "Greystone mesh"      = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Meshes\Greystone.uasset"
  "Attack_PrimaryA"     = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Animations\Attack_PrimaryA_Montage.uasset"
  "Mrs. Hall's lines"   = "$cooked\Data\MrsHallStory.uasset"
}
$missing = 0
foreach ($k in $checks.Keys) {
  if (Test-Path $checks[$k]) { Write-Output "OK      $k" }
  else { Write-Output "MISSING $k  -> $($checks[$k])"; $missing++ }
}
if ($missing -gt 0) {
  Write-Output "COOK IS INCOMPLETE - $missing required asset(s) absent. Do NOT push."
  exit 1
}

# ---- what did the city actually cost? ---------------------------------------
function PakGB($path) {
  if (-not (Test-Path $path)) { return $null }
  [math]::Round((Get-ChildItem $path -Recurse -File | Measure-Object Length -Sum).Sum / 1GB, 2)
}
$newPaks = "$outDir\Windows\SibeliusGame\Content\Paks"
$new = PakGB $newPaks
$old = PakGB $baselinePaks
$archive = [math]::Round((Get-ChildItem $outDir -Recurse -File | Measure-Object Length -Sum).Sum / 1GB, 2)

Write-Output ""
Write-Output "---------------- SIZE ----------------"
Write-Output ("pak  v0.9.8.0 : {0} GB" -f $old)
Write-Output ("pak  v1.0.0 : {0} GB" -f $new)
if ($old -and $new) { Write-Output ("the city cost : {0} GB" -f [math]::Round($new - $old, 2)) }
Write-Output ("whole archive : {0} GB" -f $archive)
Write-Output "--------------------------------------"

# The ten biggest cooked folders, so if the number IS a surprise it says where to look.
Write-Output ""
Write-Output "biggest cooked folders:"
Get-ChildItem $cooked -Directory |
  ForEach-Object {
    [PSCustomObject]@{
      Folder = $_.Name
      GB = [math]::Round((Get-ChildItem $_.FullName -Recurse -File -ErrorAction SilentlyContinue |
            Measure-Object Length -Sum).Sum / 1GB, 2)
    }
  } | Sort-Object GB -Descending | Select-Object -First 10 | Format-Table -AutoSize

Write-Output "COOK VERIFIED. Archive: $outDir"
