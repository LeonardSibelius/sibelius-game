# Package the v2.1.0 SHIPPING Win64 build, CHAT ON - "Kaia's home".
#
# Content of 2.1.0, on top of the 2026-09-28 chat build (chess park, the living room's games):
#   - Kaia's virtual home: the portal at the end of the hall (C, once Compile is earned) to
#     L_KaiaHome, Etherion's alien jungle, where he is Greystone and Refactor is lent.
#   - Refactor makes TREASURE instead of animals, for 10 sauce: Dekogon's gems, the gold coin
#     and gold pieces.
#   - Kaia's hair stops showing the landscape through it; the DAILY GRIND sign.
#
# CHAT IS ON, as every build now is (Walt, 2026-09-29: "no chat-off builds"). The two Shipping
# defaults are flipped for the package and PUT BACK afterwards whatever happens - the repo keeps
# them 0 until Valve accepts the content-survey amendment (ticket HT-65HH-2V43-8C45). And this
# build is NOT for Steam until then: the upload script refuses without -AmendmentAccepted.
#
# NEW THINGS THAT CAN GO MISSING QUIETLY, all from gitignored purchased packs:
#   - L_KaiaHome reaches the pak through ONE +MapsToCook line; the jungle rides on its references.
#   - The treasure is hard-referenced from URefactorComponent's constructor (no directory rule),
#     so each piece is checked by name - a missing one would make R fall back to fewer kinds.
#   - The portal's effect, NS_TeleporterHole, as for the spaceport.
#
# The 2.1.x builds that follow use it too (2.1.1, 2026-10-04: first person in her home, her
# plants alive and slappable): pass -Version, and -Baseline for the build to measure against.
# A second build on the same day needs -OutDir (2.1.3, 2026-10-05, Isla's home: 2.1.2 was that
# morning's, in the folder the date names) - and an existing folder is refused, never overwritten.
param(
  [string] $Version  = "2.1.0",
  [string] $Baseline = "C:\Users\wpark\builds\sibelius-shipping-chat-20260928",
  [string] $OutDir   = "C:\Users\wpark\builds\sibelius-shipping-chat-$(Get-Date -Format yyyyMMdd)"
)
$root    = "C:\Users\wpark\projects\sibelius-game"
$outDir  = $OutDir
if (Test-Path $outDir) {
  Write-Output "REFUSING: $outDir already holds a build - pass -OutDir for a new folder."
  exit 1
}
$uat     = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log     = "$root\pkg-v210.log"
$baselinePaks = "$Baseline\Windows\SibeliusGame\Content\Paks"

# ---- chat ON for this package only ------------------------------------------
# Each default is the line after "#if UE_BUILD_SHIPPING" in its cvar's declaration.
$flips = @(
  @{ File = "$root\Source\SibeliusGame\InteractorComponent.cpp"; Cvar = 'TEXT("sib.Chat.Enable")'; Off = '0'; On = '1' },
  @{ File = "$root\Source\SibeliusGame\NpcChatSubsystem.cpp";    Cvar = 'TEXT("sib.Chat.Mode")';   Off = '0'; On = '2' }
)
function Set-ChatDefault($flip, $from, $to) {
  $text = [IO.File]::ReadAllText($flip.File)
  $pattern = '(' + [regex]::Escape($flip.Cvar) + ',\s*\r?\n#if UE_BUILD_SHIPPING\r?\n\s*)' + $from + ','
  if ([regex]::Matches($text, $pattern).Count -ne 1) {
    throw "Could not find the Shipping default of $($flip.Cvar) set to $from in $($flip.File)"
  }
  [IO.File]::WriteAllText($flip.File, [regex]::Replace($text, $pattern, '${1}' + $to + ','), (New-Object Text.UTF8Encoding($false)))
}

if (Test-Path $log) { Remove-Item $log -Force }
$code = 1
try {
  foreach ($f in $flips) { Set-ChatDefault $f $f.Off $f.On }
  Write-Output "CHAT ON for this package: sib.Chat.Enable=1, sib.Chat.Mode=2 (Shipping defaults)"

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
}
finally {
  # ALWAYS put them back, then prove it: the two files must match the commit again.
  foreach ($f in $flips) {
    try { Set-ChatDefault $f $f.On $f.Off } catch { Write-Output "RESTORE: $($_.Exception.Message)" }
  }
  Push-Location $root
  git diff --quiet -- Source/SibeliusGame/InteractorComponent.cpp Source/SibeliusGame/NpcChatSubsystem.cpp
  $clean = $LASTEXITCODE -eq 0
  Pop-Location
  if ($clean) { Write-Output "Chat defaults restored to 0 in the repo." }
  else { Write-Output "WARNING: the chat default files differ from the commit - check them before committing anything." }
}

Write-Output "EXITCODE=$code"
if ($code -ne 0) {
  Write-Output "PACKAGE FAILED - see $log"
  exit $code
}

