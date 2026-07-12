// Curio.h
//
// THE SAUCE DOOR — the one glowing collectable in an Elsewhere (SIB-47, §6). Mirrors
// ABookPickup: E to collect via the shared IInteractable trace, destroys itself on
// collect (no dupes within a room). Unlike a book it writes to the persistent
// UCurioCollectionSubsystem (the Cabinet), not the run inventory.
//
// The curio reads as treasure two ways: a child point light tinted to the place's mood
// color (a colored halo), AND an emissive glow MATERIAL on the mesh (a dynamic instance
// of GlowMaterial tinted to that color) so the payoff object actually glows and never
// shows the missing-material checker. Mesh is a default-subobject sphere (the null-proxy
// lesson) — a clean glowing orb; SetDisplayMesh can swap in real art per curio later.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "Curio.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;

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

protected:
	// SIB Forest: when this curio is HAND-PLACED on a pre-made level (no builder Configure), it
	// self-configures from the staged Elsewhere plan on BeginPlay — so a forest visit's curio still
	// varies per seed, and Collect -> the Cabinet works identically to the builder-spawned cathedral
	// curio. Falls back to DefaultCurioId when no plan is staged (e.g. PIE-ing the level directly).
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Curio") FName DefaultCurioId;
	UPROPERTY(EditAnywhere, Category = "Curio") FName DefaultPlaceTypeId;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curio") FName CurioId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curio") FName PlaceTypeId;

	UPROPERTY(VisibleAnywhere, Category = "Curio") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Curio") TObjectPtr<UPointLightComponent> Glow;

	// Emissive base for the glowing-relic look (tinted to the curio color in Configure).
	// Defaults to the Server Cathedral kit's emissive material; overridable in editor.
	// If it fails to load (kit absent), the mesh keeps its valid default material — a
	// grey orb, never the checker.
	UPROPERTY(EditAnywhere, Category = "Curio") TSoftObjectPtr<UMaterialInterface> GlowMaterial;

	// Emissive strength fed to the material's "Intens" param.
	UPROPERTY(EditAnywhere, Category = "Curio") float GlowEmissiveIntensity = 9.0f;

	// FUN-5: a found curio quietly pays Sauce — the Many Worlds reward wandering
	// without a checklist (no counter, no cabinet UI; the walk stays the point).
	// Priced above a book: reaching one means riding the door and looking around.
	UPROPERTY(EditAnywhere, Category = "Curio", meta = (ClampMin = "0"))
	int32 SauceOnCollect = 15;

private:
	bool bCollected = false;   // re-entrancy guard
};
