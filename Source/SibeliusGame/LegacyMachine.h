// LegacyMachine.h
//
// ONE-DAY TEST (docs/MACHINE_PLAN.md §8) — THE LEGACY SYSTEM.
//
// Mrs. Hall's opening line is a software maintenance ticket: "The legacy system threw
// again overnight." Until now, grepping this project for that system returned her line
// and a code comment. It did not exist. This is it.
//
// WHAT IT IS: a machine that runs in front of you. A workpiece travels along a row of
// parts, gets processed, and drops into the ACCEPT bin. Except it doesn't -- it dies at
// the stage that is broken, goes to REJECT, and the tally counts the damage.
//
// WHAT THE PLAYER DOES (the whole loop this test exists to evaluate):
//
//   1. WATCH.       The machine runs. The fault is visible, not described.
//   2. READ.        Every part's plaque says what it does. They all sound fine.
//   3. CODE VISION. The true names appear. One part's source contradicts its docs.
//   4. REFACTOR.    Fix that part. The next workpiece lands in ACCEPT.
//   5. TEST-DRIVE.  Branch [6], watch a cycle, merge [7] or discard [8].
//   6. DEPLOY.      Ship it, and it is still fixed next session.
//
// There is NO quest marker and no "inspect the sorter" prompt. The evidence is the
// machine's own behaviour and its own labels.
//
// STEPS 5 AND 6 REQUIRED NO NEW CODE. The fix lives on the part's
// URefactorableComponent, which already implements IBranchable with a level-baked GUID,
// so branch/merge/discard and deploy-persistence apply to this machine for free. That is
// itself a result: the existing power systems compose onto a new subject without
// modification.
//
// =================================================================================
// THE THREE INSTRUMENTS (added after the one-day test passed)
//
// The first build shipped a machine you could only stare at. It rejected at the END of
// the row whatever was broken, it ran on its own timer whether you were ready or not,
// and it kept no history -- so the only evidence in the level was five label pairs to
// compare by eye. That is a spot-the-difference, not a diagnosis, and it does not grow:
// five parts is a squint, eleven (MACHINE_PLAN's own number) would be a search, which is
// the failure mode that document explicitly warns against.
//
// So the machine now reports on itself, three ways, and none of them cost a power:
//
//   1. IT DIES WHERE IT BREAKS.  The workpiece stops dead at the first misbehaving
//      stage, that stage's fault lamp lights, and the piece is diverted to REJECT from
//      THERE. Behaviour finally carries information: a broken INTAKE and a broken
//      GRADER no longer look identical.
//
//   2. THE LINE HAS A TRANSPORT.  E halts it and then steps it one beat at a time --
//      a leg of travel, the jam, the drop into a bin. A debugger with no code in it:
//      pause and step, which is how anyone actually watches a system misbehave. It also
//      fixes a pacing problem, because a free-running cycle is ~7 seconds of waiting
//      and the player was a spectator for all of it.
//
//   3. IT KEEPS A LOG.  The last few cycles, with the stage each one died at, and it is
//      PRE-FILLED with the overnight history the ticket refers to. Mrs. Hall says it
//      threw at 03:00; the housing now shows the 03:41 through 03:46 entries saying so.
//
// WHERE IS FREE, WHY IS EARNED. Note what these deliberately do NOT do. The lamp and the
// log name the stage; they never say what is wrong with it. GRADER's plaque promises
// "grade B or better passes" and sounds completely reasonable -- the contradiction is
// only visible under Code Vision, and realising that nothing is better than an A is
// still the player's own. Narrowing the search is what a log is FOR; doing the thinking
// is not.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "LegacyMachine.generated.h"

class ALegacyMachinePart;
class UStaticMeshComponent;
class UTextRenderComponent;
class UCodeVisionComponent;

/** Where a workpiece is in its cycle. This replaced a pair of bools (bResting plus an
 *  implicit "on the last leg") once the piece gained somewhere to die and a transport
 *  that can freeze it: the halt/step control advances EXACTLY ONE phase per press, so
 *  the phases have to be the beats a player would name -- travel, jam, drop, wait. */
