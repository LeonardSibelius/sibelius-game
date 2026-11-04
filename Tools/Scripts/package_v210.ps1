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
  # His own look borrows its eyes from the Dragonlord skin, the one skin still cooked
  # (DirectoriesToNeverCook in DefaultGame.ini, 2026-10-13).
  # Who R turns an agent into (DirectoriesToAlwaysCook; Echo via APresence), 2026-10-13.
  "stand-in Manny"      = "$cooked\Characters\Mannequins\Meshes\SKM_Manny_Simple.uasset"
  "stand-in Echo"       = "$cooked\EchoContent\Characters\Echo\Meshes\Echo.uasset"
  "stand-in Echo's hair" = "$cooked\EchoContent\Characters\Echo\Meshes\Echo_Hair.uasset"
  "stand-in Aurora"     = "$cooked\ParagonAurora\Characters\Heroes\Aurora\Meshes\Aurora.uasset"
  "stand-in Shinbi"     = "$cooked\ParagonShinbi\Characters\Heroes\Shinbi\Meshes\Shinbi.uasset"
  "stand-in Yin"        = "$cooked\ParagonYin\Characters\Heroes\Yin\Meshes\Yin.uasset"
  "Greystone's eyes"    ="$cooked\ParagonGreystone\Characters\Heroes\Greystone\Skins\Dragonlord\Materials\M_Greystone_Dragonlord_EyeRefractive_Inst.uasset"
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
  # --- THE DANCE MENU (2026-10-07): 44 Mixamo dances, cooked only by DancePaths in
  # DancerAgentComponent.cpp (Content/Mixamo is gitignored). First, last and one between.
  "Mixamo: African Rainbow" = "$cooked\Mixamo\Retargeted\Anim_African_Rainbow_MH.uasset"
  "Mixamo: Hip Hop Shimmy"  = "$cooked\Mixamo\Retargeted\Anim_Hip_Hop_Shimmy_MH.uasset"
  "Mixamo: YMCA"            = "$cooked\Mixamo\Retargeted\Anim_YMCA_MH.uasset"
  # --- THE TRANSHUMAN GRAND PRIX (2026-10-09): L_Kart by one +MapsToCook line; the race kart
  # (Content/Fab/Race_kart, gitignored) and a colour variant only by AKartRace's constructor.
  "the kart race"       = "$cooked\Maps\L_Kart.umap"
  "the race kart"       = "$cooked\Fab\Race_kart\Race_Kart.uasset"
  "a kart's colours"    = "$cooked\Fab\Race_kart\Wheel_Blue_D.uasset"
  # The agents at the wheel (2026-10-09): their pose, only by AKartRace's constructor too.
  "the agents' kart pose" = "$cooked\Mixamo\Driving\Anim_Driving_Kart_MH.uasset"
  "Manny's kart pose"     = "$cooked\Mixamo\Driving\Anim_Driving_Kart_Manny.uasset"
  # --- GO IN THE CHESS PARK (2026-10-10): AGoTable in L_Chess; its board material only by the
  # placed table's SurfaceMaterial, and the grain only by the material.
  "the Go board's wood"   = "$cooked\Go\M_GoSurface.uasset"
  "the Go board's grain"  = "$cooked\Go\T_GoWood.uasset"
  # --- THE THREE-POINT CONTEST (2026-10-15): ABasketballCourt in L_Chess, reached by the living
  # room's BASKETBALL door. The balls' leather only by the court's constructor; the door's sign
  # only by the placed door.
  "the basketballs' leather" = "$cooked\Basketball\T_Basketball.uasset"
  "the BASKETBALL sign"      = "$cooked\Basketball\T_Sign_Basketball.uasset"
  # Leonard is Manny on the court (Walt, 2026-10-15), animated by the template's own AnimBlueprint,
  # which only the court's constructor names.
  "Manny's court animation"  = "$cooked\Characters\Mannequins\Anims\Unarmed\ABP_Unarmed.uasset"
  # The shooters' bodies (2026-10-15): Mixamo's Throw, Dribble, Cheering and Disappointed, on the
  # agents' skeleton and on Manny's (import_mixamo_basketball.py), named by the court's constructor.
  "the agents' throw"        = "$cooked\Mixamo\Basketball\Anim_Throw_MH.uasset"
  "the agents' cheer"        = "$cooked\Mixamo\Basketball\Anim_Cheering_MH.uasset"
  "Manny's throw"            = "$cooked\Mixamo\Basketball\Anim_Throw_Manny.uasset"
  "Manny's shrug"            = "$cooked\Mixamo\Basketball\Anim_Disappointed_Manny.uasset"
  # --- THE AI CITY (2026-10-15): through the portal upstairs (the seed until 2026-10-22), L_Genesis - its star wears
  # Celestial Fortune's sun in layers, loaded by path (AAICity::Paint) and cooked only by the
  # /Game/Genesis DirectoriesToAlwaysCook line.
  "the AI city"              = "$cooked\Maps\L_Genesis.umap"
  "the AI city's glow"       = "$cooked\Genesis\M_GenesisGlow.uasset"
  "the live sun"             = "$cooked\Genesis\M_GenesisSunLive.uasset"
  "the sun's disc"           = "$cooked\Genesis\T_SunDisc.uasset"
  "a prominence"             = "$cooked\Genesis\T_SunLoopNE.uasset"
  "the city's thought"        = "$cooked\Genesis\M_GenesisThought.uasset"
  # The lattice (2026-10-18): its rod and ball, their materials, and the soft-edged sun -
  # all loaded by path (AAICity::LayLattice, Paint, SunDiscMaterial), cooked by /Game/Genesis.
  "the lattice's struts"     = "$cooked\Genesis\SM_GenesisStrut.uasset"
  "the lattice's thought"    = "$cooked\Genesis\M_GenesisStrut.uasset"
  "the lattice's nodes"      = "$cooked\Genesis\M_GenesisNode.uasset"
  "the soft sun"             = "$cooked\Genesis\M_GenesisSunSoft.uasset"
  # The dome (2026-10-22): cglib's Megastructure: Dome 1, its layer blueprints loaded by PATH
  # (AAICity::RaiseDome) and cooked by DefaultGame.ini's /Game/Megastructure_Dome_1/Blueprints
  # line, which pulls their meshes and materials. Missing, the city falls back to the lattice.
  "the dome"                 = "$cooked\Megastructure_Dome_1\Blueprints\BP_Dome1.uasset"
  "the dome's buildings"     = "$cooked\Megastructure_Dome_1\Meshes\Dome1Buildings.uasset"
  "the dome's inner panels"  = "$cooked\Megastructure_Dome_1\Blueprints\Dome1_Modules\BPC_InternalPanels.uasset"
  "the dome's material"      = "$cooked\Megastructure_Dome_1\Materials\M_LayeredMaster.uasset"
  "the run between racks"    = "$cooked\Mixamo\Basketball\Anim_RunInPlace_MH.uasset"
  # --- THE ARCHAIC LANGUAGE TEMPLE (2026-10-19): the hidden door by the poker machine travels to
  # it by name; its five statues load their scans by path (AArchaicStatue::OnConstruction), cooked
  # by /Game/Statues. The door's sign rides /Game/Signs.
  "the Archaic Language Temple" = "$cooked\Maps\L_Archaic_Temple.umap"
  "FORTRAN's Discobolus"     = "$cooked\Statues\SM_Statue_FORTRAN_Discobolus.uasset"
  "Python's Laocoon"         = "$cooked\Statues\SM_Statue_Python_Laocoon.uasset"
  "the statues' marble"      = "$cooked\Statues\M_StatueMarble.uasset"
  "the tablets' bronze"      = "$cooked\Statues\M_StatueTablet.uasset"
  "the temple door's sign"   = "$cooked\Signs\T_Sign_ArchaicTemple.uasset"
  # --- THE CATHEDRAL CABINET (2026-10-16): the Blender-built Celestial Fortune machine on the
  # apse ASlotCabinet, cooked because L_Cathedral references it. Its mesh and its lit marquee.
  "the cabinet"              = "$cooked\SlotFactory\Cabinet\SM_CelestialCabinet.uasset"
  "the cabinet's marquee"    = "$cooked\SlotFactory\Cabinet\M_CabMarqueeLit.uasset"
  # --- FREAKBOOK ON THE LAPTOP (2026-10-12): its pictures, loaded by name by UFreakbookWidget and
  # cooked only by the /Game/Freakbook DirectoriesToAlwaysCook line. A profile picture and a post.
  "Freakbook's faces"     = "$cooked\Freakbook\T_FB_dee_avatar.uasset"
  "Freakbook's photos"    = "$cooked\Freakbook\T_FB_jaxx_banner.uasset"
  # --- ISLA'S HOME (2026-10-05): the same way in - Fafnir's Keep since 2026-10-24, the sci-fi
  # kit's station before it. World Partition: the cook folds its actors into the map's cells.
  "Isla's home"         = "$cooked\Maps\L_IslaKeep.umap"
  # The keep's own pieces (Fafnir_Keep_Meshingun, gitignored): a missing pack cooks an empty map.
  "the keep's dragons"  = "$cooked\Fafnir_Keep_Meshingun\Meshes\SM_Dragon_a_01.uasset"
  "Isla herself"        = "$cooked\MetaHumans\MHC_IslaRowan\BP_MHC_IslaRowan.uasset"
  "Isla's dance"        = "$cooked\Characters\Retargeting\Anim_High_Rhythm_Dance_13_MH.uasset"
  # Which plants come alive: found by an asset-registry scan, cooked by the EDFoliageMap
  # PrimaryAssetTypesToScan rule in DefaultGame.ini (AlwaysCook) - no reference reaches it.
  "the foliage map"     = "$cooked\AlienJunglePlants\FoliageTypes\FoliageMap_AlienJunglePlants.uasset"
  # F on her placed plants (AVirtualHome::MushroomSound / PlantSound).
  "the boing"           = "$cooked\AlienPhysicsWorld\Sounds\Foliage\Balloon\SC_Balloon.uasset"
  "the rustle"          = "$cooked\AlienPhysicsWorld\Sounds\Foliage\Rustling\SC_Foliage_Rustle.uasset"
  # --- LANTERN BURST IN KAIA'S JUNGLE (2026-10-27): every orb, spark and ember is one sphere in
  # M_LanternOrb, coloured per instance - named only by ALanternBurst's constructor and the placed
  # lantern. Missing, the lantern draws grey balls.
  "Lantern Burst's glass" = "$cooked\LanternBurst\M_LanternOrb.uasset"
  # --- THE DRAGON'S WAGER IN ISLA'S KEEP (2026-10-28): named only by ADragonsWager's constructor.
  # Missing, the crown and jewels fall back to engine cones and spheres, the caught player stays
  # unglazed, and the pedestal becomes a grey cylinder.
  "the wager's crown"    = "$cooked\DragonsWager\SM_WagerCrown.uasset"
  "the wager's jewels"   = "$cooked\DragonsWager\SM_WagerGem.uasset"
  "the wager's gold"     = "$cooked\DragonsWager\M_WagerGold.uasset"
  "the jewels' colours"  = "$cooked\DragonsWager\M_WagerGem.uasset"
  "the wager's pedestal" = "$cooked\Fafnir_Keep_Meshingun\Meshes\SM_pedestal.uasset"
  # --- THE GIST (2026-10-29): named only by AGistSite's constructor. Missing, every gist is
  # drawn in the engine's grey checker instead of coloured clay.
  "the gist's clay"      = "$cooked\Gist\M_GistClay.uasset"
}
# --- 2.1.0: THE TREASURE - every piece URefactorComponent names -----------
# Since 2026-10-13 the gems are GemShader's, each in one of six colours, and all that is left
# of Dekogon's pack is its two chalices.
foreach ($name in "SM_Gem_01","SM_Gem_02","SM_Gem_03") {
  $checks["gem $name"] = "$cooked\GemShader\Meshes\$name.uasset"
}
foreach ($name in "MI_Gem_Blue","MI_Gem_Green","MI_Gem_Mixed","MI_Gem_Pink","MI_Gem_Red","MI_Gem_White") {
  $checks["gem colour $name"] = "$cooked\GemShader\Materials\$name.uasset"
}
foreach ($name in "SM_Chalice_01a","SM_Chalice_02b") {
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
# And what must NOT ship: three of Greystone's skins, 226 MB nobody sees (DefaultGame.ini).
foreach ($skin in "Novaborn","Tough","WhiteTiger") {
  $dir = "$cooked\ParagonGreystone\Characters\Heroes\Greystone\Skins\$skin"
  if (Test-Path $dir) { Write-Output "NOTE    Greystone's $skin skin was cooked anyway - check DirectoriesToNeverCook" }
  else { Write-Output "OK      Greystone's $skin skin left out" }
}

# ---- LEONARD'S ARCADE (2026-10-23): the TVs play the games players make with one prompt ----
# The TVs are found by their actor tag, which is map data: a re-saved L_Office_v02 that lost it
# would leave both TVs dead and nothing would say so (package_v113.ps1 learned this).
$officeText = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes("$cooked\L_Office_v02.umap"))
if ($officeText.Contains("CelestialFortuneWebTV")) {
  Write-Output "OK      the TVs' tag survived the cook"
} else {
  Write-Output "MISSING the TVs' tag -> CelestialFortuneWebTV absent from the cooked L_Office_v02"
  Write-Output "        Both TVs are dead and nothing will say so. Do NOT push."
  exit 1
}
# And Chromium, which plays them. From v1.1.3 to 2.3.3 its ABSENCE was checked, to keep 334 MB
# out of a build that never used it; the arcade does, so now a build without it is the failure.
$cefDll = Get-ChildItem $outDir -Recurse -File -Filter "libcef.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
$helper = Get-ChildItem $outDir -Recurse -File -Filter "EpicWebHelper.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($cefDll -and $helper) {
  $cefMB = [math]::Round((Get-ChildItem $cefDll.DirectoryName -Recurse -File | Measure-Object Length -Sum).Sum / 1MB, 0)
  Write-Output "OK      Chromium for the arcade ($cefMB MB) and its helper"
} else {
  Write-Output "MISSING Chromium -> libcef.dll or EpicWebHelper.exe absent from $outDir"
  Write-Output "        The TVs would open the arcade and play nothing. Do NOT push."
  exit 1
}
# The arcade's own games (Content\Arcade), staged LOOSE by DirectoriesToAlwaysStageAsNonUFS -
# Chromium cannot read a .pak - and the glowing material the TVs' "LEONARD'S ARCADE" screens are
# drawn with, which only C++ names (UArcadeTVSubsystem, by path; DirectoriesToAlwaysCook cooks it).
$samples = @(Get-ChildItem "$outDir\Windows\SibeliusGame\Content\Arcade" -Filter "*.html" -ErrorAction SilentlyContinue)
$want = @(Get-ChildItem "$root\Content\Arcade" -Filter "*.html" -ErrorAction SilentlyContinue).Count
if ($want -gt 0 -and $samples.Count -eq $want) {
  Write-Output "OK      the arcade's $want sample games, loose"
} else {
  Write-Output "MISSING sample games -> $($samples.Count) of $want staged in Content\Arcade"
  exit 1
}
$tvMat = "$cooked\ArcadeTV\M_ArcadeTVScreen.uasset"
if (Test-Path $tvMat) { Write-Output "OK      the TVs' screen material" }
else { Write-Output "MISSING the TVs' screen material -> $tvMat"; exit 1 }
# THE GIST's words (2026-10-29): read as loose files from Content\Data, staged by the "Data"
# DirectoriesToAlwaysStageAsNonUFS rule. Missing, every wish builds a nameless contraption.
foreach ($words in "GistNouns.csv", "GistModifiers.csv") {
  $staged = "$outDir\Windows\SibeliusGame\Content\Data\$words"
  if ((Test-Path $staged) -and ((Get-Item $staged).Length -eq (Get-Item "$root\Content\Data\$words").Length)) {
    Write-Output "OK      the gist's $words, loose"
  } else {
    Write-Output "MISSING the gist's words -> $staged"
    exit 1
  }
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
