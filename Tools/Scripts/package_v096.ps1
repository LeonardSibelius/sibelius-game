# Package the v0.9.6 SHIPPING Win64 RELEASE build (butler -> itch).
#
# Content of 0.9.6: the First Ticket. The living-room machine is the opening job.
# Hold V, take Refactor from Kaia, R the lying GRADER, watch ACCEPT. Elise in the
# bedroom hands over COMPILE; the library alcove orb is the attic key.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. MRS. HALL'S STORY TABLE MUST COOK -- AND IT NOW HAS THE TICKET.CLOSED ROW.
#
#    Content/Data/MrsHallStory.csv has NO CSV fallback in a packaged build.
#    Ticket.Closed is the line she says when the first piece lands in ACCEPT.
#    If the .uasset is stale she will stay silent after the job while PIE works.
#    GenerateSmokeTest asserts Ticket + Ticket.Closed + all six Power.* + Final.
#
#      Saved\Cooked\Windows\SibeliusGame\Content\Data\MrsHallStory.uasset
#
# 2. THE LEGACY MACHINE MUST COOK. Crebotoly crates (SciFiBoxes_A) and
#    M_LabelUnlit are referenced from placed actors in L_Office_v02. If the
#    labels fail to cook, the plaques wash out and the one-day test is unreadable.
#
# 3. ELISE MUST REACH THE PAK. BP_MHC_Elise is placed in L_Office_v02. Her dance
#    Anim_Mid_Rhythm_Dance_10_MH is in UDancerAgentComponent's CDO hard-refs.
#    If it does not cook, the scan never adopts her and COMPILE is unreachable
#    (the shrine is stood down at her feet).
#
# 4. THE PANEL BODY FONT (from 0.9.0). DroidSansMono font + face, as 0.9.5.
#
# 5. RTP MUST READ 95.567% and SlotSmokeTest 34 PASS / 0 FAIL.
#
# 6. SAVE COMPATIBILITY: a v0.9.5 save should load. Ticket.Legacy.Closed is a new
#    one-time grant; an old save simply has not claimed it, so a returning player
#    still has the living-room job unless they already refactored GRADER this
#    session. Hall.FirstUse.* keys are unchanged.
$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v096.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.6",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
