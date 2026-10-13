// FreakbookItem.h
//
// A thing one of the Freakbook five wants, lying somewhere in the house (docs/FREAKBOOK.md):
// Dee's binder in the attic, and - wherever Walt puts them - Gunther's coconut and Brent's belt.
// E picks it up; then the laptop's Freakbook can send it to them, and they post about it.
//
// The game decides, never a model: this actor and the profile's button are the only way a want
// comes true. Once taken (or already sent), the thing is gone from the house for good - it asks
// UFreakbookSubsystem on BeginPlay and whenever the state changes.
//
// Plain shapes, ours, in the game's palette: a blue binder with its label, a brown coconut, a gold
// belt. Placed by Tools/Scripts/place_freakbook_binder.py.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "FreakbookItem.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class SIBELIUSGAME_API AFreakbookItem : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AFreakbookItem();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	/** Whose it is: "dee" (the binder), "gunther" (the coconut) or "brent" (the belt). */
	UPROPERTY(EditAnywhere, Category = "Freakbook")
	FName Person = TEXT("dee");

	UPROPERTY(VisibleAnywhere, Category = "Freakbook")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Freakbook")
	TObjectPtr<UTextRenderComponent> Label;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Shape, colour and label for whose it is. */
	void Dress();
	/** Gone if taken or sent; there otherwise. */
	void Refresh();

	FDelegateHandle ChangedHandle;
};
