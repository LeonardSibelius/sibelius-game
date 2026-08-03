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
#include "DancerAgentComponent.h"   // F on a dancer reshuffles her dance
#include "InteractorComponent.h"    // ...using the same focus E uses
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

namespace
{
	/**
	 * The dancer agent for whatever a trace hit, or null.
	 *
	 * A MetaHuman is not one tidy actor: the capsule, the body mesh and the grooms can
	 * belong to attached or child actors, so a sweep may report a hit on something whose
	 * OWNER is the dancer rather than the dancer herself. UDancerAgentSubsystem attaches
	 * the component to the actor that carries the dancing mesh, so we walk up the owner
	 * and attachment chain before giving up.
	 *
	 * Depth-limited because Owner/AttachParent chains can, in principle, cycle.
	 */
	UDancerAgentComponent* FindDancerAgent(AActor* Actor)
	{
		for (int32 Hops = 0; Actor && Hops < 4; ++Hops)
		{
			if (UDancerAgentComponent* Agent = Actor->FindComponentByClass<UDancerAgentComponent>())
			{
				return Agent;
			}

			AActor* Next = Actor->GetOwner();
			if (!Next)
			{
				Next = Actor->GetAttachParentActor();
			}
			Actor = Next;
		}
		return nullptr;
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

	// Walt (2026-08-03): F on a dancing girl does not fight her — it reshuffles her
	// dance. That is the joke her greeting sets up ("Wanna Fight? I don't really
	// fight, I just dance"). Reuse the interactor's focus so E and F agree about who
	// the player is looking at; a separate sweep here could disagree with the prompt
	// on screen, which reads as a bug.
	if (const UInteractorComponent* Interactor = OwnerPawn->FindComponentByClass<UInteractorComponent>())
	{
		if (UDancerAgentComponent* Agent = FindDancerAgent(Interactor->GetFocusedActor()))
		{
			if (Agent->ShuffleDance())
			{
				return;   // handled — no fight, no sauce, no stat bump
			}
		}
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

	// A dancer found by the FIGHT'S OWN sweep also counts.
	//
	// The intercept at the top of this function uses the interactor's focus, which is a
	// 30 cm sphere on ECC_Visibility — the same trace that puts the prompt on screen.
	// This sweep is 60 cm on ECC_Pawn. So there is a band where the fight reaches a
	// dancer but the interactor never focused her: F then fell through and ran the
	// fight, which is what Walt saw as "red flashes like with Gideon" on Nyra while
	// Isla worked. Rule now: anywhere the fight can reach her, the shuffle reaches her.
	for (const FHitResult& Hit : Hits)
	{
		if (UDancerAgentComponent* Agent = FindDancerAgent(Hit.GetActor()))
		{
			if (Agent->ShuffleDance())
			{
				return;
			}
		}
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
					// Walt's clip catch, take two (take one — a ragdoll handoff —
					// resurrected the Paragon stretch this whole path exists to
					// avoid). Death_Back lays the body toward the actor's BACK,
					// so pick the clearest fall lane BEFORE falling: trace 8
					// directions at knee height and spin him so his back faces
					// the most open floor. Ties prefer falling away from the
					// slapper (reads as impact).
					{
						const FVector Knee = VictimLocation + FVector(0.f, 0.f, 40.f);
						const float LaneLength = 260.f;
						FCollisionQueryParams LaneParams(SCENE_QUERY_STAT(SlapFallLane), false, Victim);
						LaneParams.AddIgnoredActor(OwnerPawn);
						float BestScore = -1.f;
						float BestYaw = ViewRot.Yaw;   // fallback: away from the player
						for (int32 LaneIdx = 0; LaneIdx < 8; ++LaneIdx)
						{
							const float Yaw = LaneIdx * 45.f;
							const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
							FHitResult LaneHit;
							const bool bBlocked = World->LineTraceSingleByChannel(
								LaneHit, Knee, Knee + Dir * LaneLength, ECC_Visibility, LaneParams);
							const float Clearance = bBlocked ? LaneHit.Distance : LaneLength;
							const float Score = Clearance + 25.f * FVector::DotProduct(Dir, LaunchDir);
							if (Score > BestScore) { BestScore = Score; BestYaw = Yaw; }
						}
						// Back faces the clear lane => forward faces the opposite way.
						Victim->SetActorRotation(FRotator(0.f, BestYaw + 180.f, 0.f));
					}

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
