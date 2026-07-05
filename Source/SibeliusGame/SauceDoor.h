// SauceDoor.h
//
// THE SAUCE DOOR — the "Many Worlds" kitchen door. A subclass of AHiddenDoor, so it
// REUSES the game's existing hidden-door reveal exactly: hold Code Vision (V) and the
// panel shimmers into view (custom-depth outline), same as the office "Sauce of All
// Knowledge" door and the attic "Carousel of Fates" door.
//
// TRAVEL (Plan B — the Deck of Worlds): the door holds an editable DECK of baked
// forest levels (TravelTargetLevels). On each interact it picks one at random —
// never the same twice in a row (session-scoped memory; a static survives the
// office level reloading between visits). The pick is written into the inherited
// TravelTargetLevel and the parent's travel path does the rest, so branch-gating,
// the travel cover, and the smoke-tested reveal behavior are all unchanged.
// An EMPTY deck = the old single-target behavior (TravelTargetLevel as placed).
//
// Arming == revealed: there is no separate Sauce-completion gate here; the door is
// "armed" whenever Code Vision reveals it, matching every other hidden door.

#pragma once

#include "CoreMinimal.h"
#include "HiddenDoor.h"
#include "SauceDoor.generated.h"

UCLASS()
class SIBELIUSGAME_API ASauceDoor : public AHiddenDoor
{
	GENERATED_BODY()

public:
	ASauceDoor();

	// The deck: baked forest levels this door shuffles between (L_Forest_01, ...).
	// Set on the PLACED door in the office level. Empty = plain single-target door.
	UPROPERTY(EditAnywhere, Category = "Travel")
	TArray<FName> TravelTargetLevels;

protected:
	virtual void Interact_Implementation(AActor* Interactor) override;
};