# ---- verify the things this release cannot ship without ----------------------
$cooked = "$root\Saved\Cooked\Windows\SibeliusGame\Content"
$checks = [ordered]@{
  # --- carried forward: the story, the city, the ending, the living room ---
  "the office"          = "$cooked\L_Office_v02.umap"
  "the city"            = "$cooked\Maps\L_City.umap"
  "the planet Grok"     = "$cooked\Maps\L_Grok.umap"
  "the bowling alley"   = "$cooked\Bowling\L_Bowling.umap"
  "the chess park"      = "$cooked\Maps\L_Chess.umap"
  "Greystone mesh"      = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Meshes\Greystone.uasset"
  "Mrs. Hall's lines"   = "$cooked\Data\MrsHallStory.uasset"
  "Kaia's power face"   = "$cooked\Audio\Dancers\dancer_power_kaia_face.uasset"

  # --- 2.1.0: KAIA'S HOME ---------------------------------------------------
  "Kaia's home"         = "$cooked\Maps\L_KaiaHome.umap"
  "the jungle's look"   = "$cooked\AlienJunglePlants\Materials\MPC_AlienJungleAppearance.uasset"
  "the way there"       = "$cooked\PortalVFX\NS\NS_TeleporterHole.uasset"
  "Kaia herself"        = "$cooked\MetaHumans\MHC_Kaia\BP_Dancer_Kaia.uasset"
  # Reached only through the VirtualHome actor's FoliageInfluencer property: without it her
  # mushrooms do not answer him (Walt, 2026-10-03).
  "the living plants"   = "$cooked\AlienPhysicsWorld\FoliageSystem\BP_FoliageInfluencer.uasset"
  # --- 2.1.2: ELISE'S AND AISLING'S HOMES (2026-10-05) - one +MapsToCook line each, like Kaia's;
  # the agents are plain MetaHumans set dancing by the level, so the dances ride on it too.
  "Elise's home"        = "$cooked\Maps\L_EliseHome.umap"
  "Aisling's home"      = "$cooked\Maps\L_AislingHome.umap"
  "Elise herself"       = "$cooked\MetaHumans\MHC_Elise\BP_MHC_Elise.uasset"
  "Aisling herself"     = "$cooked\MetaHumans\MHC_Aisling\BP_MHC_Aisling.uasset"
  "Elise's dance"       = "$cooked\Characters\Retargeting\Anim_Slow_Rhythm_Dance_12_MH.uasset"
  "Aisling's dance"     = "$cooked\Characters\Retargeting\Anim_Mid_Rhythm_Dance_10_MH.uasset"
  # --- ISLA'S HOME (2026-10-05): the sci-fi kit's station, the same way in.
  "Isla's home"         = "$cooked\Maps\L_IslaHome.umap"
  "Isla herself"        = "$cooked\MetaHumans\MHC_IslaRowan\BP_MHC_IslaRowan.uasset"
  "Isla's dance"        = "$cooked\Characters\Retargeting\Anim_High_Rhythm_Dance_13_MH.uasset"
  # Which plants come alive: found by an asset-registry scan, cooked by the EDFoliageMap
  # PrimaryAssetTypesToScan rule in DefaultGame.ini (AlwaysCook) - no reference reaches it.
  "the foliage map"     = "$cooked\AlienJunglePlants\FoliageTypes\FoliageMap_AlienJunglePlants.uasset"
  # F on her placed plants (AVirtualHome::MushroomSound / PlantSound).
  "the boing"           = "$cooked\AlienPhysicsWorld\Sounds\Foliage\Balloon\SC_Balloon.uasset"
  "the rustle"          = "$cooked\AlienPhysicsWorld\Sounds\Foliage\Rustling\SC_Foliage_Rustle.uasset"
}
# --- 2.1.0: THE TREASURE - every piece URefactorComponent names -----------
foreach ($name in "SM_Gems_01a","SM_Gems_01b","SM_Gems_01c","SM_Gems_01d","SM_Coin_01a",
                  "SM_Chalice_01a","SM_Chalice_02b","SM_GoldBar_01a","SM_OrnateChest_02d",
                  "SM_Chest_02c","SM_ChestCoins_01a") {
  $checks["treasure $name"] = "$cooked\AncientTreasures\Meshes\$name.uasset"
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

# ---- what did Kaia's planet cost? -------------------------------------------
function PakGB($path) {
  if (-not (Test-Path $path)) { return $null }
  [math]::Round((Get-ChildItem $path -Recurse -File | Measure-Object Length -Sum).Sum / 1GB, 2)
}
$new = PakGB "$outDir\Windows\SibeliusGame\Content\Paks"
$old = PakGB $baselinePaks
$archive = [math]::Round((Get-ChildItem $outDir -Recurse -File | Measure-Object Length -Sum).Sum / 1GB, 2)

Write-Output ""
Write-Output "---------------- SIZE ----------------"
Write-Output ("pak  {0} : {1} GB" -f (Split-Path $Baseline -Leaf), $old)
Write-Output ("pak  v$Version : {0} GB" -f $new)
if ($old -and $new) { Write-Output ("growth : {0} GB" -f [math]::Round($new - $old, 2)) }
Write-Output ("whole archive      : {0} GB" -f $archive)
Write-Output "--------------------------------------"

Write-Output "COOK VERIFIED. Archive: $outDir"
Write-Output "NOT FOR STEAM until Valve accepts the chat amendment."
