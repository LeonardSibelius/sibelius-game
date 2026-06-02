// RefactorComponent.cpp
//
// SIB-26 — Ch2 Refactor. See header.

#include "RefactorComponent.h"
#include "RefactorableComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

URefactorComponent::URefactorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URefactorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CurrentTarget = TraceForRefactorable();
}

URefactorableComponent* URefactorComponent::TraceForRefactorable() const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return nullptr;
	}
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}

	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PC->PlayerCameraManager->GetCameraRotation().Vector() * TraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			// R4: only ever return an actor that is actually refactorable.
			return HitActor->FindComponentByClass<URefactorableComponent>();
		}
	}
	return nullptr;
}

void URefactorComponent::TriggerRefactor()
{
	if (CurrentTarget)
	{
		CurrentTarget->ToggleRefactor();
	}
}
