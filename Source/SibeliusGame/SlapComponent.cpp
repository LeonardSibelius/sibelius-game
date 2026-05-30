#include "SlapComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

USlapComponent::USlapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlapComponent::DoSlap()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector ViewStart = OwnerPawn->GetActorLocation();
	FRotator ViewRot = OwnerPawn->GetActorRotation();

	if (AController* Controller = OwnerPawn->GetController())
	{
		Controller->GetPlayerViewPoint(ViewStart, ViewRot);
	}

	const FVector Forward = ViewRot.Vector();
	const FVector ViewEnd = ViewStart + Forward * SlapRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlapSweep), false, OwnerPawn);

	TArray<FHitResult> Hits;
	const bool bAnyHit = World->SweepMultiByChannel(
		Hits,
		ViewStart,
		ViewEnd,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SlapRadius),
		Params);

	if (bDebugDraw)
	{
		DrawDebugLine(World, ViewStart, ViewEnd, FColor::Red, false, 2.0f, 0, 1.5f);
	}

	if (!bAnyHit)
	{
		return;
	}

	for (const FHitResult& Hit : Hits)
	{
		ACharacter* Victim = Cast<ACharacter>(Hit.GetActor());
		if (!Victim || Victim == OwnerPawn)
		{
			continue;
		}

		if (AController* VictimController = Victim->GetController())
		{
			VictimController->StopMovement();
			VictimController->UnPossess();
		}

		if (UCapsuleComponent* Capsule = Victim->GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (USkeletalMeshComponent* Mesh = Victim->GetMesh())
		{
			Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
			Mesh->SetSimulatePhysics(true);

			const FVector VictimLocation = Victim->GetActorLocation();
			const FVector LaunchDir = (VictimLocation - ViewStart).GetSafeNormal();
			const FVector Impulse = LaunchDir * LaunchSpeed + FVector::UpVector * UpwardSpeed;
			Mesh->AddImpulse(Impulse, NAME_None, true);

			// Despawn the ragdolled victim after a delay.
			Victim->SetLifeSpan(RagdollLifetime);
		}

		if (SlapSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, SlapSound, Hit.ImpactPoint);
		}

		break;
	}
}
