# Package the v0.8.9 SHIPPING Win64 RELEASE build (butler -> itch).
# Content of 0.8.9: the technician's panel on the cathedral slot machine (four dials,
# live par sheet report, help page, licence band), the par sheet saved between sessions,
# the wild best-reading payout fix, and the cleared cathedral apse.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. THE CATHEDRAL MACHINE IS NOW THE NATIVE SCREEN, NOT THE WEB GAME.
#    ASlotCabinet::bUseWebScreen flipped to false this release, so the cabinet runs
#    USlotScreenWidget + USlotGameModel instead of Chromium. That means the NATIVE
#    screen's assets are load-bearing for the first time in a shipped build:
#
#      Saved\Cooked\Windows\SibeliusGame\Content\SlotFactory\SymbolSprites\T_sym_*
#
#    SlotSmokeTest asserts all 9 resolve in the editor, but the editor sees the whole
#    disk. VERIFY THEY COOKED. A missing sprite is a blank reel in the shipped game and
#    nothing logs.
#
# 2. RTP MUST READ 95.567%.
#    The wild fix changed the maths deliberately (95.432 -> 95.567). If a packaged run
#    disagrees with the gate, something did not make it into the pak. Run before pushing:
#      UnrealEditor-Cmd.exe <project> -run=SlotSmokeTest -unattended -nopause -nosplash
#    Expect 27 PASS / 0 FAIL.
#
# 3. Still true from earlier releases: ParagonGideon reaches the pak ONLY through C++
#    FObjectFinder hard references (SlapComponent Death_Back, RefuserController's attack
#    montage), and Content\Characters\Retargeting\*_MH.uasset - the dancers' ten Morro
#    routines - likewise. Both folders are gitignored and in neither MapsToCook nor
#    DirectoriesToAlwaysCook. See docs/VENDOR_PACKS.md.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v089.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.9",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
