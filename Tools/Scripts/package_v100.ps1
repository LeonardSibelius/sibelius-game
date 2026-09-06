# Package the v1.0 SHIPPING Win64 build — the release.
#
# ---------------------------------------------------------------------------
# WHY THE NUMBER GOES DOWN.
#
# itch has already had 1.1.0 through 1.4.0. This is 1.0 anyway, and it is Walt's call:
# "It feels like a solid 1.0 version that can go onto Steam with no reservations."
#
# He is right that this is the first build he would hand a stranger without a caveat, and
# Steam has no history to contradict it — nothing precedes this there. On itch the number
# will read oddly for one release and then never matter again. Nothing breaks: butler
# treats --userversion as a label and the itch app tracks build ids, not version strings.
#
# ---------------------------------------------------------------------------
# WHAT IS IN IT THAT 1.4.0 DID NOT HAVE — all of it C++, none of it new assets.
#
# docs/FUN_PLAN_2.md Part A, entire:
#
#   A1  The last third of the game has an objective banner. ComputeObjective went silent
#       the moment the Synthesis was claimed, which was right when the cathedral was the
#       end; four levels come after it now.
#   A2  The game ENDS instead of stopping. Nyra finishes, and the eight memoir messages
#       roll, then the makers, then eleven artists, then the two doors.
#   A3  The Architects battle stops promising a sequel from the midpoint.
#   A4  Nyra stands down when the ship goes, instead of standing on the uFoods sidewalk
#       telling him to go and board a rocket that left.
#   A5  He goes looking for her and finds the sidewalk empty; the way that opens is not
#       visible to a body until he holds V; and at ignition every ghost in the city turns
#       to face the pad.
#
# Because it is all code, this script's asset checks are 1.4.0's carried forward. That is
# the point of carrying them: the release that adds no assets is exactly the release where
# a cook regression would go unnoticed.
#
# ONE CHECK IS NEW. A5b turns the city's ghosts by looking up SKM_Manny_Simple by asset
# path at runtime. If that mesh ever stops cooking, the ghosts do not turn, nothing errors,
# and the only symptom is a city that fails to react — the exact class of silent failure
# this list exists for.
$root    = "C:\Users\wpark\projects\sibelius-game"
$version = "1.0.0"
$outDir  = "C:\Users\wpark\builds\sibelius-v$version"
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-v100.log"

# The pak we are growing from. This release should add ROUGHLY NOTHING — it is all C++ —
# so a jump here means something got dragged into the cook that should not have been.
$baselinePaks = "C:\Users\wpark\builds\sibelius-v1.4.0\Windows\SibeliusGame\Content\Paks"

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
  # --- the world ------------------------------------------------------------
  "the city"            = "$cooked\Maps\L_City.umap"
  "the cafe"            = "$cooked\Maps\L_Cafe.umap"
  "the burger"          = "$cooked\Fab\Burger\burger\StaticMeshes\burger.uasset"
  "the ghosts' glow"    = "$cooked\Phantom\Manny_Glow.uasset"
  # NEW IN 1.0 (A5b). The ghost turn finds these actors by MESH ASSET PATH at runtime -
  # by path and not by label, because labels are editor-only data the cook throws away.
  # No mesh, no turn, no error: the city just fails to react and nothing says why.
  "the ghosts' bodies"  = "$cooked\Characters\Mannequins\Meshes\SKM_Manny_Simple.uasset"
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

  # --- the shop -------------------------------------------------------------
  "the supermarket"     = "$cooked\Maps\L_uFoods.umap"
  "its floor"           = "$cooked\Poly_Supermarket_01\Meshes\Modular\SM_Floor_01_A.uasset"
  "its walls"           = "$cooked\Poly_Supermarket_01\Meshes\Modular\SM_Wall_01.uasset"
  "the till"            = "$cooked\Poly_Supermarket_01\Meshes\Modular\SM_Cashier_Table_01.uasset"
  "the trolleys"        = "$cooked\Poly_Supermarket_01\Meshes\Props\SM_Cart_01.uasset"

  # --- the ending -----------------------------------------------------------
  "the planet"          = "$cooked\Maps\L_Grok.umap"
  "her last words"      = "$cooked\Audio\Dancers\dancer_guide5_nyra.uasset"
  "her mouth"           = "$cooked\Audio\Dancers\dancer_guide5_nyra_face.uasset"
  # THE PORTAL. A gitignored purchased pack reached only by a hard reference from
  # ASpaceport's constructor. Without it there is NO WAY TO GROK, and in 1.0 that is worse
  # than it was in 1.4.0: the portal is now INVISIBLE until Code Vision reveals it, so a
  # missing one looks exactly like a player who has not held V yet.
  "the way to Grok"     = "$cooked\PortalVFX\NS\NS_TeleporterHole.uasset"

  # --- the guide's voices ---------------------------------------------------
  "Nyra's spaceport speech" = "$cooked\Audio\Dancers\dancer_guide3_nyra.uasset"
  "the spaceport face"      = "$cooked\Audio\Dancers\dancer_guide3_nyra_face.uasset"
  "Nyra's boarding speech"  = "$cooked\Audio\Dancers\dancer_guide4_nyra.uasset"
  "the boarding face"       = "$cooked\Audio\Dancers\dancer_guide4_nyra_face.uasset"
  "Nyra's deli speech"      = "$cooked\Audio\Dancers\dancer_guide2_nyra.uasset"
  "Kaia's power face"       = "$cooked\Audio\Dancers\dancer_power_kaia_face.uasset"
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

# ---- size, which should barely move -----------------------------------------
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
Write-Output ("pak  v1.4.0   : {0} GB" -f $old)
Write-Output ("pak  v1.0     : {0} GB" -f $new)
if ($old -and $new) {
  $delta = [math]::Round($new - $old, 2)
  Write-Output ("delta         : {0} GB   (expect ~0.00 - this release is all C++)" -f $delta)
}
Write-Output ("whole archive : {0} GB" -f $archive)
Write-Output "--------------------------------------"

# ---- THE PLUME, which can still go missing quietly --------------------------
$plume = Get-ChildItem "$root\Saved\Cooked\Windows" -Recurse -Filter "Grid3D_Gas_ColoredSmoke.uasset" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($plume) {
  Write-Output ""
  Write-Output ("PLUME OK      : {0}" -f $plume.FullName.Replace("$root\Saved\Cooked\Windows\",''))
} else {
  Write-Output ""
  Write-Output "PLUME MISSING : the launch will have no fire. Do NOT push to itch."
}

Write-Output "COOK VERIFIED. Archive: $outDir"
