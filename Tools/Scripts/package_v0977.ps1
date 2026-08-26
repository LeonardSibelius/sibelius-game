# Package the v0.9.7.7 SHIPPING Win64 build — "Mrs. Hall's Crap Factory".
#
# Content of 0.9.7.7: the living room stops playing Mrs. Hall straight. Her legacy
# line has a defaced sign over it, an overflowing reject crate, and — once the first
# ticket is closed — R turns any stage into an animal while the line keeps running.
#
# *** THE CHECKS THAT MATTER FOR THIS RELEASE ***
#
# 1. YOU CAN STILL GET OUT OF THE LIVING ROOM. Read that twice. During this release
#    the whole machine was rotated 25 degrees to bring the bins into view, and it
#    blocked the player's way out of the room and put the bins inside the couch. It
#    was reverted (YAW back to 0 in build_legacy_machine.py) but the fix only reaches
#    the level when that script is RE-RUN, because the script clears and re-spawns
#    every actor it owns. A level saved between those two moments ships a room with
#    no exit.
#
#      Walk: living room -> hall -> upstairs, in the PACKAGED exe.
#
#    If the line looks angled rather than straight along the wall, stop and re-run
#    the script before shipping.
#
# 2. THE SIGN SHOWS BOTH LINES. The card must not cover the plate:
#
#      MRS. HALL'S CRAP FACTORY          (dark marker on a light card, tilted)
#      HALL DIVISION - MATERIALS RECLAMATION LINE 4    (brass, underneath)
#
#    One line without the other is the gag broken in one of the two ways it has
#    already broken: text wider than its card, or a card as dark as its own writing.
#
# 3. THE GOAT, WHICH NEEDS THE TICKET FIRST. Fix the GRADER, let Mrs. Hall close
#    ticket 1, THEN press R on any stage. Expected: the crate is replaced by an
#    animal, its brass plaque still hangs in front of it, and the line keeps running
#    — the workpiece travels to the animal and lands in ACCEPT. R again restores the
#    crate. Before the ticket closes, R must still REPAIR a stage and never transmute
#    it; if it transmutes early the puzzle can be hidden behind an animal.
#
# 4. MRS. HALL'S LIVESTOCK LINE reads from Content/Data/MrsHallStory.csv, which the
#    game loads as a LOOSE FILE at runtime (DirectoriesToAlwaysStageAsNonUFS "Data").
#    It is text-only — the AudioKey is deliberately empty. Confirm the CSV is staged:
#
#      ...\StagedBuilds\Windows\SibeliusGame\Content\Data\MrsHallStory.csv
#
#    Missing it does not crash; she simply stops speaking, for every line she has.
#
# 5. THE KITCHEN STILL HAS NO FLOATING POT — the 0.9.7.6 fix, and PIE has never once
#    reproduced that bug. Packaged exe only.
#
# 6. THE BOOT CUTSCENE, carried from 0.9.7.4/.5/.6. Kaia's face, her voice, then the
#    office. The voice under /Game/Audio/Cinematics is gitignored AI audio and rides
#    on the sequence's hard reference alone:
#
#      ...\Content\Cinematics\L_Cine_KaiaIntro.umap
#      ...\Content\Cinematics\LS_Kaia_Intro.uasset
#      ...\Content\Cinematics\AS_MHP_Kaia_Intro_Face.uasset
#      ...\Content\Audio\Cinematics\kaia_intro.uasset
#
# 7. SYNTHETIC-VOICE LICENCE. AI speech ships in this build. Walt confirmed a paid
#    ElevenLabs plan (2026-08-25).
#
# 8. SAVE COMPATIBILITY: no new save fields. The menagerie arms off the existing
#    Ticket.Legacy.Closed grant, so an old save that has closed the ticket gets the
#    goats immediately, which is correct.

$uat = "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat"
$log = "C:\Users\wpark\projects\sibelius-game\pkg-v0977.log"
if (Test-Path $log) { Remove-Item $log -Force }
$uatArgs = @(
  "BuildCookRun",
  "-nop4","-utf8output","-nocompileeditor","-skipbuildeditor","-cook",
  "-project=C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject",
  "-target=SibeliusGame",
  "-platform=Win64","-installed","-SkipCookingErrorSummary","-clientarchitecture=x64",
  "-stage","-archive","-package","-build","-pak","-iostore","-compressed","-prereqs",
  "-archivedirectory=C:\Users\wpark\builds\sibelius-v0.9.7.7",
  "-clientconfig=Shipping"
)
& $uat @uatArgs *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
