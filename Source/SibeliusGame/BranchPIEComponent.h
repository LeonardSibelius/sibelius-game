// BranchPIEComponent.h
//
// SIB-36 — PIE-side consumers of UBranchSubsystem. Integration only, no branch
// logic: it READS the subsystem's published state and reacts.
//   - debug keys F6-F9 drive the FSM (Enter/Merge/Discard/RequestDeploy) with
//     on-screen result text;
//   - while branched: desaturate the camera (greyer with depth), show a HUD
//     "BRANCH xN" marker, make BookPickups inert, and freeze Refusers (spike T2).
// Lives on the player character (native subobject).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BranchPIEComponent.generated.h"

class UBranchSubsystem;

UCLASS(ClassGroup = (Sibelius), meta = (BlueprintSpawnableComponent))
class SIBELIUSGAME_API UBranchPIEComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBranchPIEComponent();

	// Bound to F6-F9 by the owning character. Each drives the FSM + reports on screen.
	void Debug_Enter();
	void Debug_Merge();
	void Debug_Discard();
	void Debug_Deploy();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UBranchSubsystem* GetBranch() const;
	void OnDepthChanged(int32 Depth);

	void ApplyDesaturation(int32 Depth);
	void UpdateHudMarker(int32 Depth);
	void SetPickupsInert(bool bInert);
	void FreezeRefusers(bool bFreeze);
	void Toast(const FString& Msg, const FColor& Color) const;

	FDelegateHandle DepthHandle;

	// Saturation lost per depth level (0..1). depth 0 = full colour, deeper = greyer.
	// 0.5 → depth 1 reads as clearly muted, depth 2 as full greyscale (clamped).
	UPROPERTY(EditAnywhere, Category = "Branch")
	float SaturationPerDepth = 0.5f;
};
