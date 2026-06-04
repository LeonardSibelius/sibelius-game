// Ch3 - Compile (SIB-27). Player-side build driver (B); E-verbs go through IInteractable.
// Lives on SibeliusGameCharacter (no actor-finds-player races - Ch1/R7 lesson).
// Holds NO per-site state; sites own their own state (Ch2 principle).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuildComponent.generated.h"

class ABuildSite;
class UInventoryComponent;

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API UBuildComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Bound to IA_Build (B). Builds the current proximate site if affordable. (C12: no-op otherwise.)
	UFUNCTION(BlueprintCallable, Category = "Build")
	void TriggerBuild();

	// Collect / unlock / dismantle on E run through the shared IInteractable system
	// (UInteractorComponent) - the Ch3 actors implement the interface directly (C7).

	// How often we re-scan for the nearest build site (timer, not tick).
	UPROPERTY(EditAnywhere, Category = "Build", meta = (ClampMin = "0.05"))
	float SiteScanInterval = 0.25f;

private:
	void ScanForSite();
	UInventoryComponent* GetInventory() const;

	TWeakObjectPtr<ABuildSite> CurrentSite;
	FTimerHandle SiteScanTimer;
};
