#include "SlapComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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
#include "ProgressionSubsystem.h"   // FUN-2: slaps pay Sauce

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

		// Kill character movement BEFORE ragdolling. With the capsule's collision
		// gone the movement component finds no floor and free-falls the capsule;
		// bones without physics bodies are animated relative to that plummeting
		// component transform, so the skin stretches between the simulated bodies
		// (staying put in world space) and the falling capsule.
		if (UCharacterMovementComponent* Move = Victim->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
			Move->SetComponentTickEnabled(false);
		}

		// Pick which skeletal mesh to ragdoll. Prefer the default Character mesh
		// when it has a physics asset; otherwise find a MetaHuman body mesh (a
		// separate skeletal mesh component, not CharacterMesh0) that has one.
		USkeletalMeshComponent* Mesh = Victim->GetMesh();
		if (!Mesh || Mesh->GetPhysicsAsset() == nullptr)
		{
			USkeletalMeshComponent* PhysicsMesh = nullptr;
			TArray<USkeletalMeshComponent*> SkelMeshes;
			Victim->GetComponents<USkeletalMeshComponent>(SkelMeshes);
			for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
			{
				// Never simulate a leader-pose FOLLOWER (MetaHuman face/torso/
				// legs copy their bones from the body every frame) — physics
				// fighting leader pose is what distorts the mesh. Only a leader
				// with its own physics asset may ragdoll; followers ride along.
				if (SkelMesh && SkelMesh->GetPhysicsAsset() != nullptr
					&& SkelMesh->LeaderPoseComponent.Get() == nullptr)
				{
					PhysicsMesh = SkelMesh;
					break;
				}
			}
			Mesh = PhysicsMesh ? PhysicsMesh : Victim->GetMesh();
		}

		if (Mesh)
		{
			Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
			Mesh->SetSimulatePhysics(true);

			const FVector VictimLocation = Victim->GetActorLocation();
			const FVector LaunchDir = (VictimLocation - ViewStart).GetSafeNormal();
			const FVector Impulse = LaunchDir * LaunchSpeed + FVector::UpVector * UpwardSpeed;
			Mesh->AddImpulse(Impulse, NAME_None, true);

			// Clothing (Gideon's Paragon-era APEX coat) treats the ragdoll
			// detach as a violent teleport and stretches into room-sized
			// sheets. Reset the cloth sim after the impulse; verified live
			// that it un-taffies the coat and stays sane on the ragdoll.
			Mesh->ForceClothNextUpdateTeleportAndReset();

			// Despawn the ragdolled victim after a delay.
			Victim->SetLifeSpan(RagdollLifetime);
		}

		if (SlapSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, SlapSound, Hit.ImpactPoint);
		}

		// FUN-2: the connected slap pays out (null-safe when headless).
		if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
		{
			Progression->GrantSauce(SauceOnSlap);
		}

		break;
	}
}
