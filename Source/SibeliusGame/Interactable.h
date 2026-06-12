// Interactable.h
//
// Reusable interaction contract. Any actor that implements this can be focused
// by the player's UInteractorComponent (camera line trace) and activated with E.
//
// Interact / GetInteractionPrompt are BlueprintNativeEvents, so an implementer
// may be pure C++ (override *_Implementation) or a Blueprint. Callers MUST go
// through IInteractable::Execute_Interact / Execute_GetInteractionPrompt — never
// call the virtuals directly, or Blueprint implementers silently no-op.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

// SIBELIUSGAME_API (SIB-42/PK13): the Execute_ thunks are static members of
// this class; the editor module's commandlets link against them cross-module.
class SIBELIUSGAME_API IInteractable
{
	GENERATED_BODY()

public:
	/** Called when the player interacts (E) while focused on this actor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(AActor* Interactor);

	/** Prompt shown while the player is focused on this actor. Empty = no prompt. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	FText GetInteractionPrompt() const;
};
