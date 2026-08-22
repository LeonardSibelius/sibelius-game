// LegacyMachinePart.h
//
// ONE-DAY TEST (docs/MACHINE_PLAN.md §8) — a single stage of the legacy system.
//
// THE MECHANIC IN MINIATURE: every part carries TWO descriptions of itself.
//
//   Plaque   — always visible, engraved on the housing. What the part CLAIMS to do.
//              This is the documentation.
//   TrueName — visible only under Code Vision. What the part ACTUALLY does.
//              This is the source.
//
// On a healthy part they agree. On the faulty one they do not, and the whole diagnosis
// is that disagreement: the docs say "passes grade A", the code says "rejects
// everything". Every programmer who has ever inherited a system knows that feeling, and
// it is the reason Code Vision is the FIRST power rather than a gimmick — reading what a
// thing really does is the job.
//
// A PART IS ITS OWN ACTOR, deliberately. URefactorComponent line-traces and then calls
// HitActor->FindComponentByClass<URefactorableComponent>() -- it resolves per ACTOR, not
// per component. Parts bolted onto one machine actor as extra meshes would all share a
// single refactorable, so aiming at ANY of them would fix the machine and "find the part
// that is lying" would stop being a question worth asking.
//
// The fix rides on URefactorableComponent's existing bIsRefactored, which means
// Test-Drive (branch/merge/discard) and Deploy (persist across sessions) work here with
// no new code at all: that component already implements IBranchable with a level-baked
// GUID identity.
//
// ---------------------------------------------------------------------------------
// WHERE IT FAILS IS NOT WHY IT FAILS.
//
// A part now carries a THIRD label, the FaultLamp, and it is deliberately NOT gated on
// Code Vision. When the workpiece dies at this stage, the lamp lights and says so, in
// plain sight, to a player who has taken no powers at all.
//
// That split is the whole point of the mechanic:
//
//   the lamp (and the run log)  tell you WHERE   — no power needed, it is the machine
//                                                  reporting on itself, like a log line
//   Code Vision                 tells you WHY    — the plaque and the source disagree
//
// Before this, the machine rejected at the END of the row no matter which stage was
// broken, so its behaviour carried no information and the only evidence in the level was
// five label pairs to compare by eye. Five is a squint; eleven (MACHINE_PLAN's number)
// would be a search, which is exactly the failure mode that document warns against.
// Localising the fault is what lets these machines get bigger without getting tedious.
//
// A PART IS ALSO IINTERACTABLE, and every one of them forwards E to the machine that
// owns it. The transport control (halt / step) belongs to the line, not to a stage, but
// the player is looking at a row of crates — asking them to find the one hittable strip
// of bed between the boxes would be an aiming puzzle, and this game already decided
// (UInteractorComponent's sphere sweep) that aiming is not the fun part.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "LegacyMachinePart.generated.h"

class ALegacyMachine;
class UStaticMeshComponent;
class UTextRenderComponent;
class URefactorableComponent;

