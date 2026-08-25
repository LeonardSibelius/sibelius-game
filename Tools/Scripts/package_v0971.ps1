# Package the v0.9.7.1 SHIPPING Win64 build (experiment 1 of 7 — sauce is a fluid).
#
# Content of 0.9.7.1: Niagara Fluids on the kitchen cauldron (simmer scales with
# the sauce wallet; shop-open and a blend make it boil) and the temple bowl
# (E pours a 3D FLIP hose; the filled surface ripples until C compiles it).
# Escape hatch in-game: `sib.SauceFluids 0`.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. NIAGARAFLUIDS PLUGIN MUST BE ENABLED. SibeliusGame.uproject lists it.
#    If the cook log says it skipped NiagaraFluids content, the stove is a
#    dark pot again.
#
# 2. THE THREE PLUGIN TEMPLATES MUST COOK. They are C++ FObjectFinder hard
#    refs, so they should land even without /Game/Sauce copies:
#
#      Saved\Cooked\Windows\Engine\Plugins\FX\NiagaraFluids\Content\Templates\Gas\3D\Systems\Grid3D_Gas_ColoredSmoke.uasset
#      Saved\Cooked\Windows\Engine\Plugins\FX\NiagaraFluids\Content\Templates\Liquid\2D\Systems\Grid2D_FLIP_Hose.uasset
#      Saved\Cooked\Windows\Engine\Plugins\FX\NiagaraFluids\Content\Templates\Liquid\2D\Systems\ShallowWater\Grid2D_SW_Pool.uasset
#
# 3. OPTIONAL GAME COPIES + SURFACE. If build_sauce_fluids.py was run:
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Sauce\NS_SauceSimmer.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Sauce\NS_SaucePour.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Sauce\NS_SaucePool.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Sauce\M_SauceSurface.uasset
#
#    /Game/Sauce is in DirectoriesToAlwaysCook. A missing copy is not a ship
#    blocker — the plugin originals are.
#
# 4. SauceSmokeTest must PASS (now covers fluids + bowl pour/claim).
#
# 5. PIE: kitchen stove steams; E on the shop makes it boil; temple E pours
#    a living stream, C claims. If the 3D grid is too big, nudge SauceFluid
#    SimmerScale / PourScale on the placed actor. `sib.SauceFluids 0` if the
#    box cannot hold a 3D gas sim.
#
# 6. SAVE COMPATIBILITY: no new save fields. v0.9.7 loads.

$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v0971.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.7.1",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
