// Ch3 - Compile (SIB-27). A collectible book (E to collect via the shared IInteractable system).
// Place ~12 in the library room. Destroys itself on collect (C3: no dupes).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CompileTypes.h"
#include "Interactable.h"
#include "BookPickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
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

	// SIB-36: pickups go inert while a branch is open (dropped collision so the
	// interact trace skips them + no prompt + shrunk; the scene desaturate greys
	// them). Driven by UBranchPIEComponent off the branch depth.
	void SetInert(bool bNewInert);

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(EditAnywhere, Category = "Pickup")
	EResourceType Resource = EResourceType::Book;

	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (ClampMin = "1"))
	int32 Amount = 1;

	// FUN-2: every book also pays a little Sauce — knowledge is the raw
	// ingredient of the Sauce of All Knowledge, so collecting it feeds the
	// wallet. Small and repeatable (books are finite, placed by hand).
	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (ClampMin = "0"))
	int32 SauceOnCollect = 5;

	/**
	 * The pickup glow, per instance. Defaults suit the DIM LIBRARY; a book sitting in
	 * window light (the living-room table) needs a brighter, tighter lamp or it is
	 * invisible — roughly 4000 at radius 70.
	 *
	 * THESE ARE PROPERTIES, NOT A NAME TEST. This was previously chosen with
	 *
	 *     GetActorNameOrLabel().Contains(TEXT("LivingRoom"))
	 *
	 * which reads the EDITOR LABEL — and actor labels are WITH_EDITOR only. In a cooked
	 * build there is no label, so it falls back to GetName(), the internal object name,
	 * which the placement script never set (it only calls set_actor_label). PIE would
	 * light the book correctly and the shipped game would leave it at library dimness,
	 * with nothing in any log — the same shape as the prompts that were invisible for
	 * eight releases. Map data survives cooking; editor labels do not.
	 */
	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (ClampMin = "0"))
	float GlowIntensity = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (ClampMin = "1"))
	float GlowRadius = 140.0f;

private:
	void EnsureGlow();

	bool bCollected = false; // re-entrancy guard (C3)
	bool bInert = false;     // SIB-36: suspended while a branch is open
};
