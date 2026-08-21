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

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LegacyMachinePart.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class URefactorableComponent;

UCLASS()
class SIBELIUSGAME_API ALegacyMachinePart : public AActor
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

	/** Is this the part that is wrong? Exactly one part of a machine should be. */
	UPROPERTY(EditAnywhere, Category = "Legacy Part")
	bool bIsFaulty = false;

	/** True when this part is behaving — a healthy part always, a faulty one only once
	 *  it has been refactored. The machine asks every part this each cycle. */
	UFUNCTION(BlueprintPure, Category = "Legacy Part")
	bool IsBehaving() const;

	/** Show/hide the Code Vision labels. Driven by ALegacyMachine, which owns the one
	 *  subscription to the player's UCodeVisionComponent — N parts each retry-binding to
	 *  the pawn would be N times the timers for the same answer. */
	void SetTrueNameVisible(bool bVisible);

	/** The plaque's claim, without the heading. "GRADER\ngrade B or better passes"
	 *  becomes "grade B or better passes". The eye compares this to TrueName. */
	FString GetPlaqueClaim() const;

	/** Push PlaqueText onto the plaque and the live true-name (authored lie, or the
	 *  claim once this part has been refactored) onto TrueLabel. BeginPlay, a refactor,
	 *  a revert, and the headless self-test all go through here so the displayed pair
	 *  cannot drift from the state the machine is actually running. */
	void SyncLabelsToState();

	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The engraved plaque: the docs. Small, always on. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UTextRenderComponent> Plaque;

	/** The truth: bigger, brighter, Code Vision only. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<UTextRenderComponent> TrueLabel;

	/** The fix lives here. Refactoring this part clears the fault; because this component
	 *  is already IBranchable, Test-Drive and Deploy inherit that for free. */
	UPROPERTY(VisibleAnywhere, Category = "Legacy Part")
	TObjectPtr<URefactorableComponent> Refactorable;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleRefactorChanged(bool bIsRefactored);
};
