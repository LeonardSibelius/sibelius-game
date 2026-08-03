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

	/**
	 * Aim assist for the dancing girls. A dancer's collision capsule is a fixed cylinder
	 * at her actor origin and does NOT follow the animation, so a dance that travels
	 * leaves her body outside her own capsule most of the time and the trace above keeps
	 * missing. When the trace finds nothing interactable, we fall back to "is a dancer
	 * near the centre of the screen and in view" — measured against her mesh bounds,
	 * which do move with her.
	 *
	 * Only used when the trace found NOTHING, so a machine or door you are actually
	 * looking at always wins over a dancer standing behind it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Dancer", meta=(ClampMin="0.0"))
	float DancerAssistRange = 500.f;

	/** Half-angle (degrees) from screen centre within which a dancer can be picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Dancer", meta=(ClampMin="0.0", ClampMax="90.0"))
	float DancerAssistAngle = 22.f;

	/** Activate whatever the player is currently focused on. Bind this to E. */
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void TryInteract();

	/**
	 * What the player is looking at right now, or null. USlapComponent reads this so
	 * F can reshuffle a dancer's dance instead of fighting her — one focus, two verbs.
	 */
	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Re-evaluate the focused interactable from the camera trace. */
	void UpdateFocus();

	/** Best dancer near the crosshair, or null. See DancerAssistRange. */
	AActor* FindDancerByAim(const FVector& Start, const FVector& Forward) const;

private:
	TWeakObjectPtr<UCameraComponent> CameraComp;
	TWeakObjectPtr<AActor> FocusedActor;
};
