// Curio.h
//
// THE SAUCE DOOR — the one glowing collectable in an Elsewhere (SIB-47, §6). Mirrors
// ABookPickup: E to collect via the shared IInteractable trace, destroys itself on
// collect (no dupes within a room). Unlike a book it writes to the persistent
// UCurioCollectionSubsystem (the Cabinet), not the run inventory.
//
// The "glow" is a child point light tinted to the place's mood color — reads as the
// hero object with zero authored materials (runtime-safe; the null-proxy lesson: the
// mesh is a default subobject so it actually renders).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "Curio.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class SIBELIUSGAME_API ACurio : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACurio();

	// Set the identity + glow before/after spawn (the builder calls this from the
	// staged plan). PlaceTypeId is carried so the Cabinet can group by origin.
	UFUNCTION(BlueprintCallable, Category = "Curio")
	void Configure(FName InCurioId, FName InPlaceTypeId, FLinearColor GlowColor);

	// Swap the placeholder sphere for the curio's real mesh (from FCurioDef::Mesh).
	// Null leaves the default sphere (an undressed curio still reads as collectable).
	UFUNCTION(BlueprintCallable, Category = "Curio")
	void SetDisplayMesh(UStaticMesh* InMesh, float UniformScale = 0.6f);

	// Adds to the collection subsystem and destroys this curio. Returns true if it
	// actually collected (false if already collected or no game instance). Split out
	// so the smoke gate can drive it without an interactor pawn.
	bool Collect(UObject* WorldContext);

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curio") FName CurioId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curio") FName PlaceTypeId;

	UPROPERTY(VisibleAnywhere, Category = "Curio") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Curio") TObjectPtr<UPointLightComponent> Glow;

private:
	bool bCollected = false;   // re-entrancy guard
};
