// SauceDoor.h
//
// THE SAUCE DOOR — the "Many Worlds" kitchen door (SIB-47, §3). A subclass of
// AHiddenDoor, so it REUSES the game's existing hidden-door reveal exactly: hold Code
// Vision (V) and the panel shimmers into view (custom-depth outline), same as the
// office "Sauce of All Knowledge" door and the attic "Carousel of Fates" door. The
// only difference from a plain hidden door is what E does: instead of opening a fixed
// TravelTargetLevel, it asks the UElsewhereSubsystem to roll + stage a fresh Elsewhere,
// then steps through into the generation map (§8: a variant of the travel door).
//
// Arming == revealed: there is no separate Sauce-completion gate here (the office has
// no Sauce-feed); the door is "armed" whenever Code Vision reveals it, matching every
// other hidden door in the game.

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

	// Reveal-gated travel: when revealed (Code Vision held), E stages a fresh Elsewhere
	// and steps through. Unrevealed = a plain wall (no prompt, no travel).
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	// The generation map the door travels into (built there from the staged plan).
	UPROPERTY(EditAnywhere, Category = "Sauce Door|Travel")
	FName ElsewhereLevelName = TEXT("L_Elsewhere");

	UPROPERTY(EditAnywhere, Category = "Sauce Door|Travel")
	FText StepThroughPrompt = NSLOCTEXT("Sibelius", "SauceDoorPrompt", "Step through [E]");
};
