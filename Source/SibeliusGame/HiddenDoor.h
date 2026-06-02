// HiddenDoor.h
//
// SIB-25 — Ch1 Code Vision. A wall segment that reads as solid wall normally
// and becomes a revealed, passable door while Code Vision is held.
//
//   Vision OFF -> DoorMesh hidden, BlockingBox blocks (reads as wall)
//   Vision ON  -> DoorMesh revealed (custom-depth outline), BlockingBox passable
//
// One code path (ApplyState) drives BOTH visual and collision — the CV4/CV8
// guard against drift.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HiddenDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialParameterCollection;

UCLASS()
class SIBELIUSGAME_API AHiddenDoor : public AActor
{
	GENERATED_BODY()

public:
	AHiddenDoor();

	// Headless self-test for the smoke commandlet: proves CV4 (collision +
	// visibility track the state in BOTH directions) without needing a spawned
	// player pawn. Leaves the door in the inactive (wall) state.
	bool RunCollisionSelfTest();

protected:
	virtual void BeginPlay() override;

	// Drives the MPC fallback poll (see CodeVisionMPC below). Tick stays cheap:
	// a no-op once bound to the component delegate.
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void HandleCodeVisionChanged(bool bIsActive);

	// THE single path: drives visual + collision together.
	void ApplyState(bool bRevealed);

	UPROPERTY(VisibleAnywhere, Category = "Hidden Door")
	TObjectPtr<USceneComponent> SceneRoot;

	// The glowing door panel — revealed in Code Vision. Visual only.
	UPROPERTY(VisibleAnywhere, Category = "Hidden Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	// Blocks the opening when Code Vision is OFF (reads as solid wall);
	// collision disabled when ON (passable).
	UPROPERTY(VisibleAnywhere, Category = "Hidden Door")
	TObjectPtr<UBoxComponent> BlockingBox;

private:
	// Binds to the player's UCodeVisionComponent; retries for a late pawn spawn
	// (the office can spawn the pawn after the door's BeginPlay).
	void TryBindToPlayer();

	// --- Robust fallback (CV4/CV8): read the same source of truth a different way.
	// The MPC is driven exclusively by UCodeVisionComponent, so polling its 'Active'
	// scalar reveals the door even if we never resolve the player pawn. Used only
	// while the delegate binding hasn't taken over (bBoundToComponent == false).
	UPROPERTY(EditAnywhere, Category = "Hidden Door|Fallback")
	TObjectPtr<UMaterialParameterCollection> CodeVisionMPC;

	UPROPERTY(EditAnywhere, Category = "Hidden Door|Fallback")
	FName ActiveParameterName = TEXT("Active");

	// Blend crosses this -> reveal. Matches the component's 0<->1 MPC scalar.
	UPROPERTY(EditAnywhere, Category = "Hidden Door|Fallback", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float RevealThreshold = 0.5f;

	FTimerHandle BindRetryHandle;
	int32 BindAttempts = 0;

	// True once AddDynamic succeeded — the delegate is then authoritative and the
	// tick fallback short-circuits.
	bool bBoundToComponent = false;

	// Last state we pushed through ApplyState; avoids redundant per-tick work and
	// gates the diagnostic logging.
	bool bRevealedState = false;
};
