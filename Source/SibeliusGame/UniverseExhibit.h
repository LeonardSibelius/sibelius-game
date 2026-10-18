// UniverseExhibit.h
//
// THE AI CITY - a universe of his own, on display (GPT-6 Astra, 2026-10-17; tidied the same day).
// At the forge, K keeps a finished universe in one of three gallery slots
// (UProgressionSubsystem::KeepUniverse), and each slot has one of these in the gallery: a glass orb
// on a dark plinth, its stars arranged in the shape of how that universe ended, a label, and E to
// watch it again at the forge.
//
// AAICity::BeginPlay spawns the three rather than L_Genesis holding them, so the gallery needs no
// map resave and the seven authored universes stay exactly as they are. An exhibit shows the SAVED
// record - the outcome as it was when he kept it - never a fresh run of today's simulation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "UniverseExhibit.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class SIBELIUSGAME_API AUniverseExhibit : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AUniverseExhibit();

	/** Which gallery slot this plinth shows, 0 to UProgressionSubsystem::UniverseSlots - 1. */
	void SetSlot(int32 InSlot);

	/** Read the slot from the save again and dress to match: the forge calls it after a keep. */
	void Refresh();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

private:
	int32 Slot = 0;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Orb;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pedestal;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Rim;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Stars;

	UPROPERTY()
	TObjectPtr<UBoxComponent> Reach;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	// Held from the constructor, as AAICity and AUniverseForge hold theirs, so the cooker always
	// takes them: a material only loaded by path at run time is there only if something else
	// happened to cook it.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> GlassMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SurfaceMaterial;
};
