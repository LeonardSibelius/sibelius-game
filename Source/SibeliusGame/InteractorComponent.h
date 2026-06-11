// InteractorComponent.h
//
// Player-side interactor. Each tick it line-traces ECC_Visibility from the
// owner's camera, and if it hits an IInteractable it surfaces that actor's
// prompt on screen. Bind your "interact" input (E) to TryInteract().

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractorComponent.generated.h"

class UCameraComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SIBELIUSGAME_API UInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractorComponent();

	/** How far (cm) the interaction trace reaches from the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="0.0"))
	float InteractRange = 450.f;

	/**
	 * Radius (cm) of the interaction sweep. The trace is a sphere, not a thin
	 * line, so aim doesn't have to be pixel-perfect. 0 = old razor-line behavior.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="0.0"))
	float InteractRadius = 30.f;

	/** Activate whatever the player is currently focused on. Bind this to E. */
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void TryInteract();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Re-evaluate the focused interactable from the camera trace. */
	void UpdateFocus();

private:
	TWeakObjectPtr<UCameraComponent> CameraComp;
	TWeakObjectPtr<AActor> FocusedActor;
};
