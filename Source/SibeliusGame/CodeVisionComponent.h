// CodeVisionComponent.h
//
// SIB-25 — Ch1 Code Vision. The SINGLE source of truth for the mechanic's
// on/off state. Lives on SibeliusGameCharacter. Nobody else stores a copy of
// the boolean — the door, the board, and the post-process all read this
// component (via OnCodeVisionChanged or the getter). That is the fix for the
// #1 risk here: visual/collision drift (CV4/CV8).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CodeVisionComponent.generated.h"

class UMaterialParameterCollection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCodeVisionChanged, bool, bIsActive);

UCLASS(ClassGroup=(Sibelius), meta=(BlueprintSpawnableComponent))
class SIBELIUSGAME_API UCodeVisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCodeVisionComponent();

	// THE one switch. Idempotent. Drives the MPC scalar + broadcasts the change.
	UFUNCTION(BlueprintCallable, Category = "Code Vision")
	void SetCodeVisionActive(bool bNewActive);

	// Convenience for the input bindings (hold = Started/Completed).
	UFUNCTION(BlueprintCallable, Category = "Code Vision")
	void ActivateCodeVision() { SetCodeVisionActive(true); }

	UFUNCTION(BlueprintCallable, Category = "Code Vision")
	void DeactivateCodeVision() { SetCodeVisionActive(false); }

	UFUNCTION(BlueprintPure, Category = "Code Vision")
	bool IsCodeVisionActive() const { return bIsActive; }

	// Door, investigation board, etc. subscribe here. Fires on every change.
	UPROPERTY(BlueprintAssignable, Category = "Code Vision")
	FOnCodeVisionChanged OnCodeVisionChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// The collection both the post-process overlay and the reveal materials read.
	UPROPERTY(EditAnywhere, Category = "Code Vision")
	TObjectPtr<UMaterialParameterCollection> CodeVisionMPC;

	UPROPERTY(EditAnywhere, Category = "Code Vision")
	FName ActiveParameterName = TEXT("Active");

	// Visual fade speed (units/sec) for the MPC scalar 0<->1. Note: COLLISION
	// flips on the hard bIsActive boolean, not on this blend (CV10/CV4).
	UPROPERTY(EditAnywhere, Category = "Code Vision", meta = (ClampMin = "0.5"))
	float BlendSpeed = 6.f;

	bool bIsActive = false;
	float CurrentBlend = 0.f;

	void ApplyBlendToMPC();
};