UENUM()
enum class ELegacyCyclePhase : uint8
{
	/** Riding a leg from one part to the next. */
	Travelling,
	/** Stopped at a misbehaving stage, fault lamp lit. The visible fault. */
	Jammed,
	/** On its way off the row into ACCEPT or REJECT. */
	Exiting,
	/** In the bin, waiting for the next piece to start. */
	Resting
};

UCLASS()
class SIBELIUSGAME_API ALegacyMachine : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ALegacyMachine();

	virtual void Tick(float DeltaSeconds) override;

	/** The stages, in running order. Placed as separate actors and wired here — see the
	 *  note in LegacyMachinePart.h for why each part must be its own actor. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine")
	TArray<TObjectPtr<ALegacyMachinePart>> Parts;

	/** Seconds a workpiece spends travelling between two parts. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine", meta = (ClampMin = "0.1"))
	float StageSeconds = 1.1f;

	/** Seconds the finished piece sits in its bin before the next one starts. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine", meta = (ClampMin = "0.1"))
	float ResetSeconds = 1.4f;

	/** Seconds the piece sits dead at the broken stage before being thrown out. This is
	 *  the beat that makes the fault READABLE from across the room — without a pause the
	 *  eye cannot tell "it turned off at box three" from "it turned off somewhere". */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine", meta = (ClampMin = "0.0"))
	float JamSeconds = 0.9f;

	/** How high above the row the workpiece rides, in centimetres.
	 *
	 *  IT TRAVELS THROUGH THE STATIONS, NOT OVER THEM. Lifting it to 80 did make the
	 *  motion legible, and it was the wrong fix: a piece sailing above the crates reads as
	 *  floating past the machine rather than being processed by it. The signs came down
	 *  instead. Measured (dump_legacy_machine_geometry.py): the crates span z90–160, this
	 *  puts the piece at z130 and its mesh is base-at-origin and 18cm tall, so it occupies
	 *  z130–148 — and the plaque plates now top out at z128 with the fault lamps starting
	 *  at z149, leaving that band clear the whole length of the row.
	 *
	 *  Those three numbers move together. Change one and re-run the dump. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine", meta = (ClampMin = "0.0"))
	float CarryHeight = 40.0f;


	/** How many cycles the housing's run log shows. Six is a screenful at reading
	 *  distance and enough history to see a pattern rather than an incident. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine", meta = (ClampMin = "1"))
	int32 RunLogRows = 6;

	/* ---- MAKING IT LOOK LIKE A MACHINE ---------------------------------------------
	   The line was mechanically finished and visually inert: a crate sliding down a
	   straight line at constant speed, which reads as a placeholder no matter how
	   correct the state machine behind it is. None of this changes a single outcome —
	   the piece lands in the same bin either way — it just makes the thing look like it
	   is doing work, which is most of what "entertaining" means for a machine you stand
	   and watch. No new assets: every one of these moves a component that already
	   exists. */

	/** How high the piece arcs between stations. It peaks at mid-gap, where there is no
	 *  plate to clip — 37cm from either station against plates 27cm wide. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Feel", meta = (ClampMin = "0.0"))
	float HopHeight = 14.0f;

	/** Squash-and-recover when the piece lands on a stage: that stage doing something
	 *  to it, without touching the crate (whose mesh is the actor root, so scaling it
	 *  would breathe the plaques and plates bolted to it). */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Feel", meta = (ClampMin = "0.0"))
	float SquashSeconds = 0.24f;

	/** How hard the piece rattles while jammed. A stuck part that sits perfectly still
	 *  looks switched off; one that shakes looks like it is failing at something. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Feel", meta = (ClampMin = "0.0"))
	float JamRattle = 1.7f;

	/** Fault-lamp blink rate while jammed, in Hz. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Feel", meta = (ClampMin = "0.0"))
	float LampBlinkHz = 5.0f;

	/** How long the winning bin's label swells when a piece lands in it. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Feel", meta = (ClampMin = "0.0"))
	float VerdictFlashSeconds = 0.45f;

	/** True when every part is behaving. Read fresh each cycle — never cached, so a
	 *  discarded Test-Drive branch needs no invalidation. */
	UFUNCTION(BlueprintPure, Category = "Legacy Machine")
	bool IsHealthy() const;

	/** Index of the FIRST stage that is misbehaving, or INDEX_NONE on a healthy line.
	 *  First, not worst: a piece cannot reach stage four if stage two already threw it
	 *  out, which is also why one fault at a time stays the rule. */
	UFUNCTION(BlueprintPure, Category = "Legacy Machine")
	int32 FindFaultStage() const;

	/** Is the line stopped under the player's hand? */
	UFUNCTION(BlueprintPure, Category = "Legacy Machine")
	bool IsHalted() const { return bHalted; }

	/** Lifetime counts since the level loaded, shown on the housing. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	int32 Accepted = 0;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	int32 Rejected = 0;

	/* A BARE ROOT, and everything else hangs off IT rather than off the bed.
	   The first build made Bed the root and attached the workpiece, the bins and the
	   tally to it. Bed is scaled (0.5, 5.6, 0.12) to run the length of the machine, and
	   children INHERIT that scale — so a tally placed 14cm up landed 1.68cm up, at floor
	   level behind the parts, and the bins came out squashed. The machine's own evidence
	   was invisible. Siblings never hang off a stretched component. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UStaticMeshComponent> Bed;

	/** Bin labels, so the verdict is readable rather than inferred from which way the
	 *  workpiece went. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UTextRenderComponent> AcceptLabel;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UTextRenderComponent> RejectLabel;

	/** The thing being processed. Slides from part to part, then drops into a bin. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UStaticMeshComponent> Workpiece;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UStaticMeshComponent> AcceptBin;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UStaticMeshComponent> RejectBin;

	/* ---- THE VERDICT, AS FURNITURE ------------------------------------------------
	   The tally reads ACCEPTED 0 / REJECTED 47 and the run log lists the overnight
	   throws, but both of those are text on a housing: you have to walk up to the
	   machine and read them. The bins are the same evidence at a glance, and until now
	   they were two identical empty crates that said nothing at all.

	   So REJECT overflows -- a heap on the crate and boxes scattered on the carpet
	   around it -- and ACCEPT holds nothing but a film of grime. One look from the
	   doorway and you know how this machine's career has gone, before Mrs. Hall has
	   said a word about it. It also costs no new mechanic: the pile IS the 47 pieces
	   the tally already claims were thrown overnight.

	   THE SPILL IS MADE OF THE WORKPIECE. DressTheVerdict() reads the mesh and the
	   scale off the Workpiece component rather than naming an asset here, so the heap
	   is literally the thing this machine rejects, and it keeps matching if
	   build_legacy_machine.py ever swaps WORKPIECE_MESH. */

	/** How many spilled pieces are shown. Components past this are hidden rather than
	 *  destroyed, so the heap can be thinned in the Details panel without a rebuild. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Verdict", meta = (ClampMin = "0", ClampMax = "12"))
	int32 RejectSpillCount = 12;

	/** A FIXED heap, not a growing one. It is already there when the player walks in,
	 *  which is the whole point -- and set dressing that animates is set dressing that
	 *  can be caught mid-animation looking wrong. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine|Verdict")
	TArray<TObjectPtr<UStaticMeshComponent>> RejectSpill;

	/** The film in the bottom of an untouched crate. ACCEPT's emptiness does the real
	 *  work; this only stops it reading as "clean and ready" instead of "never used". */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine|Verdict")
	TObjectPtr<UStaticMeshComponent> AcceptDust;

	/* ---- THE SIGN ------------------------------------------------------------------
	   Walt's note: the line should read as a joke and a failure, under a big demeaning
	   title. The trap in that is VOICE. Mrs. Hall would never call her own line a crap
	   factory, so a title that says so is the GAME editorialising over her, and this
	   room already has one narrator too many.

	   So the demeaning title is not the game's. It is a sheet of card, hand-lettered
	   and taped crooked over the official brass plate, with both ends of the real name
	   still showing past it. Same joke, except now somebody MADE it: a programmer sat
	   in this chair before you, felt exactly what you are about to feel, and defaced
	   the sign. That is characterisation of the job rather than commentary on it, and
	   it is the cheapest storytelling in the level -- two slabs and two strings.

	   THE PLATE STAYS STRAIGHT-FACED, exactly like the part plaques. The comedy is the
	   hardware and the card; the official language never winks, because a plaque that
	   is in on the joke cannot also be the thing Code Vision catches lying. */

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine|Sign")
	TObjectPtr<UStaticMeshComponent> SignPlate;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine|Sign")
	TObjectPtr<UTextRenderComponent> SignOfficialText;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine|Sign")
	TObjectPtr<UStaticMeshComponent> SignCard;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine|Sign")
	TObjectPtr<UTextRenderComponent> SignText;

	/** The line's real name. Bureaucratically reasonable on purpose: it is funnier read
	 *  straight, and it has to survive being the thing the card is mocking. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Sign")
	FString OfficialName = TEXT("HALL DIVISION  -  MATERIALS RECLAMATION LINE 4");

	/* ---- THE HOUSING READOUTS ARE OFF -----------------------------------------------
	   Walt, 2026-08-26, looking at the room: *"the two signs RUN LOG and ACCEPTED 0
	   REJECTED 73 are really not very interesting to me and I would like them removed."*
	   His call, and he is the one who has to look at it.

	   WHAT IT COSTS, stated so the trade is on the record rather than rediscovered. The
	   run log was one of the three instruments added in MACHINE_PLAN section 9, and it
	   was the one that named the failing stage in words -- "03:56 REJECTED AT GRADER",
	   six rows of it, pre-seeded with the overnight history Mrs. Hall's ticket refers
	   to. Without it the diagnosis rests entirely on WATCHING: the piece dies at the
	   broken stage and that stage's fault lamp lights. That is still sufficient, and it
	   is more diegetic than a caption -- but it is one channel, not two, and section 9
	   added the log precisely because one channel made the puzzle a spot-the-difference.
	   If the first ticket starts reading as opaque, this is the first thing to turn back
	   on.

	   HIDDEN, NOT DELETED. UpdateTally() and UpdateRunLog() still run and still hold
	   correct text; only the four components are invisible. Flip this in the Details
	   panel and the instruments come straight back, with no rebuild and no lost work. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Verdict")
	bool bShowHousingReadouts = false;

	/** What the last programmer wrote on the card. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine|Sign")
	FString HandLetteredName = TEXT("MRS. HALL'S CRAP FACTORY");

	/** Components are made in the constructor, which runs before any instance value is
	 *  applied, so the count cannot come from RejectSpillCount. Twelve is made; the
	 *  property decides how many are shown. */
	static constexpr int32 MaxRejectSpill = 12;

	/** THE EVIDENCE. "SINCE 03:00 — ACCEPTED 0 / REJECTED 47". The overnight throw Mrs.
	 *  Hall is complaining about, stated as numbers the player can watch change. Also
	 *  says LINE HALTED, so a machine stopped by the player is never mistaken for a
	 *  machine that has died. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UTextRenderComponent> Tally;

	/** THE HISTORY, stacked directly above the tally so the two read as one instrument
	 *  cluster on the housing rather than as two captions floating in a room. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UTextRenderComponent> RunLog;

	/* DARK PLATES BEHIND THE TWO READOUTS. See LegacyMachinePart.h for the reasoning —
	   the same dark-chip answer the HUD reached in v0.9.3, which the 3D labels never got.

	   These are sized for the WIDEST row the readout can ever show, not the current one,
	   so the panel does not breathe in and out every cycle as "ACCEPTED" becomes
	   "REJECTED AT GRADER". A stable rectangle reads as equipment; a resizing one reads
	   as a bug. That is also why UpdateTally always emits three lines. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UStaticMeshComponent> TallyPlate;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UStaticMeshComponent> RunLogPlate;

	/* ---- TICKET 2: TEST-DRIVE FINALLY HAS A JOB -----------------------------------
	   Test-Drive has worked on this machine since day one and has never been worth
	   pressing, because a deterministic fault is confirmed by watching one piece. An
	   intermittent one cannot be: after refactoring a stage that was dropping one in
	   three, three clean pieces is what luck looks like 30% of the time.

	   So the machine can be MEASURED. A test batch runs TestBatchSize pieces as pure
	   arithmetic — no animation, no waiting — and posts a single verdict row to the run
	   log. Twenty trials against a one-in-three fault miss it about three times in ten
	   thousand, which is the difference between hoping and knowing.

	   AND IT IS ONLY OFFERED INSIDE A BRANCH. The tally is Mrs. Hall's production
	   record; twenty experimental pieces down the live line is twenty rejects on it.
	   You do not test in production — that is the discipline the whole chapter is named
	   after, and here it is the reason the verb exists rather than a line of dialogue
	   about it. */
	UPROPERTY(EditAnywhere, Category = "Legacy Machine", meta = (ClampMin = "1"))
	int32 TestBatchSize = 20;

	/** Run the batch and post its verdict to the run log. Pure state: the tally does not
	 *  move, no piece animates, nothing persists. Returns the row it logged. */
	UFUNCTION(BlueprintCallable, Category = "Legacy Machine")
	FString RunTestBatch();

	/** True when reality is branched, so a test batch is on offer instead of the
	 *  transport. */
	bool CanRunTestBatch() const;

	/** Every armed fault refactored — the machine is not merely having a good cycle.
	 *  Ticket closing asks this, never IsHealthy(), or a lucky roll would close a job
	 *  the player has not done. */
	UFUNCTION(BlueprintPure, Category = "Legacy Machine")
	bool AreAllFaultsCleared() const;

	/** Has a test batch come back clean? Ticket 2.s close condition, alongside the fix. */
	UFUNCTION(BlueprintPure, Category = "Legacy Machine")
	bool IsProvenClean() const { return bBatchProvenClean; }

	/** Progression grant for the second job — the intermittent fault, armed by the
	 *  first ticket closing and closed in its turn by a proven fix. */
	static const FName IntermittentTicketGrant;

	/** Headless self-test for the smoke commandlet: a faulty machine rejects, the same
	 *  machine with its faulty part refactored accepts, and reverting brings the fault
	 *  back. Pure state — no world, no player, no ticking. */
	bool RunMachineSelfTest(FString& OutError) const;

	/** Progression grant claimed on the first ACCEPT. Durable (the progression slot),
	 *  so a fresh player who cannot Deploy yet still finds the job done after a reload.
	 *  Scoped to this machine — we do not snapshot the house. */
	static const FName ClosedTicketGrant;

	// IInteractable: E is the line's transport. Every part forwards its E here, so the
	// player can point at any crate in the row rather than hunting for the one strip of
	// bed that is not behind a box.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Meshes, scales, rotations and visibility for the reject heap, the grime in
	 *  ACCEPT, and the two lines of text on the sign. Runs from OnConstruction so the
	 *  editor shows it while dressing the room, and again at BeginPlay -- this project
	 *  has been caught three times by things that were only true in the editor. */
	void DressTheVerdict();

	/* ---- THE MENAGERIE, ONCE THE JOB IS DONE ---------------------------------------
	   Tag every stage WildRefactorOK. URefactorComponent reads that tag two ways: the
	   stage becomes a legal transmutation target despite being interactable, and its
	   plaques keep rendering while an animal stands in its place.

	   WHY AFTER THE TICKET AND NOT BEFORE. A stage's authored refactor IS the GRADER
	   fix -- the whole first job. Arm this early and a player can turn the broken stage
	   into a goat before diagnosing it, leaving the puzzle standing behind an animal.
	   Arming it on the close makes it a reward, landing exactly when the player has just
	   learned that this machine matters.

	   AND IT IS ALL FIVE, NEVER A SUBSET. Gate it per stage -- say, only ones that are
	   behaving -- and the stage that refuses to become a goat is the answer.

	   Nothing else has to change for the line to keep running: ALegacyMachine drives the
	   workpiece to each part's world LOCATION and never asks what the part looks like.
	   The box goes in the front of the goat and out the back on its own. */
	void ArmTheMenagerie();

	/** She says it once. Latched rather than checked against a grant because it is a
	 *  gag, not progression -- nothing else should ever branch on it. */
	bool bSaidTheLivestockLine = false;

