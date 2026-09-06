# SUPERSEDED by the 1.0 renumbering — kept as the record of the old 1.1.0.
# Package the v1.1.0 SHIPPING Win64 build — "The Spaceport".
#
# Content of 1.1.0: Generate stops being able to make only a lamp. A catalog row can now
# name an ACTOR class, and the first one is a spaceport - type it on the lawn across from
# Jacob's and a 120-metre launch complex materialises 160 metres out in the field, part
# by part, out of a cyan apparition. It is PackDev's own composition, read out of their
# showcase map rather than guessed. Test-Drive can branch it and discard it for free,
# because it extends ABuildSite and inherited that.
#
# The rocket does not launch yet. That is Phase C.
#

$root    = "C:\Users\wpark\projects\sibelius-game"
$version = "1.1.0"
$outDir  = "C:\Users\wpark\builds\sibelius-v$version"
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-vv110.log"

# The pak we are growing from, so the cost of the city is a number and not a feeling.
$baselinePaks = "C:\Users\wpark\builds\sibelius-v1.0.0\Windows\SibeliusGame\Content\Paks"

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
  # 1.1.0 - THE SPACEPORT, and every line of it is here because the whole thing reaches
  # the pak by DIRECTORY RULE rather than by reference. Spaceport.cpp names these meshes
  # by soft path, which the cooker does not follow; the catalog row names the class as a
  # string; the apparition material is loaded on demand. Nothing in this feature is a
  # package reference, so nothing in it is safe without checking.
  "the rocket"          = "$cooked\Rocket_Launch_Pad\Meshes\Rocket\SM_Rocket.uasset"
  "the launch pad"      = "$cooked\Rocket_Launch_Pad\Meshes\Environment\SM_Launch_Pad.uasset"
  "the pad's apron"     = "$cooked\Rocket_Launch_Pad\Meshes\Environment\SM_Ground.uasset"
  "the rocket holder"   = "$cooked\Rocket_Launch_Pad\Meshes\Environment\SM_Rocket_Holder.uasset"
  # Without this the spaceport still builds - in dead silence, with no fade, popping into
  # existence one part at a time. Visible only to someone who knew what it should look like.
  "the materialise fx"  = "$cooked\AIApparition\M_materialise.uasset"
  # The row that makes "spaceport" a word the game knows. A stale catalog is the one
  # failure that looks exactly like the player typing the wrong thing.
  "the generate catalog"= "$cooked\Data\GenerateCatalog.uasset"
  # 1.1.0 — the faces. Eight baked performances turn five agents from voices over still
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
Write-Output ("pak  v1.0.0  : {0} GB" -f $old)
Write-Output ("pak  v1.1.0 : {0} GB" -f $new)
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
