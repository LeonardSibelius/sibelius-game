# Run all headless smoke-test gates (editor CLOSED) and summarize pass/fail.
$cmd = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$uproject = "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject"
$gates = @(
  "SibeliusSmokeTest","CodeVisionSmokeTest","RefactorSmokeTest","CompileSmokeTest",
  "RefuserSmokeTest","BranchSmokeTest","GenerateSmokeTest","CathedralDoorSmokeTest",
  "CarouselSmokeTest","ElsewhereSmokeTest","SauceSmokeTest","SlotSmokeTest",
  "FinaleSmokeTest","ProgressionSmokeTest","PokerSmokeTest"
)
$results = foreach ($g in $gates) {
  $runArg = "-run=" + $g
  $gArgs = @($uproject, $runArg, "-unattended", "-nopause", "-nosplash", "-stdout")
  & $cmd @gArgs *> ("C:\Users\wpark\projects\sibelius-game\gate-" + $g + ".log")
  [pscustomobject]@{ Gate = $g; Exit = $LASTEXITCODE; Status = $(if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }) }
}
$results | Format-Table -AutoSize
$failed = @($results | Where-Object { $_.Exit -ne 0 }).Count
Write-Output ("FAILED=" + $failed + " of " + $results.Count)
