// ReturnDoor.h
//
// THE SAUCE DOOR — the way home from an Elsewhere (SIB-47, §3 step 6). E to leave:
// it discards the staged plan (the room is throwaway — §4) and travels back to the
// house. The Elsewhere level unloads on travel, so the generated geometry is gone
// for free; DiscardStagedElsewhere just clears the in-memory staging.
//
// The builder spawns one of these in every Elsewhere so there's always a way back.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "ReturnDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API AReturnDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AReturnDoor();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	// Where "home" is — the house with the Cabinet. Defaults to the office/house map.
	UPROPERTY(EditAnywhere, Category = "Return Door")
	FName HomeLevelName = TEXT("L_Office_v02");

	UPROPERTY(EditAnywhere, Category = "Return Door")
	FText ReturnPromptText = NSLOCTEXT("Sibelius", "ReturnDoorPrompt", "Return through the door [E]");

	UPROPERTY(VisibleAnywhere, Category = "Return Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;
};
