# Package the v0.9.1 SHIPPING Win64 RELEASE build (butler -> itch).
#
# Content of 0.9.1: player-facing prompts and feedback SURVIVE SHIPPING. Every
# interaction prompt, pickup, refusal and chapter line in the game went through
# GEngine->AddOnScreenDebugMessage, which the engine compiles out of Shipping builds --
# so none of it has ever reached a downloaded copy. Moved to two HUD canvas channels.
#
# *** THE CHECK THAT MATTERS FOR THIS RELEASE ***
#
# 1. THIS RELEASE CANNOT BE VERIFIED IN PIE. AT ALL.
#
#    That is the whole point of it. In PIE (a Development build) the OLD debug messages
#    render perfectly -- which is exactly why this bug survived since v0.8.1 and why the
#    headless gates cannot see it either: no commandlet draws a HUD.
#
#    The ONLY valid test is running the PACKAGED build and looking:
#
#      C:\Users\wpark\builds\sibelius-v0.9.1\Windows\SibeliusGame.exe
#
#      a. Walk up to the cathedral slot cabinet. A prompt line must appear under the
#         reticle. Before this build, nothing appeared.
#      b. Press Q ONCE. "Press Q again to quit" must appear. Before this build the
#         first press was silent and the second closed the game.
#      c. Collect a curio or fill the sauce bowl -- "+N SAUCE (total M)" must appear.
#      d. Branch with 6, then try the cathedral door. The refusal must be legible.
#
#    If any of those are silent, the HUD channel is not being reached and the fix did
#    not land. Nothing will log the failure -- that is the character of this entire bug.
#
# 2. Still true from 0.9.0: the panel body font must cook. USlotTechPanelWidget
#    hard-references /Engine/EngineFonts/DroidSansMono via a constructor FObjectFinder.
#    Verify BOTH the descriptor AND the face payload -- a UFont carries no glyph data:
#      Saved\Cooked\Windows\Engine\Content\EngineFonts\DroidSansMono.uasset
#      Saved\Cooked\Windows\Engine\Content\EngineFonts\Faces\DroidSansMono.ufont
#
# 3. Still true: RTP must read 95.567% on the factory sheet (34 PASS / 0 FAIL from
#    -run=SlotSmokeTest); the 9 symbol sprites must cook; ParagonGideon and the ten
#    Morro _MH retargets reach the pak only through C++ hard references.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v091.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.1",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
