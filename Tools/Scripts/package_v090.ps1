# Package the v0.9.0 SHIPPING Win64 RELEASE build (butler -> itch).
#
# Content of 0.9.0: THE FLOOR REPORT. The cathedral machine now counts what it actually
# did (session + lifetime meters), compares it against par, and states the range of
# returns that count as normal for the number of spins played. Plus the help page section
# that explains all of it, and a monospace panel body so the columns line up.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. THE PANEL BODY FONT MUST COOK.  <-- NEW, AND THE ONE MOST LIKELY TO BITE
#    USlotTechPanelWidget's constructor hard-references
#
#      /Engine/EngineFonts/DroidSansMono
#
#    via FObjectFinder, because the meters page is columns of figures and only aligns in
#    a monospace face. SlotSmokeTest asserts the path RESOLVES, but the editor sees the
#    whole disk - that is not the same as staging. Verify after the cook:
#
#      Saved\Cooked\Windows\Engine\Content\EngineFonts\DroidSansMono.uasset
#
#    A missing font FAILS SILENTLY: the text block falls back to the proportional default,
#    every column goes ragged, and nothing logs. Same class of trap as ParagonGideon's
#    montage (docs/VENDOR_PACKS.md) - a scan is not a reference, and neither is a path.
#
# 2. LIFETIME METERS ARE A NEW SAVED FIELD.
#    FProgressionState gained SlotLifetimeMeters (a nested FSlotMeters USTRUCT). It is
#    ADDITIVE - old saves default-fill to zeroes, which reads correctly as "never played" -
#    but a v0.8.9 save should be loaded once against this build to confirm it does not
#    wipe sauce, powers or the par-sheet dials. ProgressionSmokeTest covers the round trip
#    headlessly; the real save is the one players have.
#
# 3. RTP MUST STILL READ 95.567% ON THE FACTORY SHEET.
#    Nothing in 0.9.0 was meant to touch the maths. If a packaged run disagrees with the
#    gate, something did not make it into the pak. Expect 34 PASS / 0 FAIL:
#      UnrealEditor-Cmd.exe <project> -run=SlotSmokeTest -unattended -nopause -nosplash
#
# 4. Still true from earlier releases: the 9 symbol sprites under
#    Content\SlotFactory\SymbolSprites must cook (the cathedral machine runs the NATIVE
#    screen since 0.8.9, so they are load-bearing), and ParagonGideon + the ten Morro
#    _MH dance retargets reach the pak ONLY through C++ hard references. Both folders are
#    gitignored and in neither MapsToCook nor DirectoriesToAlwaysCook.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v090.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.0",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
