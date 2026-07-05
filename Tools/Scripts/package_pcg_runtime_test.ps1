# DE-RISK: does the EasyBiomes PCG forest generate at RUNTIME in a cooked build?
# Cooks ONLY L_Elsewhere_Dev to a scratch dir, Development config.
# Uses -Map= so the real shipping MapsToCook / DefaultGame.ini is NOT touched.
# Throwaway — delete the build folder + this script once the question is answered.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-pcg-runtime-test.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-Map=/Game/Maps/L_Elsewhere_Dev",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-pcg-runtime-test",
  "-clientconfig=Development"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
