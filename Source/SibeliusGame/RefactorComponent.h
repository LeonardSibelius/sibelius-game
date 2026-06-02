// RefactorComponent.h
//
// SIB-26 — Ch2 Refactor. The player's selection + trigger component. Lives ON
// the character, so there's NO "find the player" race (R7 — the Ch1 door bug
// can't happen here). It camera-traces each tick for a URefactorableComponent
// and, on the Refactor input, toggles whatever it's currently looking at.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RefactorComponent.generated.h"

class URefactorableComponent;

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

private:
	URefactorableComponent* TraceForRefactorable() const;

	UPROPERTY() TObjectPtr<URefactorableComponent> CurrentTarget;
};
