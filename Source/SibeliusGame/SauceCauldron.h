// SauceCauldron.h — "The Sauce of All Knowledge"
// FUN-3 (docs/FUN_PLAN.md Step 3): un-stubbed from the June 13 P0. The cauldron
// is now the game's SHOP — E opens USauceShopWidget, where earned Sauce buys
// powers and upgrades at known prices (deterministic spend; the gamble lives at
// the Carousel, not here). The original blend state (FeedSauce/OnSauceComplete)
// is kept intact for ABookRain and the World Three ceremony.

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

	// FUN-8.1 (Walt): the interaction lives on an INVISIBLE box stretched over
	// existing kitchen props (the stove + pots), so no placeholder mesh has to
	// squat in the scene. Mirrors BookPickup's collision recipe — QueryOnly,
	// block-all — so the interactor's camera trace finds it. Scale it in-editor
	// to wrap whatever should read as "the cauldron".
	UPROPERTY(VisibleAnywhere, Category = "Sauce")
	TObjectPtr<class UBoxComponent> InteractZone;

	// Optional dress meshes — leave empty when real props (the stove) play the
	// part; assign only if the cauldron ever gets its own hero mesh.
	UPROPERTY(VisibleAnywhere, Category = "Sauce")
	TObjectPtr<UStaticMeshComponent> CauldronMesh;

	UPROPERTY(VisibleAnywhere, Category = "Sauce")
	TObjectPtr<UStaticMeshComponent> ContentsMesh;   // glowing sauce surface; optional, hide if unused

	UPROPERTY(EditAnywhere, Category = "Sauce")
	FText PromptText = FText::FromString(TEXT("Blend the Sauce [E]"));

private:
	bool bComplete = false;
	void HandleComplete();

	// FUN-3: the shop screen, created on first open and reused.
	UPROPERTY()
	TObjectPtr<class USauceShopWidget> ShopWidget;
};