private:
	/** One subscription for the whole machine, forwarded to every part. */
	void TryBindCodeVision();
	UFUNCTION()
	void HandleCodeVisionChanged(bool bIsActive);

	void BeginCycle();
	void FinishCycle();
	void UpdateTally();

	/** Put the piece ON a stage and decide what that stage does with it: a misbehaving
	 *  stage jams (lamp on), the last healthy stage starts the exit, anything else
	 *  travels. Every phase change into the row goes through here, which is why the
	 *  fault cannot be localised in one place and animated from another. */
	void EnterStage(int32 Index);

	/** Douse every fault lamp on the row. Called when a new piece starts, so a lit lamp
	 *  always refers to the piece the player is watching. */
	void ClearFaultLamps();

	/** First ACCEPT on a healthy machine closes Mrs. Hall's opening ticket. */
	void TryCloseTicket();

	/** If the ticket was already closed on this save, re-apply the fix after every
	 *  part's RefactorableComponent has reset itself in BeginPlay. */
	void MaybeRestoreClosedTicket();

	/* ---- the transport (mechanic 2) ------------------------------------------------
	   ONE KEY, THREE MEANINGS, DISAMBIGUATED BY WHERE THE PIECE IS. E halts a running
	   line; while halted mid-cycle E steps exactly one phase; and once the piece has
	   landed in a bin the line is PARKED between pieces, where E hands it back.

	   That last state is what keeps one key honest. The first version auto-resumed the
	   moment a piece landed, which meant a halt pressed during the drop into the bin did
	   nothing visible at all — roughly a fifth of every cycle where the button appeared
	   broken. Parking instead means every halt stops the line; it may just stop it at the
	   end of the piece rather than in the middle of the row. And it cannot strand the
	   machine either, because the tally says LINE HALTED and the prompt says how to give
	   it back.

	   The obvious alternative — a run/pause toggle plus a separate step key — needs two
	   verbs, and this game has exactly one interact verb by design. */
	void RequestHaltOrStep();

	/** Called when a phase completes: clears the one-shot step so a halted line freezes
	 *  again rather than running on. */
	void ConsumeStep();

	/** True when the machine may advance this frame. */
	bool CanAdvance() const { return !bHalted || bStepRequested; }

	/* ---- the run log (mechanic 3) --------------------------------------------------
	   Rows are plain strings because that is all the housing can render: one
	   UTextRenderComponent, newest row at the top. The component's COLOUR tracks the
	   newest verdict only — a text render has one colour for the whole block, so the
	   per-row distinction has to live in the words ("REJECTED AT GRADER" / "ACCEPTED"),
	   and it does. */
	void SeedOvernightLog();
	void AppendRawLogRow(const FString& Row, bool bGood);
	void AppendLogEntry(bool bAccepted, int32 FaultStage);
	void UpdateRunLog();

	/** Wall-clock label for cycle N, counting one minute per cycle from 03:00 — the hour
	 *  the ticket names. Cycle 47 is 03:47, which is why the player walks up to a log
	 *  that already runs to the small hours instead of one that starts when they do. */
	FString CycleTimestamp(int32 CycleNumber) const;

	/** Newest first. Capped at RunLogRows. */
	TArray<FString> LogRows;
	bool bLastLoggedAccept = false;

	FTimerHandle BindRetryHandle;
	FTimerHandle RestoreTicketHandle;
	int32 BindAttempts = 0;
	bool bBoundToCodeVision = false;

	/** Cycle state: which leg of the journey, how far along it, and what beat we are on. */
	int32 StageIndex = 0;
	float StageElapsed = 0.0f;
	ELegacyCyclePhase Phase = ELegacyCyclePhase::Travelling;

	/** Which stage this piece died at, latched at the moment it jams so the exit leg and
	 *  the log entry agree even if the player refactors mid-flight. */
	int32 JammedStage = INDEX_NONE;

	/** Stream for test batches. Separate from each part.s own fault stream so running a
	 *  batch never disturbs the sequence the live line is rolling against. */
	FRandomStream BatchStream;

	/** Set by a test batch that came back clean. Ticket 2 will not close without it:
	 *  refactoring an intermittent fault and watching one good piece is exactly the
	 *  false confidence the second job exists to punish. */
	bool bBatchProvenClean = false;

	/* Presentation state. None of it feeds a verdict. */
	FVector WorkpieceBaseScale = FVector::OneVector;
	float SquashElapsed = -1.0f;      // <0 = not squashing
	float VerdictFlashElapsed = -1.0f;
	bool bFlashAccepted = false;
	void TickPresentation(float DeltaSeconds);

	/** Transport state. */
	bool bHalted = false;
	bool bStepRequested = false;
};
