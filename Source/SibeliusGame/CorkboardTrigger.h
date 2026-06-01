// CorkboardTrigger.h
//
// An invisible interactable. Placed over the office corkboard, it has no visual
// of its own but blocks the camera's Visibility trace so the player can focus it
// and press E. Doing so fires the Hall alarm once via UHallAlarmSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "CorkboardTrigger.generated.h"

class UBoxComponent;

UCLASS()
class SIBELIUSGAME_API ACorkboardTrigger : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACorkboardTrigger();

	/** Invisible box that blocks the interaction (Visibility) trace. */
	UPROPERTY(VisibleAnywhere, Category="Corkboard")
	TObjectPtr<UBoxComponent> InteractionVolume;

	/** Prompt shown while the player looks at the corkboard. */
	UPROPERTY(EditAnywhere, Category="Corkboard")
	FText Prompt = FText::FromString(TEXT("Press E to read the corkboard"));

	// IInteractable
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

private:
	bool bTriggered = false;
};
