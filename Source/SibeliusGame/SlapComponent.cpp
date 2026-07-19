#include "SlapComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimSequence.h"   // APPEAL-6: the slap death animation
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "ProgressionSubsystem.h"   // FUN-2: slaps pay Sauce
#include "RefuserController.h"      // bOnlySlapRefusers target filter
#include "RefactorComponent.h"      // APPEAL-6c: wild creatures are slappable
#include "Components/StaticMeshComponent.h"

USlapComponent::USlapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// APPEAL-6: Gideon's own Paragon death. FObjectFinder = a real CDO
	// reference, so the cooker ships the asset (the v0.7.4 soft-ref miss).
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathAnimFinder(
		TEXT("/Game/ParagonGideon/Characters/Heroes/Gideon/Animations/Death_Back.Death_Back"));
	if (DeathAnimFinder.Succeeded())
	{
		SlapDeathAnim = DeathAnimFinder.Object;
	}
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
		// APPEAL-6c: wild-refactored creatures are fair game — pure comedy, no
		// sauce and no stat (a zebra is not a Refuser, and paying would make an
		// infinite R-then-slap sauce farm). Skeletals ragdoll on their pack's
		// own physics assets; statues launch as rigid bodies. The creature
		// keeps its UWildRefactorState wherever it lands, so R still restores
		// the original at its original spot.
		if (AActor* HitActor = Hit.GetActor())
		{
			if (HitActor->FindComponentByClass<UWildRefactorState>())
			{
				const FVector CreatureImpulse =
					(HitActor->GetActorLocation() - ViewStart).GetSafeNormal() * LaunchSpeed
					+ FVector::UpVector * UpwardSpeed;
				if (USkeletalMeshComponent* SkelComp = HitActor->FindComponentByClass<USkeletalMeshComponent>())
				{
					SkelComp->SetCollisionProfileName(TEXT("Ragdoll"));
					SkelComp->SetSimulatePhysics(true);
					SkelComp->AddImpulse(CreatureImpulse, NAME_None, true);
					// The Ragdoll profile dropped the R-trace guarantee (Walt's
					// downed fox could never give the couch back). Re-block it.
					SkelComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
					SkelComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				}
				else if (UStaticMeshComponent* MeshComp = HitActor->FindComponentByClass<UStaticMeshComponent>())
				{
					MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
					MeshComp->SetSimulatePhysics(true);
					MeshComp->AddImpulse(CreatureImpulse, NAME_None, true);
					MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
					MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				}
				if (SlapSound)
				{
					UGameplayStatics::PlaySoundAtLocation(World, SlapSound, Hit.ImpactPoint);
				}
				break;
			}
		}

		ACharacter* Victim = Cast<ACharacter>(Hit.GetActor());
		if (!Victim || Victim == OwnerPawn)
		{
			continue;
		}

		if (bOnlySlapRefusers && Cast<ARefuserController>(Victim->GetController()) == nullptr)
		{
			continue;
		}

		if (AController* VictimController = Victim->GetController())
		{
			VictimController->StopMovement();
			VictimController->UnPossess();
		}

		// Kill character movement BEFORE any physics takes over. With the
		// capsule's collision changed the movement component finds no floor and
		// free-falls the capsule; bones without physics bodies are animated
		// relative to that plummeting component transform, so the skin
		// stretches between the simulated bodies and the falling capsule.
		if (UCharacterMovementComponent* Move = Victim->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
			Move->SetComponentTickEnabled(false);
		}

		const FVector VictimLocation = Victim->GetActorLocation();
		const FVector LaunchDir = (VictimLocation - ViewStart).GetSafeNormal();
		const FVector Impulse = LaunchDir * LaunchSpeed + FVector::UpVector * UpwardSpeed;

		if (bRigidKnockback)
		{
			// APPEAL-6 (Walt's call): a slapped Gideon COLLAPSES where he stands —
			// the death animation plays in place (single-node, non-looping, holds
			// the final pose) and there is no launch at all. Only when the anim
			// can't play (missing asset / wrong skeleton) do we fall back to the
			// original stretch-proof freeze-and-launch, which works on any mesh.
			bool bCollapsing = false;
			if (USkeletalMeshComponent* PoseMesh = Victim->GetMesh())
			{
				UAnimSequence* DeathAnim = SlapDeathAnim;
				if (DeathAnim && PoseMesh->GetSkeletalMeshAsset()
					&& PoseMesh->GetSkeletalMeshAsset()->GetSkeleton() == DeathAnim->GetSkeleton())
				{
					PoseMesh->PlayAnimation(DeathAnim, false);
					bCollapsing = true;
				}
				else
				{
					PoseMesh->bPauseAnims = true;
				}
				PoseMesh->SuspendClothingSimulation();
			}
			if (UCapsuleComponent* Capsule = Victim->GetCapsuleComponent())
			{
				if (bCollapsing)
				{
					// The corpse shouldn't body-block the player while it fades.
					Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
				else
				{
					Capsule->SetCollisionProfileName(TEXT("PhysicsActor"));
					Capsule->SetSimulatePhysics(true);
					Capsule->AddImpulse(Impulse, NAME_None, true);
				}
			}
			if (bCollapsing)
			{
				// Walt's clip catch: with the capsule dead, the animated pose
				// ignored the room (legs in walls, head in the couch). Let the
				// anim sell the stagger, then hand the body to physics with NO
				// impulse — the Ragdoll profile collides with the world (and
				// ignores the player), so the crumple settles on whatever is
				// actually there.
				if (USkeletalMeshComponent* PoseMesh = Victim->GetMesh();
					PoseMesh && PoseMesh->GetPhysicsAsset() != nullptr)
				{
					TWeakObjectPtr<USkeletalMeshComponent> WeakMesh = PoseMesh;
					FTimerHandle HandoffTimer;
					World->GetTimerManager().SetTimer(HandoffTimer,
						FTimerDelegate::CreateWeakLambda(Victim, [WeakMesh]()
						{
							if (USkeletalMeshComponent* HandoffMesh = WeakMesh.Get())
							{
								HandoffMesh->SetCollisionProfileName(TEXT("Ragdoll"));
								HandoffMesh->SetSimulatePhysics(true);   // zero impulse: a crumple, not a launch
							}
						}),
						FMath::Max(0.01f, CollapseRagdollDelay), false);
				}
			}
			Victim->SetLifeSpan(RagdollLifetime);

			if (SlapSound)
			{
				UGameplayStatics::PlaySoundAtLocation(World, SlapSound, Hit.ImpactPoint);
			}
			if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
			{
				Progression->GrantSauce(SauceOnSlap);
				Progression->BumpStat(SibeliusStats::RefusersSlapped);
			}
			break;
		}

		if (UCapsuleComponent* Capsule = Victim->GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
			// Freeze the cloth in its last live pose before the bodies detach
			// (the converted APEX coat explodes if left simulating).
			Mesh->SuspendClothingSimulation();
			Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
			Mesh->SetSimulatePhysics(true);
			Mesh->AddImpulse(Impulse, NAME_None, true);

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
			Progression->BumpStat(SibeliusStats::RefusersSlapped);
		}

		break;
	}
}
