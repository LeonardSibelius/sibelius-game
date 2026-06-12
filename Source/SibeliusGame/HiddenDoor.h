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
#include "Interactable.h"
#include "HiddenDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialParameterCollection;
class UTexture2D;

UCLASS()
class SIBELIUSGAME_API AHiddenDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHiddenDoor();

	// Headless self-test for the smoke commandlet: proves CV4 (collision +
	// visibility track the state in BOTH directions) without needing a spawned
	// player pawn. Leaves the door in the inactive (wall) state.
	bool RunCollisionSelfTest();

	// --- SIB-44: the obelisk IS the gate to world two -----------------------
	// When revealed (Code Vision held) AND TravelTargetLevel is set, the
	// revealed panel takes E directly: prompt shows, E travels. Unrevealed =
	// no prompt, no travel — the secret keeps itself. Same branch guard as
	// the cathedral doors.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "Hidden Door|Travel")
	FName TravelTargetLevel = NAME_None;   // e.g. L_Stacks; None = no travel

	UPROPERTY(EditAnywhere, Category = "Hidden Door|Travel")
	FText TravelPromptText = NSLOCTEXT("Sibelius", "HiddenDoorTravelPrompt", "Enter the Stacks [E]");

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

	// --- SIB-44: the inscription (world-two gate sign) -----------------------
	// "AI is the Sauce of All Knowledge. It is cooking behind this door."
	// A glowing unlit plane ATTACHED TO DoorMesh — ApplyState's
	// SetVisibility(..., propagate=true) reveals it with the door, so the sign
	// exists only in Code Vision (speak-friend-and-enter). Configured in
	// OnConstruction only when SignTexture is assigned; assignment on the
	// placed actor is also what gets the texture COOKED (the PK16 lesson).
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, Category = "Hidden Door|Sign")
	TObjectPtr<UTexture2D> SignTexture;

	// Defaults = the values Walt scrubbed to on HiddenDoor_1 (June 12, the
	// great axis hunt). Future doors are born with their sign hung straight.
	UPROPERTY(EditAnywhere, Category = "Hidden Door|Sign")
	FVector SignRelativeLocation = FVector(0.0f, 70.0f, 10.0f);

	// Explicit FRotator(Pitch, Yaw, Roll). WARNING from the hunt: pitch=90
	// combos gimbal-lock the other two knobs — roll-only (90,0,0 in the
	// Details X field) is the working orientation for the engine Plane on
	// this door. Nudge in Details, never recompile.
	UPROPERTY(EditAnywhere, Category = "Hidden Door|Sign")
	FRotator SignRelativeRotation = FRotator(0.0f, 0.0f, 90.0f);   // Details shows X(roll)=90

	UPROPERTY(EditAnywhere, Category = "Hidden Door|Sign")
	float SignWidth = 100.0f;   // cm; art is 2:1 (height = width/2 when SignHeight=0)

	// 0 = automatic (width/2, the art's native shape). Set a value to stretch
	// the plaque vertically — the lettering stretches with it (320 on the
	// tall office door reads as stately rather than distorted).
	UPROPERTY(EditAnywhere, Category = "Hidden Door|Sign")
	float SignHeight = 320.0f;

	UPROPERTY(VisibleAnywhere, Category = "Hidden Door|Sign")
	TObjectPtr<UStaticMeshComponent> SignMesh;

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
