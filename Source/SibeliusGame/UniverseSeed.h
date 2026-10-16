// UniverseSeed.h
//
// THE SEED - upstairs in the office, a grain of light floating over a slim pedestal (Walt,
// 2026-10-15: "the player goes upstairs and sees an object to "E" on, not an agent ... Player
// must be granted all the AI powers before they can enter"). The games downstairs are the
// ones Leonard grew up with; this is the one the AIs made.
//
// With fewer than six powers, E only says how many he has. With all six, the grain swells
// until it is all he can see, and he arrives in the AI city (L_Genesis, AAICity): a city that
// is also a mind, wrapped round a star, where the AIs grow universes to amuse themselves.
//
// Built from the engine's shapes and /Game/Genesis's two materials (make_genesis_assets.py),
// like the basketball court: nothing bought, so it can be rendered and judged by eye.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "UniverseSeed.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class SIBELIUSGAME_API AUniverseSeed : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AUniverseSeed();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<USceneComponent> SceneRoot;

	/** A square foot, a slim column and a shallow dish: dark stone, so the grain is the light. */
	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<UStaticMeshComponent> Column;

	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<UStaticMeshComponent> Dish;

	/** The grain itself, a few centimetres of light above the dish. */
	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<UStaticMeshComponent> Grain;

	/** A breath of glass round it, brightest at its edge. */
	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<UStaticMeshComponent> Halo;

	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<UPointLightComponent> Glow;

	/** What the interaction trace lands on. Visibility only: he can walk round it. */
	UPROPERTY(VisibleAnywhere, Category = "Seed")
	TObjectPtr<UBoxComponent> Reach;

	UPROPERTY(EditAnywhere, Category = "Seed")
	FName CityLevel = TEXT("L_Genesis");

	/** The PlayerStart tag he arrives at in the AI city (place_genesis.py). */
	UPROPERTY(EditAnywhere, Category = "Seed")
	FName ArrivalTag = TEXT("GenesisGallery");

	/** How long the grain takes to fill his view before he goes. */
	UPROPERTY(EditAnywhere, Category = "Seed", meta = (ClampMin = "0.2"))
	float SwellSeconds = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Seed")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, Category = "Seed")
	TObjectPtr<UMaterialInterface> GlassMaterial;

	/** M_GoSurface (Color x Pattern): the stone. */
	UPROPERTY(EditAnywhere, Category = "Seed")
	TObjectPtr<UMaterialInterface> StoneMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void Paint();
	int32 PowersHeld() const;

	float Age = 0.0f;
	float Swell = -1.0f;     // below zero: not swelling; 0..1 while it fills his view
	bool bTravelled = false;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GrainLight;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HaloGlass;
};
