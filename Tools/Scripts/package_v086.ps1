# Package the v0.8.6 SHIPPING Win64 RELEASE build (butler -> itch).
# Content of 0.8.6: the FinaleAltar summons a dancer when the Synthesis
# completes (L_Cathedral), plus brighter key lights on the AI Temple pair.
#
# Cook note: the dancer reaches the pak through the LEVEL -> the FinaleAltar
# actor's DancerClass -> BP_MHC_* -> meshes. That is a hard reference chain, the
# same one the office dancers use. If DancerClass were a soft path in C++ it
# would work in PIE and be MISSING from the shipped pak - verify after cooking.
#
# Watch item: L_Cathedral now pulls a MetaHuman in. Compare the archive against
# 0.8.5's 5.49 GB - the shared Common/ folder is already cooked for L_Office_v02,
# so expect a small rise (one character's meshes/textures), not another 600 MB.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v086.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.8.6",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
