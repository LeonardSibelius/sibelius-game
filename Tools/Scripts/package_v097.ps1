# Package the v0.9.7 SHIPPING Win64 RELEASE build (butler -> itch).
#
# Content of 0.9.7: the legacy machine reports on itself, and throws a second time.
# The piece now dies AT the broken stage with that stage's fault lamp lit; the housing
# keeps a run log pre-filled with the overnight history; E halts the line and steps it.
# Once the first ticket closes, STAMP starts dropping one piece in three -- a fault you
# cannot confirm you have fixed by watching, so [6] branches and E runs a test batch.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. THE NEW PLATE MATERIAL MUST COOK. M_LabelPlate is brand new this release and is
#    referenced ONLY as a component material override on actors placed in L_Office_v02 --
#    there is no C++ hard reference to it anywhere. That is the same shape as the bug
#    recorded in the soft-reference lesson: PIE reads it off disk and lies. If it does
#    not reach the pak, every label on the machine loses its dark backing and the text
#    goes back to being unreadable over wood and crates.
#
#      Saved\Cooked\Windows\SibeliusGame\Content\SlotFactory\M_LabelPlate.uasset
#
# 2. M_LabelUnlit LIKEWISE (unchanged from 0.9.6, but the placement script deletes and
#    re-duplicates it on every run, so its guid moves). Same folder.
#
# 3. MRS. HALL'S STORY TABLE. Content/Data/MrsHallStory.csv has NO CSV fallback in a
#    packaged build. Ticket.Closed is the line she says when a job closes -- and in
#    0.9.7 she says it TWICE, once per ticket, because the second job has no line of
#    its own yet. If the .uasset is stale she is silent while PIE works.
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Data\MrsHallStory.uasset
#
# 4. THE CREBOTOLY CRATES. SciFiBoxes_A, referenced from placed actors in L_Office_v02.
#
# 5. ELISE MUST REACH THE PAK (as 0.9.6). BP_MHC_Elise placed in L_Office_v02; her dance
#    is in UDancerAgentComponent's CDO hard-refs. No Elise, no COMPILE.
#
# 6. RTP MUST READ 95.567% and SlotSmokeTest 34 PASS / 0 FAIL.
#
# 7. SAVE COMPATIBILITY: a v0.9.6 save loads. Ticket.Legacy.Intermittent.Closed is a new
#    one-time grant an old save has not claimed -- so a player who already closed the
#    first ticket walks back into the living room and finds the SECOND one waiting,
#    which is the intended way to meet it. Nothing was renamed or repurposed.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v097.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.7",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
