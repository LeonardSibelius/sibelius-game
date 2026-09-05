# Package the v1.4.0 SHIPPING Win64 build — "Grok".
#
# Content of 1.4.0: the game has an ending.
#
# C boards the rocket and C launches it. The ship goes without him. Where it stood a
# portal opens; C goes through, and he arrives on an alien world by the road Nyra should
# have brought him by. She is waiting, and she apologises: "I ran the numbers and I was
# very sure and I was completely wrong. A rocket is a body's way of going somewhere. I am
# not a body." Then she tells a 71-year-old programmer that he was born forty years too
# early and survived to reach the AI age. "Good job, Leonard."
#
# THE BIG NEW COST IS THE PLANET. L_Grok is duplicated from Elite Landscapes: Alien Part
# IV, and a LANDSCAPE lives inside its own .umap - about 647 MB before cooking. Only that
# one map of the pack's six is named in MapsToCook, so the cooker follows only its
# references and the other five never reach the pak. READ THE SIZE BLOCK AT THE BOTTOM
# before pushing: this is the first release that could meaningfully lengthen the download.
#
# AND THREE THINGS THAT CAN GO MISSING QUIETLY, all reached only by hard reference from
# C++ into gitignored purchased packs: the portal, the plume, and Nyra's last words. A
# soft path would work in PIE and vanish from the pak - the v0.7.4 invisible-spaceport
# bug. All three are checked.
$root    = "C:\Users\wpark\projects\sibelius-game"
$version = "1.4.0"
$outDir  = "C:\Users\wpark\builds\sibelius-v$version"
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-v140.log"

# The pak we are growing from, so the cost of the supermarket is a number and not a feeling.
$baselinePaks = "C:\Users\wpark\builds\sibelius-v1.3.0\Windows\SibeliusGame\Content\Paks"

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

  # --- 1.4.0: THE ENDING ---------------------------------------------------
  # L_Grok reaches the pak through ONE +MapsToCook line, and a LANDSCAPE rides inside its
  # own .umap rather than referencing external assets - so the map IS the payload, and
  # checking it is checking the planet. Elite_AlienPack_04 needs no directory rule: the
  # level hard-references what it places, the L_uFoods arrangement.
  "the planet"          = "$cooked\Maps\L_Grok.umap"
  # Nyra's last words and the face that speaks them. /Game/Audio/Dancers cooks by
  # DirectoriesToAlwaysCook, so a missing one of these means the import never ran - and
  # her scene would play in silence with a still face, which is worse than not shipping.
  "her last words"      = "$cooked\Audio\Dancers\dancer_guide5_nyra.uasset"
  "her mouth"           = "$cooked\Audio\Dancers\dancer_guide5_nyra_face.uasset"
  # THE PORTAL. Content/PortalVFX/ is a gitignored purchased pack reached only by a hard
  # reference from ASpaceport's constructor. Without it there is NO WAY TO GROK - the last
  # scene becomes unreachable and nothing on screen says why.
  "the way to Grok"     = "$cooked\PortalVFX\NS\NS_TeleporterHole.uasset"
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
Write-Output ("pak  v1.3.0   : {0} GB" -f $old)
Write-Output ("pak  v1.4.0   : {0} GB" -f $new)
if ($old -and $new) { Write-Output ("Grok cost     : {0} GB" -f [math]::Round($new - $old, 2)) }
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

# ---- THE PLUME, which is the one thing 1.3.0 can lose quietly ----------------
# A WARNING, NOT A FAILURE. The cooked path for plugin template content is not something
# to hard-code and false-fail a release on; what matters is that a human reads this line
# before the butler push. If it says MISSING, the launch cutscene ships with no fire.
$plume = Get-ChildItem "$root\Saved\Cooked\Windows" -Recurse -Filter "Grid3D_Gas_ColoredSmoke.uasset" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($plume) {
  Write-Output ""
  Write-Output ("PLUME OK      : {0}" -f $plume.FullName.Replace("$root\Saved\Cooked\Windows\",''))
} else {
  Write-Output ""
  Write-Output "PLUME MISSING : the launch will have no fire. Do NOT push to itch."
}

Write-Output "COOK VERIFIED. Archive: $outDir"
