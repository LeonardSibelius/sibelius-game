// SauceDoor.h
//
// THE SAUCE DOOR — the kitchen trigger (SIB-47, §3). A sibling of AHiddenDoor: it
// reuses the reveal pattern (a panel that reads as wall, then shimmers in and becomes
// the passable, E-takeable travel door) but is ARMED by the Sauce of All Knowledge,
// not Code Vision. When armed and taken, it does NOT open a fixed level — it asks the
// UElsewhereSubsystem to roll + stage a fresh Elsewhere, then travels into the
// generation map (§8: a variant of the travel door).
//
//   Unarmed -> panel hidden, box blocks (reads as wall), no prompt.
//   Armed   -> panel revealed (shimmer), box passable but takes the interaction
//              trace, prompt shows, E stages an Elsewhere + travels.
//
// Arming source: bound to an ASauceCauldron's OnSauceComplete (using the Sauce arms
// the door), or bStartArmed for a standalone test kitchen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "SauceDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class ASauceCauldron;

UCLASS()
class SIBELIUSGAME_API ASauceDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ASauceDoor();

	// Manifest the door (the Sauce cracked it open). Idempotent. Drives visual +
	// collision through the single ApplyState path (the HiddenDoor CV4 discipline).
	UFUNCTION(BlueprintCallable, Category = "Sauce Door")
	void Arm();

	UFUNCTION(BlueprintPure, Category = "Sauce Door")
	bool IsArmed() const { return bArmed; }

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	// The generation map the door travels into. The Elsewhere is built there from the
	// staged plan, then discarded on return. (Authored as an editor follow-up; the
	// gate never opens a level.)
	UPROPERTY(EditAnywhere, Category = "Sauce Door|Travel")
	FName ElsewhereLevelName = TEXT("L_Elsewhere");

	UPROPERTY(EditAnywhere, Category = "Sauce Door|Travel")
	FText TravelPromptText = NSLOCTEXT("Sibelius", "SauceDoorPrompt", "Step through [E]");

	// Bind here to arm the door when this cauldron's Sauce completes.
	UPROPERTY(EditAnywhere, Category = "Sauce Door")
	TObjectPtr<ASauceCauldron> Cauldron;

	// Test/standalone: arm at BeginPlay with no cauldron.
	UPROPERTY(EditAnywhere, Category = "Sauce Door")
	bool bStartArmed = false;

	// Headless gate helper: proves arm gates the prompt AND that the reveal drives
	// collision both ways (the HiddenDoor self-test, for this door). Leaves it unarmed.
	bool RunArmGateSelfTest();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleSauceComplete();

	// THE single path (CV4): visibility + collision move together with the armed state.
	void ApplyState(bool bRevealed);

	UPROPERTY(VisibleAnywhere, Category = "Sauce Door") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Sauce Door") TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleAnywhere, Category = "Sauce Door") TObjectPtr<UBoxComponent> BlockingBox;

private:
	bool bArmed = false;
};
