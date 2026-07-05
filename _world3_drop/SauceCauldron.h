// SauceCauldron.h — World Three / "The Sauce of All Knowledge"
// P0 STUB (June 13, 2026). Drop in Source/SibeliusGame/ (RUNTIME module).
// The altar centerpiece: holds the Sauce blend state, is E-interactable, fires once on completion.
// Code: reconcile the IInteractable include path + exact method signatures with the project's Interactable.h.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"            // project-wide E-interact interface (Ch1/Ch2). Verify name/path.
#include "SauceCauldron.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSauceComplete);

UCLASS()
class SIBELIUSGAME_API ASauceCauldron : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ASauceCauldron();

	// --- Sauce state ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sauce")
	float BlendProgress = 0.f;                 // 0..1

	UPROPERTY(EditAnywhere, Category = "Sauce")
	float CompleteThreshold = 1.f;

	// Add knowledge to the Sauce (called by ABookRain on arrival, and later by the Generate-feed path).
	// Returns true if THIS call pushed it to completion.
	UFUNCTION(BlueprintCallable, Category = "Sauce")
	bool FeedSauce(float Delta);

	UFUNCTION(BlueprintPure, Category = "Sauce")
	bool IsComplete() const { return bComplete; }

	// Fires exactly once when the Sauce completes. P5 hooks this to the AI Apparition + progress subsystem.
	UPROPERTY(BlueprintAssignable, Category = "Sauce")
	FOnSauceComplete OnSauceComplete;

	// --- IInteractable (match the project's exact signatures) ---
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sauce")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Sauce")
	TObjectPtr<UStaticMeshComponent> CauldronMesh;

	UPROPERTY(VisibleAnywhere, Category = "Sauce")
	TObjectPtr<UStaticMeshComponent> ContentsMesh;   // glowing sauce surface; optional, hide if unused

	UPROPERTY(EditAnywhere, Category = "Sauce")
	FText PromptText = FText::FromString(TEXT("The Sauce of All Knowledge"));

private:
	bool bComplete = false;
	void HandleComplete();
};
