# Package the v0.9.7.4 SHIPPING Win64 build — "Kaia opens the game".
#
# Content of 0.9.7.4: the game boots into a talking cutscene. Kaia introduces
# herself by name against black, lip-synced from her own ElevenLabs recording,
# then it travels to the office.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. THE BOOT MAP MUST COOK. GameDefaultMap is now
#    /Game/Cinematics/L_Cine_KaiaIntro. A boot map missing from the pak is a
#    game that opens on nothing, and it will not show up in PIE. Required:
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Cinematics\L_Cine_KaiaIntro.umap
#
# 2. THE CUTSCENE'S THREE PIECES. The Level Sequence hard-references the face
#    animation and the voice, so these should follow it into the pak - but the
#    voice lives under /Game/Audio/Cinematics, which is gitignored AI audio and
#    is NOT in DirectoriesToAlwaysCook. It rides on the sequence reference
#    alone. Verify, do not assume:
#
#      ...\Content\Cinematics\LS_Kaia_Intro.uasset
#      ...\Content\Cinematics\AS_MHP_Kaia_Intro_Face.uasset
#      ...\Content\Audio\Cinematics\kaia_intro.uasset
#
#    Missing the audio = she mouths silently. Missing the animation = she
#    stares. Neither crashes, so neither announces itself.
#
# 3. THE FIVE DANCER VOICES still cook via DirectoriesToAlwaysCook
#    (/Game/Audio/Dancers) - the 0.9.7.3 check, still worth a glance.
#
# 4. FIRST TEN SECONDS OF THE BUILD. Run the packaged exe: Kaia's face, her
#    voice, then the office. That is the whole release, and PIE cannot test it
#    because PIE plays the open level and ignores GameDefaultMap.
#
# 5. SYNTHETIC-VOICE LICENCE. AI speech ships in this build. Walt confirmed a
#    paid ElevenLabs plan (2026-08-25).
#
# 6. SAVE COMPATIBILITY: no new save fields. PlayedVideoCues is session-only.

$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v0974.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.7.4",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
