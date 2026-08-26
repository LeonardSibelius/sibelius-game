# Package the v0.9.7.6 SHIPPING Win64 build — "the pot leaves the kitchen".
#
# Content of 0.9.7.6: no new content. One fix — SauceCauldron no longer decides
# whether to show its pot by reading its own actor label. Labels are editor-only
# data and the cook strips them, so the test was always false in a packaged
# build and every cauldron loaded a pot. The kitchen actor sits at Z=170 to
# wrap the stove, so the pot arrived floating at counter height.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. THE KITCHEN, IN THE PACKAGED EXE — NOT IN PIE. This bug was invisible in
#    the editor for three releases. PIE keeps the actor label and hid the pot
#    correctly every single time. The ONLY way to verify this fix is to run the
#    shipped exe, walk to the kitchen, and look. "Blend the Sauce [E]" must
#    still prompt off the stove with nothing hanging in the air.
#
# 2. THE TEMPLE CAULDRON MUST KEEP ITS POT. The fix deletes the branch that
#    used to LoadObject() one at runtime; the pot now comes from SM_Pot saved
#    on the instance in L_AI_Temple. Verified in the map binary, but the temple
#    is a live level - go and look at it, and press E on it. The mesh has to
#    block the camera trace or the shop never opens.
#
# 3. THE BOOT MAP MUST STILL COOK. GameDefaultMap is
#    /Game/Cinematics/L_Cine_KaiaIntro. A boot map missing from the pak is a
#    game that opens on nothing, and PIE cannot show it. Required:
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Cinematics\L_Cine_KaiaIntro.umap
#
# 4. THE CUTSCENE'S THREE PIECES, carried over from 0.9.7.4/.5. The voice under
#    /Game/Audio/Cinematics is gitignored AI audio and is NOT in
#    DirectoriesToAlwaysCook - it rides on the sequence's hard reference alone:
#
#      ...\Content\Cinematics\LS_Kaia_Intro.uasset
#      ...\Content\Cinematics\AS_MHP_Kaia_Intro_Face.uasset
#      ...\Content\Audio\Cinematics\kaia_intro.uasset
#
#    Missing the audio = she mouths silently. Missing the animation = she
#    stares. Neither crashes, so neither announces itself.
#
# 5. THE FIVE DANCER VOICES still cook via DirectoriesToAlwaysCook
#    (/Game/Audio/Dancers) - the 0.9.7.3 check, still worth a glance.
#
# 6. SYNTHETIC-VOICE LICENCE. AI speech ships in this build. Walt confirmed a
#    paid ElevenLabs plan (2026-08-25).
#
# 7. SAVE COMPATIBILITY: no new save fields.

$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v0976.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.7.6",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
