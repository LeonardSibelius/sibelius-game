# Package the v1.1.3 SHIPPING Win64 build — "The TVs work".
#
# ---------------------------------------------------------------------------
# WHAT IS IN IT THAT 1.1.2 DID NOT HAVE.
#
# The televisions in the living room and the bedroom answer E, and open the player's own
# browser on https://leonardsibelius.com/celestial-fortune/ — the web slot machine, on the
# open web, outside the memoir.
#
# One C++ change (InteractorComponent) and one tag on some actors in L_Office_v02. No new
# assets, so the asset checks below are 1.1.2's carried forward unchanged. That is exactly
# why they are carried: the release that adds nothing is the release where a cook
# regression would go unnoticed.
#
# ---------------------------------------------------------------------------
# ONE CHECK IS NEW, AND IT GUARDS THE ONLY THING THIS RELEASE CAN LOSE SILENTLY.
#
# The TVs are found by the actor tag CelestialFortuneWebTV. A tag is map data, and this
# map reaches the pak by MapsToCook — so the failure mode is not a missing file that
# Test-Path would catch. It is a cooked L_Office_v02 that loads perfectly and simply has
# no tagged actors in it: every TV goes dead, nothing errors, and the only symptom is a
# player pressing E at a television and getting the same nothing they got in 1.1.2.
#
# So the check reads the COOKED map and looks for the tag string in it. It is the same
# lesson as SKM_Manny_Simple in 1.1.2 and the hotel tile in 1.1.1 — the things that break
# quietly are the ones found by name at runtime, and the cooked bytes are the only
# testimony that counts.
#
# WHAT THIS CHECK CANNOT TELL YOU: how MANY televisions carry the tag. UE stores the
# string once in the map's name table and points at it by index, so its presence proves
# at least one and never two. Walk into the living room AND the bedroom before pushing.
#
# WHAT NOTHING HERE CAN TELL YOU: whether the website is up. The TVs point at a live
# address; if leonardsibelius.com goes down, they open a browser on an error page and the
# package is blameless. Load the URL yourself before a push.
$root    = "C:\Users\wpark\projects\sibelius-game"
$version = "1.1.3"
$outDir  = "C:\Users\wpark\builds\sibelius-v$version"
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-v113.log"

# The pak we are growing from. This release should add ROUGHLY NOTHING — it is all C++ —
# so a jump here means something got dragged into the cook that should not have been.
$baselinePaks = "C:\Users\wpark\builds\sibelius-v1.1.2\Windows\SibeliusGame\Content\Paks"

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

  # --- the enhancement (docs/PROTEIN_MACHINES.md) ---------------------------
  # BOARDING NOW DEPENDS ON THIS, which is what makes these checks load-bearing rather
  # than tidy: ASpaceport::Board and PreflightCompile both refuse without the enhancement
  # grant, so anything that stops the office working stops the game being finishable.
  #
  # The materials reach the pak by LEVEL REFERENCE (L_City places the actor and a level
  # hard-references what it places) — the L_uFoods arrangement, not a directory rule. If
  # reference-following ever changes, the office ships as untextured geometry in a level
  # the player MUST use, and PIE would never say so.
  "the office"          = "$cooked\ProteinMachines\M_OfficeInterior.uasset"
  "the office's navy"   = "$cooked\ProteinMachines\M_OfficeNavy.uasset"
  "the protein display" = "$cooked\ProteinMachines\M_ProteinCyan.uasset"
  "its bonds"           = "$cooked\ProteinMachines\M_ProteinBond.uasset"
  # And her line about it, which arrives by DirectoriesToAlwaysCook on /Game/Audio/Dancers
  # — the v0.7.4 arrangement exactly: found locally by PIE, absent from a shipped build.
  # Without it she stands outside the deli saying nothing at the one beat that explains
  # what the blue ghosts are.
  "her protein speech"  = "$cooked\Audio\Dancers\dancer_protein_nyra.uasset"
  # AND HER MOUTH. Every other Nyra line ships with a baked _face; this one shipped
  # without at first, and the doc that added it claimed "the existing audio-driven mouth
  # motion remains available" — which is not so: bTalkMouthMotion is false because
  # RigLogic replaces the morph array after our weights land. So the bake is the ONLY
  # thing that moves her mouth here, and a missing one is a still face on one line out of
  # five, which reads as broken rather than absent.
  "her protein mouth"   = "$cooked\Audio\Dancers\dancer_protein_nyra_face.uasset"

  # --- the epilogue (docs/SIBELIUS_HOTEL.md) --------------------------------
  # THE HIGHEST-RISK CONTENT IN THE BUILD, and it looks like the lowest.
  #
  # L_SibeliusHotel_Generated is loaded by ULevelStreamingDynamic BY NAME at runtime. A
  # name is a SOFT reference: the cooker does not follow it, so this map reaches the pak
  # ONLY through its MapsToCook line. That is the v0.7.4 invisible-spaceport arrangement
  # exactly — flawless in PIE, absent from the shipped game — and the failure here is
  # silent, because a hotel that does not stream in looks identical to a hillside.
  #
  # The tile is gitignored (it is generated from a purchased pack into a public repo), so
  # a clean checkout has to rebuild it with Tools/Scripts/make_hotel_tile.py BEFORE
  # packaging. This check is what turns "forgot to regenerate" from a shipped bug into a
  # failed package.
  "the hotel"           = "$cooked\Maps\L_SibeliusHotel_Generated.umap"
  # AND ITS ARCHITECTURE. The tile hard-references what it places, so these ride in with
  # the map the way L_uFoods carries the supermarket — but ElvenArchitecture is a
  # gitignored purchased pack, and an uninstalled one would cook a map full of nothing.
  "the hotel's stone"   = "$cooked\ElvenArchitecture\Meshes\SM_Scene.uasset"
  "its grounds"         = "$cooked\ElvenArchitecture\Meshes\SM_Grass.uasset"
  # THE TROPHY. Placed in the hotel tile, so it cooks by the same level-reference route as
  # the architecture — and unlike everything else in that room it is COMMITTED to the repo
  # (CC BY 4.0 permits redistribution with attribution), so a clean checkout has it. The
  # check is here because "the last thing you collect in the game" going missing silently
  # is exactly the class of failure this list exists for.
  "the trophy"          = "$cooked\Trophy\golden_trophy\StaticMeshes\golden_trophy.uasset"

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


# ---- THE TAG, which is the whole feature and cannot be seen with Test-Path ----
# Read the cooked office map and look for the tag by its bytes. The tag is plain ASCII on
# purpose: a name carrying an em-dash or any non-ASCII character would be serialised as
# UTF-16 and this search would find nothing while the game worked fine. Keep it ASCII.
$officeCooked = "$cooked\L_Office_v02.umap"
if (-not (Test-Path $officeCooked)) {
  Write-Output "MISSING the office map -> $officeCooked"
  Write-Output "COOK IS INCOMPLETE. Do NOT push."
  exit 1
}
$officeBytes = [System.IO.File]::ReadAllBytes($officeCooked)
$officeText  = [System.Text.Encoding]::ASCII.GetString($officeBytes)
if ($officeText.Contains("CelestialFortuneWebTV")) {
  Write-Output "OK      the TVs' tag survived the cook"
} else {
  Write-Output "MISSING the TVs' tag -> CelestialFortuneWebTV absent from the cooked L_Office_v02"
  Write-Output "        Every television is dead and nothing will say so. Do NOT push."
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
Write-Output ("pak  v1.1.2   : {0} GB" -f $old)
Write-Output ("pak  v1.1.3   : {0} GB" -f $new)
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
