// CoffeeCup.h — the first thing Leonard buys that isn't a tool.
//
// ===========================================================================
// WHY A CUP OF COFFEE IS WORTH A C++ CLASS.
//
// Every other transaction in this game buys CAPABILITY. Sauce goes on power unlocks, a
// bigger Generate budget, a harder slap — things that make him better at the work. The
// cauldron is a shop for a man still trying to get out.
//
// He is out. Nyra walks him to a deli on a sunlit street and tells him to have some food
// before he explores, and the only thing to spend forty years of distilled knowledge on
// is a coffee. That is the whole point of it: it buys nothing, upgrades nothing, and
// persists nowhere. It is the first purely pleasant thing the game lets him do.
//
// So: no stat, no unlock, no PurchaseCounts entry. FSauceShop is deliberately NOT used —
// its entire apparatus exists to record and re-apply a lasting effect, and there isn't
// one. This is UProgressionSubsystem::TrySpendSauce and a line of dialogue.
//
// ---------------------------------------------------------------------------
// "SURPRISINGLY ADEQUATE" is printed on the sign above Jacob's Downtown Deli, in
// Downtown West, by an artist who never met this game. Nyra quotes it when she sends him
// in. The coffee quotes it back. Three times makes it the town's opinion of itself, and
// it costs nothing to arrange because the sign was already there.
//
// ---------------------------------------------------------------------------
// PLACING ONE. Place Actors panel -> search "Coffee Cup" -> drag it onto the counter in
// L_Cafe, then set its Mesh to a cup from RestaurantScene. It has no mesh by default and
// an unassigned one is INVISIBLE BUT STILL INTERACTABLE — the same trap that made the
// meadow door a prompt attached to nothing. BeginPlay logs a warning if you forget.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "CoffeeCup.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class SIBELIUSGAME_API ACoffeeCup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACoffeeCup();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// IInteractable: buy it on E.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	/* THE THING THE TRACE ACTUALLY HITS, and it is deliberately bigger than the food.

	   UInteractorComponent sweeps a 30 cm sphere and then applies OnSurfaceSlack: it
	   records the first thing it hits as "furniture" and only accepts an interactable
	   within 50 cm BEHIND that. The rule exists so a book resting on a table is still
	   reachable past the tabletop, and it works well for that.

	   It does not work for a mug in the MIDDLE of a table ringed by chairs. The sweep
	   hits a chair back first, the mug is well over 50 cm further on, and it is rejected
	   from every angle a person can stand - which is exactly what Walt hit: a prompt that
	   flashed once and never came back.

	   So the item carries its own generous sphere. A 22 cm ball around a 9 cm mug is hit
	   sooner and from more angles, which is enough to land inside the slack. Tuning the
	   shared OnSurfaceSlack instead would have changed the reach of every interactable in
	   the game to fix one table. */
	/* A PLAIN SCENE ROOT, and it is not decoration.

	   The first attempt made the Reach sphere the root and then called
	   SetRelativeLocation on it to lift it off the plate. For a ROOT component "relative"
	   means WORLD - it has no parent to be relative to - so instead of raising the sphere
	   16 cm, it teleported all three cups to world origin. Walt opened the level and the
	   burger and coffee were simply gone.

	   With an empty root, the mesh sits at zero (so the actor's location IS the food's
	   location, which is what anyone placing one expects) and the sphere hangs above it,
	   and both are relative to something that is genuinely their parent. */
	UPROPERTY(VisibleAnywhere, Category = "Coffee")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Coffee")
	TObjectPtr<USphereComponent> Reach;

	/* WHERE THE TARGET VOLUME SITS, and both numbers came from Walt aiming at it.

	   "If I aim the reticule ABOVE the burger and coffee then they work, but if I aim it
	   directly, the prompt is not there."

	   That is the whole diagnosis. Aiming above, the 30 cm sweep reaches clear air and
	   the item is the first thing it meets. Aiming at the food, the sweep grazes the
	   TABLETOP on the way in, and that first contact is what disqualifies the item under
	   OnSurfaceSlack. The item is not too small; it is in the wrong place - down in the
	   clutter instead of up in the air the player is already pointing at.

	   So the sphere is lifted off the plate. These are EditAnywhere and applied in
	   OnConstruction, so they update live in the viewport: select the cup, drag them, and
	   watch the wireframe ball move. Nobody should have to rebuild to tune a hitbox. */
	UPROPERTY(EditAnywhere, Category = "Coffee", meta = (ClampMin = "1"))
	float ReachRadius = 20.0f;

	/** Centimetres above the food. The air over a plate is where a person aims. */
	UPROPERTY(EditAnywhere, Category = "Coffee", meta = (ClampMin = "0"))
	float ReachHeight = 16.0f;

	/** Assign a cup from RestaurantScene. Empty = invisible, and BeginPlay says so. */
	UPROPERTY(VisibleAnywhere, Category = "Coffee")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/* CHEAP ON PURPOSE. A book pays 5 Sauce, so this is two books — enough to be a
	   transaction rather than a pickup, small enough that nobody has to weigh it against
	   a power. Anyone who reaches the city is carrying thousands. */
	UPROPERTY(EditAnywhere, Category = "Coffee", meta = (ClampMin = "0"))
	int32 Price = 10;

	/** Shown while he is looking at it; {0} becomes the price. */
	UPROPERTY(EditAnywhere, Category = "Coffee")
	FString PromptText = TEXT("Coffee — {0} Sauce [E]");

	/** What the game says when he drinks it. See the header on why it is this line. */
	UPROPERTY(EditAnywhere, Category = "Coffee")
	FString BoughtLine = TEXT("You drink it.  Surprisingly adequate.");

	/** When he cannot afford it; {0} becomes the price. */
	UPROPERTY(EditAnywhere, Category = "Coffee")
	FString TooPoorLine = TEXT("The coffee is {0} Sauce.  Come back when you have it.");

	/* THE CUP IS TAKEN AND DOES NOT COME BACK — this visit. It is hidden rather than
	   destroyed, and nothing is written to the save, so leaving and returning restocks
	   the counter. That is what a cafe does, and a permanent record of one coffee would
	   be a save field that means nothing to anybody. */
	UPROPERTY(EditAnywhere, Category = "Coffee")
	bool bTakenForThisVisit = false;
};
