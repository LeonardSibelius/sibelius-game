// CodeVisionComponent.cpp
//
// SIB-25 — Ch1 Code Vision component. See header for the single-source-of-truth
// rationale.

#include "CodeVisionComponent.h"

#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"

UCodeVisionComponent::UCodeVisionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCodeVisionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Start ordinary. Broadcast once so any pre-placed subscribers (door/board)
	// snap to the inactive state.
	bIsActive = false;
	CurrentBlend = 0.f;
	ApplyBlendToMPC();
	OnCodeVisionChanged.Broadcast(false);
}

void UCodeVisionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// CV3 / CV14: never leave the world in the revealed state.
	SetCodeVisionActive(false);
	Super::EndPlay(Reason);
}

void UCodeVisionComponent::SetCodeVisionActive(bool bNewActive)
{
	if (bIsActive == bNewActive)
	{
		return; // idempotent — guards against double-apply (CV4/CV10)
	}

	bIsActive = bNewActive;

	// Collision + visual state flip on the HARD boolean, via subscribers.
	OnCodeVisionChanged.Broadcast(bIsActive);
}

void UCodeVisionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Smoothly drive the MPC scalar toward the target so the overlay/reveal
	// materials fade rather than pop (CV10).
	const float Target = bIsActive ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(CurrentBlend, Target))
	{
		CurrentBlend = FMath::FInterpConstantTo(CurrentBlend, Target, DeltaTime, BlendSpeed);
		ApplyBlendToMPC();
	}
}

void UCodeVisionComponent::ApplyBlendToMPC()
{
	if (CodeVisionMPC)
	{
		// The one bridge between game state and every Code Vision material (CV7).
		UKismetMaterialLibrary::SetScalarParameterValue(this, CodeVisionMPC,
			ActiveParameterName, CurrentBlend);
	}
}
