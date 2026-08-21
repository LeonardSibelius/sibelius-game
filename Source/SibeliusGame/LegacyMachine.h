// LegacyMachine.h
//
// ONE-DAY TEST (docs/MACHINE_PLAN.md §8) — THE LEGACY SYSTEM.
//
// Mrs. Hall's opening line is a software maintenance ticket: "The legacy system threw
// again overnight." Until now, grepping this project for that system returned her line
// and a code comment. It did not exist. This is it.
//
// WHAT IT IS: a machine that runs in front of you. A workpiece travels along a row of
// parts, gets processed, and drops into the ACCEPT bin. Except it doesn't -- it goes to
// REJECT, every single time, and the tally on the housing counts the damage.
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
// machine's own behaviour and its own labels. If that is satisfying for ten minutes, the
// direction in MACHINE_PLAN is real; if it is not, it cost a day.
//
// STEPS 5 AND 6 REQUIRED NO NEW CODE. The fix lives on the part's
// URefactorableComponent, which already implements IBranchable with a level-baked GUID,
// so branch/merge/discard and deploy-persistence apply to this machine for free. That is
// itself a result: the existing power systems compose onto a new subject without
// modification.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LegacyMachine.generated.h"

class ALegacyMachinePart;
class UStaticMeshComponent;
class UTextRenderComponent;
class UCodeVisionComponent;

UCLASS()
class SIBELIUSGAME_API ALegacyMachine : public AActor
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

	/** True when every part is behaving. Read fresh each cycle — never cached, so a
	 *  discarded Test-Drive branch needs no invalidation. */
	UFUNCTION(BlueprintPure, Category = "Legacy Machine")
	bool IsHealthy() const;

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

	/** THE EVIDENCE. "SINCE 03:00 — ACCEPTED 0 / REJECTED 47". The overnight throw Mrs.
	 *  Hall is complaining about, stated as numbers the player can watch change. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Machine")
	TObjectPtr<UTextRenderComponent> Tally;

	/** Headless self-test for the smoke commandlet: a faulty machine rejects, the same
	 *  machine with its faulty part refactored accepts, and reverting brings the fault
	 *  back. Pure state — no world, no player, no ticking. */
	bool RunMachineSelfTest(FString& OutError) const;

	/** Progression grant claimed on the first ACCEPT. Durable (the progression slot),
	 *  so a fresh player who cannot Deploy yet still finds the job done after a reload.
	 *  Scoped to this machine — we do not snapshot the house. */
	static const FName ClosedTicketGrant;

protected:
	virtual void BeginPlay() override;

private:
	/** One subscription for the whole machine, forwarded to every part. */
	void TryBindCodeVision();
	UFUNCTION()
	void HandleCodeVisionChanged(bool bIsActive);

	void BeginCycle();
	void FinishCycle();
	void UpdateTally();

	/** First ACCEPT on a healthy machine closes Mrs. Hall's opening ticket. */
	void TryCloseTicket();

	/** If the ticket was already closed on this save, re-apply the fix after every
	 *  part's RefactorableComponent has reset itself in BeginPlay. */
	void MaybeRestoreClosedTicket();

	FTimerHandle BindRetryHandle;
	FTimerHandle RestoreTicketHandle;
	int32 BindAttempts = 0;
	bool bBoundToCodeVision = false;

	/** Cycle state: which leg of the journey, and how far along it. */
	int32 StageIndex = 0;
	float StageElapsed = 0.0f;
	bool bResting = false;
	bool bLastRunAccepted = false;
};
