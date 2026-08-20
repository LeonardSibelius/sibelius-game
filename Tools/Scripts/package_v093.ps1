# Package the v0.9.3 SHIPPING Win64 RELEASE build (butler -> itch).
#
# Content of 0.9.3: living-room spawn, Vision then poker, 50 starting sauce,
# table book, poker deck visual, HUD chips, [O] after poker, Many Worlds
# out of the journal. EasyBiomes is NOT in MapsToCook / DirectoriesToAlwaysCook.
#
# After EXITCODE=0:
#   & "C:\Users\wpark\butler\butler.exe" push `
#     "C:\Users\wpark\builds\sibelius-v0.9.3\Windows" `
#     leonardsibelius/leonard-sibelius:windows --userversion 0.9.3
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v093.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.3",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