UCLASS()
class SIBELIUSGAME_API ALegacyMachinePart : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ALegacyMachinePart();

	/** What the housing says this part does — the documentation. Always readable. */
	UPROPERTY(EditAnywhere, Category = "Legacy Part")
	FString PlaqueText = TEXT("STAGE");

	/** What it ACTUALLY does — revealed by Code Vision only. On the faulty part this
	 *  contradicts the plaque, and that contradiction is the whole puzzle. */
	UPROPERTY(EditAnywhere, Category = "Legacy Part")
	FString TrueName = TEXT("STAGE  ok");

	/** Is this the part that is wrong? Exactly one part of a machine should be, per
	 *  armed ticket. */
	UPROPERTY(EditAnywhere, Category = "Legacy Part")
	bool bIsFaulty = false;

	/* ---- TICKET 2: THE FAULT THAT IS NOT THERE EVERY TIME -------------------------
	   A deterministic fault is a logic error: it is wrong, it is wrong every cycle, and
	   one look confirms both the diagnosis and the fix. An INTERMITTENT fault is a
	   different animal and it is the one that teaches Test-Drive, because the thing you
	   cannot do with it is confirm a fix by watching. Three good pieces after a refactor
	   proves nothing when one in three was failing anyway. */

	/** 0 = a deterministic fault (ticket 1: wrong every cycle). Above 0 = the fraction of
	 *  cycles this part misbehaves on. Only meaningful when bIsFaulty. */
	UPROPERTY(EditAnywhere, Category = "Legacy Part", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaultChance = 0.0f;

	/** Fixed on purpose. A reproducible intermittent bug is both testable from the gate
	 *  and, in fiction, exactly the kind a maintenance programmer can actually corner —
	 *  the horror is the ones that are not reproducible, and that is not a game. */
	UPROPERTY(EditAnywhere, Category = "Legacy Part")
	int32 FaultSeed = 20260821;

	/** Progression grant that brings this fault to life. NAME NONE = armed from the
	 *  start (ticket 1's GRADER). Set to Ticket.Legacy.Closed and the fault — AND the
	 *  plaque/true-name disagreement that diagnoses it — simply does not exist until the
	 *  player has closed the first job.
	 *
	 *  That gating is the whole reason ticket 2 can live on the same machine. The opening
	 *  puzzle is "find the ONE part that is lying", and it would be ruined by a second
	 *  liar standing in the row from the first minute. */
	UPROPERTY(EditAnywhere, Category = "Legacy Part")
	FName ArmedByGrant;

	/** Does this part's authored fault exist yet? */
	UFUNCTION(BlueprintPure, Category = "Legacy Part")
	bool IsFaultArmed() const;

	/** Is this part permanently sound — no armed fault, or an armed one that has been
	 *  refactored? Distinct from IsBehaving(), which answers only for THIS cycle: an
	 *  intermittent part that happened to roll well is behaving but is not fixed, and
	 *  closing a ticket on that would hand the player a pass they did not earn. */
	UFUNCTION(BlueprintPure, Category = "Legacy Part")
	bool IsFaultCleared() const;

	/** Roll this part's luck for the cycle about to start. Called once per cycle by the
	 *  machine — NEVER from IsBehaving(), which is const, cheap and asked several times
	 *  per cycle by the tick, the log and the tally. A fault that re-rolled on every
	 *  question would flicker inside a single cycle and the piece would jam at a stage
	 *  the log then disagreed with. */
	void RollForCycle();

	/** Would this part misbehave on a fresh roll? Used by the test batch, which is a
	 *  statistical question and must not disturb the live cycle's latched verdict. */
	bool WouldMisbehaveOnTrial(FRandomStream& Stream) const;

	/** The same trial, but assuming the fault is live regardless of its arming grant.
	 *  The headless gate measures an intermittent rate through this: a commandlet has
	 *  claimed no grants, so ticket 2.s fault is dormant there and asking the armed
	 *  question would only ever measure zero. */
	bool WouldMisbehaveIfArmed(FRandomStream& Stream) const;

	/** True when this part is behaving — a healthy part always, a faulty one only once
	 *  it has been refactored. The machine asks every part this each cycle. */
	UFUNCTION(BlueprintPure, Category = "Legacy Part")
	bool IsBehaving() const;

private:
	/** This cycle.s latched verdict for an intermittent fault. */
	bool bMisbehavingThisCycle = false;
	FRandomStream FaultStream;

public:

	/** Show/hide the Code Vision labels. Driven by ALegacyMachine, which owns the one
	 *  subscription to the player's UCodeVisionComponent — N parts each retry-binding to
	 *  the pawn would be N times the timers for the same answer. */
	void SetTrueNameVisible(bool bVisible);

	/** Light the fault lamp while the workpiece is dying at this stage. NOT Code Vision
	 *  gated — see the header note: where is free, why is earned. */
	void SetFaultLampVisible(bool bVisible);

	/** Blink just the lamp TEXT, leaving its plate lit. A warning light flashing on a
	 *  steady panel reads as equipment; strobing the dark plate too reads as a bug. */
	void SetLampTextVisible(bool bVisible);

	/** The plaque's claim, without the heading. "GRADER\ngrade B or better passes"
	 *  becomes "grade B or better passes". The eye compares this to TrueName. */
	FString GetPlaqueClaim() const;

	/** Collapse line breaks and runs of spaces, so a wrapped claim can still be compared
	 *  word-for-word against a true name that wraps differently. */
	static FString FlattenLabel(const FString& In);

	/** The plaque's heading alone — "GRADER". The run log and the step prompt name
	 *  stages with this, so the machine's readouts and its housing use one vocabulary
	 *  and the player never has to map a log line onto a box. */
	FString GetPlaqueHeading() const;

	/** Push PlaqueText onto the plaque and the live true-name (authored lie, or the
	 *  claim once this part has been refactored) onto TrueLabel. BeginPlay, a refactor,
	 *  a revert, and the headless self-test all go through here so the displayed pair
	 *  cannot drift from the state the machine is actually running. */
	void SyncLabelsToState();

	/** The line this stage belongs to. Set by ALegacyMachine::BeginPlay, which already
	 *  holds the authoritative Parts array — a part that searched for its own machine
	 *  could find the wrong one the day there are two on the floor. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<ALegacyMachine> OwningMachine;

	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The engraved plaque: the docs. Small, always on. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UTextRenderComponent> Plaque;

	/** The truth: bigger, brighter, Code Vision only. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UTextRenderComponent> TrueLabel;

	/** The fault light: red, above the plaque, lit only while a piece dies here. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UTextRenderComponent> FaultLamp;

	/* DARK PLATES BEHIND THE TEXT — the same answer the HUD reached in v0.9.3 ("sauce and
	   Status sit on a dark chip so they read on wood"), finally applied to the 3D labels.

	   The row crosses a beige crate, a grey crate, a green crate and a wood floor, so the
	   background changes under every line and NO single text colour can work across it.
	   Brightening the font just makes a brighter smear. A plate makes the background
	   constant, and only then does colour mean anything — which also rescues the true
	   names, whose red/cyan distinction the Code Vision green flood has been washing out
	   since the day they were written.

	   Geometry (scale and offsets) is set by Tools/Scripts/build_legacy_machine.py, which
	   already knows every mesh bound and text size in one place; these are declared here
	   and positioned there, exactly like the refactor edit. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UStaticMeshComponent> PlaquePlate;

	/** Backing for the true name. Follows it, so Code Vision reveals a lit panel rather
	 *  than uncovering text on a plate that was already sitting there empty. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UStaticMeshComponent> TruePlate;

	/** Backing for the fault lamp. Shown and hidden with the lamp, never on its own — a
	 *  permanently lit empty plate above every crate would be five dark rectangles
	 *  hanging over a machine that is behaving perfectly. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UStaticMeshComponent> LampPlate;

	/** The fix lives here. Refactoring this part clears the fault; because this component
	 *  is already IBranchable, Test-Drive and Deploy inherit that for free. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<URefactorableComponent> Refactorable;

	// IInteractable: E anywhere on the row is the line's transport control. Both calls
	// forward to OwningMachine, so a stage never has an opinion of its own about
	// halting — there is one line and one state.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleRefactorChanged(bool bIsRefactored);
};
