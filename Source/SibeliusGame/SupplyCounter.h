// SupplyCounter.h — one [E], and Leonard is provisioned for space.
//
// docs/SPACEPORT_PLAN.md Phase E, the supply run.
//
// ===========================================================================
// WHY THERE IS NO SHOPPING.
//
// Walt, having walked into uFoods for the first time: "Instead of picking from all that
// stuff, maybe just [E] to Buy all Supplies at once."
//
// The room holds 1,956 meshes and 286 price tags, and none of that is a shopping system —
// it is a shop, which is a different thing. The errand exists so that BOARDING THE ROCKET
// is something Leonard earned rather than something he was handed; the beat is "he went
// and got what he needed", and an inventory screen would add clicks without adding that.
//
// So: stand at the counter, press E, and it is done. This is also why there is no clerk.
//
// ---------------------------------------------------------------------------
// WHAT IT ACTUALLY DOES, in the order that matters:
//
//   1. Already have the supplies?   Say so, change nothing. Re-entering the shop after
//                                   buying must not offer to sell them again.
//   2. TrySpendSauce(Price)         which IS the affordability check - it refuses and
//                                   leaves the wallet untouched when short. Asking "can
//                                   he afford it" separately would be a second copy of
//                                   the same rule that could disagree with the first.
//                                   (ACoffeeCup records this; same call, same reason.)
//   3. ClaimOneTimeGrant            claim-and-save in one, so the purchase survives a
//                                   quit at the shop door.
//
// The grant is the ONLY thing bought. Not an item, not a carried object - because what
// the rest of the game asks is "may he board", and that is a permission, not a rucksack.
// If supplies ever need to be visible or loseable, add the item then; a grant does not
// have to be undone to be joined.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "SupplyCounter.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class SIBELIUSGAME_API ASupplyCounter : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ASupplyCounter();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	/** True once the supplies are bought — asked by the rocket, and by Nyra's stage 3. */
	UFUNCTION(BlueprintPure, Category = "Supplies")
	static bool HasSupplies(const UObject* WorldContext);

	/** The one grant this whole errand exists to produce. */
	static const FName SuppliesGrant;

protected:
	/* An empty root that nothing is measured against but everything hangs off. Making the
	   trigger sphere the root is what sent three coffee cups to the world origin. */
	UPROPERTY(VisibleAnywhere, Category = "Supplies")
	TObjectPtr<USceneComponent> Root;

	/** Optional — the counter is usually the shop's own cashier table, already modelled. */
	UPROPERTY(VisibleAnywhere, Category = "Supplies")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/* WHAT THE INTERACT TRACE FINDS. Query-only and blocking: felt by the trace, walked
	   through by the player. A counter he cannot walk into but also cannot see is worse
	   than either. */
	UPROPERTY(VisibleAnywhere, Category = "Supplies")
	TObjectPtr<USphereComponent> Reach;

	UPROPERTY(EditAnywhere, Category = "Supplies", meta = (ClampMin = "0"))
	float ReachRadius = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Supplies")
	float ReachHeight = 90.0f;

	/* WHAT IT COSTS.

	   The first draft justified a low price with "there is no way to grind Sauce in this
	   game, so a player who cannot afford the only path forward is stuck". Walt: "if they
	   need more sauce they can go back to the office." Which is true, and it is the whole
	   shape of the game - the office is where the work is. Being short is a detour, not a
	   dead end, so this number is free to mean something.

	   250 against a wallet that reads in the thousands by the time a spaceport exists.
	   Raise it if the errand should cost a trip home. */
	UPROPERTY(EditAnywhere, Category = "Supplies", meta = (ClampMin = "0"))
	int32 Price = 250;

	UPROPERTY(EditAnywhere, Category = "Supplies")
	FText Prompt = NSLOCTEXT("Sibelius", "SupplyCounterPrompt", "Buy all supplies [E]");

	UPROPERTY(EditAnywhere, Category = "Supplies")
	FText DonePrompt = NSLOCTEXT("Sibelius", "SupplyCounterDone", "You have your supplies");

	UPROPERTY(EditAnywhere, Category = "Supplies")
	FString BoughtLine = TEXT("Supplies bought.  You are provisioned for the planet Grok.");

	/* {0} is replaced with the price, so the number can never disagree with Price.

	   AND IT NAMES THE NEXT MOVE, which is this project's standing rule for a refusal —
	   the [>] city key records it: "A locked key that says 'locked' teaches nothing. This
	   one names the obstacle AND the next move." Sauce is earned at the office, so a
	   refusal that only says "you are poor" hides the answer the player already owns. */
	UPROPERTY(EditAnywhere, Category = "Supplies")
	FString TooPoorLine = TEXT("You need {0} Sauce for a voyage's worth of supplies.  Press O and go back to the office to earn some.");
};
