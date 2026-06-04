// Ch3 - Compile (SIB-27). A collectible book (E to collect via the shared IInteractable system).
// Place ~12 in the library room. Destroys itself on collect (C3: no dupes).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CompileTypes.h"
#include "Interactable.h"
#include "BookPickup.generated.h"

class UStaticMeshComponent;
class UInventoryComponent;

UCLASS()
class SIBELIUSGAME_API ABookPickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ABookPickup();

	// Adds to the inventory and destroys this pickup. Returns false if already collected.
	bool Collect(UInventoryComponent* Inventory);

	// IInteractable: collect on E. Inventory comes from the interactor (the pawn).
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Pickup")
	EResourceType Resource = EResourceType::Book;

	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (ClampMin = "1"))
	int32 Amount = 1;

private:
	bool bCollected = false; // re-entrancy guard (C3)
};
