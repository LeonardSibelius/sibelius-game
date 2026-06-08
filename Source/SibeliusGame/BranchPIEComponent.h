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
	void Debug_ClearDeploy(); // wipe the deploy save so the next load starts authored-clean

	// Debug (SIB-37): true while the load-time apply input-gate is holding input off.
	bool IsLoadInputGated() const { return bLoadInputGated; }

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

	// Ch5 PIE hook (SIB-37): on level load-complete, re-apply the deployed save at
	// depth 0 BEFORE the player can act, gating input until it returns (spike D2).
	// Deferred one tick so it runs AFTER every branchable's BeginPlay (some reset
	// their own state there) — the deployed deltas then land on top.
	void ApplyDeployedOnLoad();
	void SetPlayerInputEnabled(bool bEnabled);

	FDelegateHandle DepthHandle;
	bool bLoadInputGated = false; // SIB-37: tracks whether apply-on-load currently holds input off

	// Saturation lost per depth level (0..1). depth 0 = full colour, deeper = greyer.
	// 0.75 → depth 1 is strongly drained (0.25 sat), depth 2+ full greyscale (clamped).
	// Paired with a cold-blue gain + contrast lift in ApplyDesaturation for an
	// unmistakable "different reality" read while branched.
	UPROPERTY(EditAnywhere, Category = "Branch")
	float SaturationPerDepth = 0.75f;
};
