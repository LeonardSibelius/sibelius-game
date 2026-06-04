// Ch3 - Compile (SIB-27). The locked attic hatch. E (shared IInteractable system) + a built Key unlocks it.
// Owns a blocking mesh that drops collision + hides on unlock.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "HatchLock.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHatchUnlocked);

UCLASS()
class SIBELIUSGAME_API AHatchLock : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHatchLock();

	// Spends one Key to unlock. Returns false (and stays shut) without a Key.
	UFUNCTION(BlueprintCallable, Category = "Hatch")
	bool TryUnlock(UInventoryComponent* Inventory);

	// IInteractable: unlock on E. Inventory comes from the interactor (the pawn).
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Hatch")
	bool IsLocked() const { return bLocked; }

	// Headless self-test for CompileSmokeTest (bar item 7).
	bool RunLockSelfTest(FString& OutError);

	UPROPERTY(VisibleAnywhere, Category = "Hatch")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Hatch")
	TObjectPtr<UStaticMeshComponent> BlockerMesh;

	UPROPERTY(BlueprintAssignable, Category = "Hatch")
	FOnHatchUnlocked OnUnlocked;

private:
	void ApplyLockedState(bool bNowLocked);

	UPROPERTY(EditAnywhere, Category = "Hatch")
	bool bLocked = true;
};
