# Package the v0.9.7.3 SHIPPING Win64 build — "The agents speak".
#
# Content of 0.9.7.3: every AI agent introduces herself by name in her own
# ElevenLabs voice during the talk close-up, the HUD blanks for the shot, the
# portrait holds still, and the camera finally tracks her face.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. THE SIX VOICE CLIPS MUST COOK. They are gitignored AI audio reached only
#    through DirectoriesToAlwaysCook=/Game/Audio/Dancers in DefaultGame.ini —
#    a soft LoadObject path, NOT a hard reference. This is exactly the v0.7.4
#    soft-ref miss (works in PIE, missing from the pak). After the cook, all
#    six of these must exist:
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Audio\Dancers\dancer_power.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Audio\Dancers\dancer_power_kaia.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Audio\Dancers\dancer_power_nyra.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Audio\Dancers\dancer_power_isla.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Audio\Dancers\dancer_power_aisling.uasset
#      Saved\Cooked\Windows\SibeliusGame\Content\Audio\Dancers\dancer_power_elise.uasset
#
#    Missing = the agents mime. The game does not crash (the component logs a
#    warning and runs the shot silent), so this WILL ship broken if unchecked.
#
# 2. SYNTHETIC-VOICE LICENCE. AI-generated speech is going out on a public
#    build. Clear the ElevenLabs commercial terms before the push, per the
#    standing note in .gitignore.
#
# 3. IN-GAME: E on a dancer with no power left → HUD vanishes, camera lands on
#    her face and STAYS there, she says her own name. Five different voices
#    across Kaia / Nyra / Isla / Aisling / Elise.
#
# 4. SAVE COMPATIBILITY: no new save fields. v0.9.7 loads.

$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v0973.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.7.3",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
