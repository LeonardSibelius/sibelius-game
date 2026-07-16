// RefactorComponent.h
//
// SIB-26 — Ch2 Refactor. The player's selection + trigger component. Lives ON
// the character, so there's NO "find the player" race (R7 — the Ch1 door bug
// can't happen here). It camera-traces each tick for a URefactorableComponent
// and, on the Refactor input, toggles whatever it's currently looking at.

// APPEAL-R (Walt): R is also the WILD refactor — point at nearly any prop (the
// office desk, a chair) or a Refuser and rewrite it as a creature from the
// Menagerie. Props are reversible (R again restores the original — drafts are
// drafts); a refactored Refuser is gone for good. Pre-authored
// URefactorableComponents always win over the wild path.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RefactorComponent.generated.h"

class URefactorableComponent;
class UStaticMesh;
class UStaticMeshComponent;

// Bookkeeping stamped onto a wild-refactored prop so R can restore it.
UCLASS()
class SIBELIUSGAME_API UWildRefactorState : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY() TObjectPtr<UStaticMesh> OriginalMesh;
	UPROPERTY() FVector OriginalScale = FVector::OneVector;
};

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API URefactorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URefactorComponent();

	// Bind to IA_Refactor (Started). Toggles the currently targeted refactorable.
	UFUNCTION(BlueprintCallable, Category = "Refactor")
	void TriggerRefactor();

	// The refactorable under the crosshair this frame, or null. HUD can read this
	// to show a "[R] Refactor" prompt.
	UFUNCTION(BlueprintPure, Category = "Refactor")
	URefactorableComponent* GetCurrentTarget() const { return CurrentTarget; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// How far the selection trace reaches (cm).
	UPROPERTY(EditAnywhere, Category = "Refactor")
	float TraceDistance = 800.f;

	// --- Wild refactor (APPEAL-R) --------------------------------------------

	// The creatures a wild refactor can produce. HARD references (the
	// soft-refs-don't-cook lesson): defaults found in the constructor —
	// dragon, toy rabbit, butterfly, dragonfly — extendable in editor.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	TArray<TObjectPtr<UStaticMesh>> Menagerie;

	// Props whose bounding box exceeds this (largest side, cm) are refused —
	// keeps walls, floors, and whole buildings out of the zoo.
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	float MaxWildTargetSize = 350.f;

	// A refactored Refuser becomes a creature roughly this tall (cm).
	UPROPERTY(EditAnywhere, Category = "Refactor|Wild")
	float RefuserCreatureSize = 180.f;

private:
	URefactorableComponent* TraceForRefactorable() const;

	// The wild path: prop → creature, creature-prop → original, Refuser →
	// creature statue. Returns true if anything changed.
	bool TryWildRefactor();
	UStaticMesh* PickCreature() const;
	static void ApplyCreature(UStaticMeshComponent* Target, UStaticMesh* Creature, float MatchSize);

	UPROPERTY() TObjectPtr<URefactorableComponent> CurrentTarget;
};
