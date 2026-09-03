# Package the v1.2.0 SHIPPING Win64 build — "The Supply Run".
#
# Content of 1.2.0: uFoods stops being a painted storefront. Generate the spaceport and
# Nyra appears at the lawn's edge — while the eight-second assembly holds the player's
# eye, because L_City has no navmesh and she cannot walk. She sends him down the block to
# a real supermarket (Poly Supermarket 01, duplicated from its showcase the way L_Cafe was
# duplicated from RestaurantScene), where one [E] buys every supply at once. He comes out
# and she is waiting again — with the news that she is uploading herself into the ship and
# coming with him to Grok, forty light years away.
#
# The guide has three stages now, and the stage is no longer read once at BeginPlay.
#
# Boarding does not exist yet. That is the next thing, and her own line names it:
# "go back to the spaceport and we will do the boarding procedures".
#

$root    = "C:\Users\wpark\projects\sibelius-game"
$version = "1.2.0"
$outDir  = "C:\Users\wpark\builds\sibelius-v$version"
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-v120.log"

# The pak we are growing from, so the cost of the supermarket is a number and not a feeling.
$baselinePaks = "C:\Users\wpark\builds\sibelius-v1.1.0\Windows\SibeliusGame\Content\Paks"

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
  # --- carried forward from 1.1.0: still true, still worth failing on -------
  "the city"            = "$cooked\Maps\L_City.umap"
  "the cafe"            = "$cooked\Maps\L_Cafe.umap"
  "the burger"          = "$cooked\Fab\Burger\burger\StaticMeshes\burger.uasset"
  "the ghosts' glow"    = "$cooked\Phantom\Manny_Glow.uasset"
  "the rocket"          = "$cooked\Rocket_Launch_Pad\Meshes\Rocket\SM_Rocket.uasset"
  "the launch pad"      = "$cooked\Rocket_Launch_Pad\Meshes\Environment\SM_Launch_Pad.uasset"
  "the pad's apron"     = "$cooked\Rocket_Launch_Pad\Meshes\Environment\SM_Ground.uasset"
  "the materialise fx"  = "$cooked\AIApparition\M_materialise.uasset"
  "the generate catalog"= "$cooked\Data\GenerateCatalog.uasset"
  "the guide's idle"    = "$cooked\Characters\Retargeting\Combat\GS_Idle_MH.uasset"
  "the city's daylight" = "$cooked\Downtown_West\Maps\Sub-Levels\Daytime_Lighting.umap"
  "the meadow"          = "$cooked\Cinematics\L_Meadow.umap"
  "Greystone mesh"      = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Meshes\Greystone.uasset"
  "Mrs. Hall's lines"   = "$cooked\Data\MrsHallStory.uasset"

  # --- 1.2.0: THE SHOP -----------------------------------------------------
  # L_uFoods reaches the pak through ONE +MapsToCook line, and its 1,956 meshes come with
  # it only because a LEVEL hard-references what it places. That is the opposite of the
  # Rocket_Launch_Pad arrangement (soft paths from C++, needing DirectoriesToAlwaysCook),
  # and it is why Poly_Supermarket_01 has no directory rule of its own. If reference
  # following ever stops working the way it does today, the shop ships as an empty room
  # and PIE will never tell us. So check the room AND its walls AND its props.
  "the supermarket"     = "$cooked\Maps\L_uFoods.umap"
  "its floor"           = "$cooked\Poly_Supermarket_01\Meshes\Modular\SM_Floor_01_A.uasset"
  "its walls"           = "$cooked\Poly_Supermarket_01\Meshes\Modular\SM_Wall_01.uasset"
  "the till"            = "$cooked\Poly_Supermarket_01\Meshes\Modular\SM_Cashier_Table_01.uasset"
  # The trolleys are not decoration: move_ufoods_playerstart.py finds the ENTRANCE by
  # their centroid, so no carts means the player arrives somewhere arbitrary.
  "the trolleys"        = "$cooked\Poly_Supermarket_01\Meshes\Props\SM_Cart_01.uasset"

  # --- 1.2.0: THE GUIDE'S NEW VOICES ---------------------------------------
  # Four assets, all reaching the pak by DirectoriesToAlwaysCook on /Game/Audio/Dancers,
  # which is the v0.7.4 arrangement exactly: gitignored content that PIE finds locally and
  # a shipped build does not. Without these she stands there in silence and the feature
  # looks broken rather than unrecorded.
  "Nyra's spaceport speech" = "$cooked\Audio\Dancers\dancer_guide3_nyra.uasset"
  "the spaceport face"      = "$cooked\Audio\Dancers\dancer_guide3_nyra_face.uasset"
  "Nyra's boarding speech"  = "$cooked\Audio\Dancers\dancer_guide4_nyra.uasset"
  "the boarding face"       = "$cooked\Audio\Dancers\dancer_guide4_nyra_face.uasset"
  # Carried from 1.1.0 - the earlier stages must not have regressed while stages were added.
  "Nyra's deli speech"  = "$cooked\Audio\Dancers\dancer_guide2_nyra.uasset"
  "Kaia's power face"   = "$cooked\Audio\Dancers\dancer_power_kaia_face.uasset"
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

# ---- what did the supermarket actually cost? --------------------------------
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
Write-Output ("pak  v1.1.0   : {0} GB" -f $old)
Write-Output ("pak  v1.2.0   : {0} GB" -f $new)
if ($old -and $new) { Write-Output ("the shop cost : {0} GB" -f [math]::Round($new - $old, 2)) }
Write-Output ("whole archive : {0} GB" -f $archive)
Write-Output "--------------------------------------"

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
