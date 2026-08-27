#include "RefuserController.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AITypes.h"        // EPathFollowingRequestResult
#include "Navigation/PathFollowingComponent.h" // EPathFollowingStatus / GetMoveStatus
#include "NavigationInvokerComponent.h" // forest roads: navmesh bubbles around agents
#include "SibeliusGame.h"   // LogSibeliusGame

ARefuserController::ARefuserController()
{
	// The Refusers actually placed in L_Office_v02 and L_AI_Temple are Paragon
	// GIDEON, not the MetaHuman MH_Refuser the docs describe — the levels
	// reference Gideon and SlapComponent plays his Paragon death. So his own
	// Primary_Attack montage is the correct asset: right skeleton, already a
	// montage, no retarget.
	//
	// FObjectFinder, not a soft path: a soft-path-only asset passes in PIE and is
	// MISSING from the shipped pak. This is a real CDO reference, so the cooker
	// ships it — the same fix SlapComponent carries for Death_Back.
	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackFinder(
		TEXT("/Game/ParagonGideon/Characters/Heroes/Gideon/Animations/Primary_Attack_A_Medium_Montage.Primary_Attack_A_Medium_Montage"));
	if (AttackFinder.Succeeded())
	{
		AttackMontage = AttackFinder.Object;
	}
}

void ARefuserController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Inject a navigation invoker so the pawn generates navmesh around itself
	// (invoker-only generation; see DefaultEngine.ini). Done here on possess so
	// EVERY Refuser pawn gets one without editing any Blueprint asset.
	if (InPawn && !InPawn->FindComponentByClass<UNavigationInvokerComponent>())
	{
		UNavigationInvokerComponent* Invoker =
			NewObject<UNavigationInvokerComponent>(InPawn, TEXT("NavInvoker"));
		Invoker->SetGenerationRadii(4000.f, 6000.f);
		Invoker->RegisterComponent();
	}

	// Track the player so the Refuser faces you while closing and while swinging.
	// Without this it walks past your shoulder and attacks the wall behind you.
	if (bFacePlayer)
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			SetFocus(PlayerPawn);
		}
	}

	GetWorldTimerManager().SetTimer(
		ChaseTimerHandle,
		this,
		&ARefuserController::ChasePlayer,
		ChaseInterval,
		/*bLoop=*/true,
		/*FirstDelay=*/0.f);
}

/* STAND THERE. An army massed on a ridge is not a workaround for a performance
   problem - it is what an army on a ridge does. They wait, and they come when the fight
   starts. But it is also the fix for a genuine scaling wall, so both reasons are written
   down rather than only the flattering one.

   EVERY REFUSER CARRIES A NAVIGATION INVOKER with 40 m / 60 m generation radii, injected
   in OnPossess so no Blueprint has to be edited, and this project generates navmesh
   INVOKER-ONLY. One Refuser in an office is free. A hundred and fifty spread across a
   ninety-degree arc of hillside asks the navigation system to build navmesh across most
   of a square kilometre of steep terrain, at runtime, in one frame. That is what took
   the meadow to 3.7 fps - not the demons, which the bench had already measured at 66.

   So a held Refuser gives its invoker back. It is scenery until something wants it to
   fight, and scenery does not need to know where the floor is. */
void ARefuserController::HoldPosition()
{
	GetWorldTimerManager().ClearTimer(ChaseTimerHandle);
	StopMovement();

	if (APawn* P = GetPawn())
	{
		if (UNavigationInvokerComponent* Invoker = P->FindComponentByClass<UNavigationInvokerComponent>())
		{
			Invoker->UnregisterComponent();
			Invoker->DestroyComponent();
		}
	}
}

void ARefuserController::ChasePlayer()
{
	if (GetPawn() == nullptr)
	{
		// We got unpossessed (e.g. the Refuser was slapped); stop chasing.
		GetWorldTimerManager().ClearTimer(ChaseTimerHandle);
		return;
	}

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		// Close enough to menace? Swing before considering another move, so an
		// arrived Refuser attacks instead of standing there re-pathing.
		if (TryAttack(*PlayerPawn))
		{
			return;
		}

		if (GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			return; // move in progress — MoveToActor already tracks the goal actor
		}

		/* THE SKATING, AND IT IS NOT A LOCOMOTION BUG.
		   Gideon_AnimBlueprint's Event Blueprint Begin Play plays Paragon's
		   LevelStart_Montage - a hero-arrival animation for a MOBA respawn pad. It
		   carries a FullBody curve, which drives the AnimBP's FullBody bool and blends
		   the WHOLE body off the locomotion pose. So a freshly spawned Refuser slides
		   toward you frozen in an arrival stance with its legs still.

		   Speed was never the problem: the AnimBP sets it from Get Velocity on the
		   Actor, with no cast to fail. Checked in the graph rather than assumed, after
		   assuming twice today and being wrong twice.

		   Stopped here rather than in OnPossess because the montage has not STARTED by
		   then - the AnimBP's BeginPlay runs after possession. This is the first moment
		   a Refuser is about to move, which is exactly when a standing-around animation
		   has outstayed its welcome. */
		if (USkeletalMeshComponent* MeshComp = GetPawn() ? GetPawn()->FindComponentByClass<USkeletalMeshComponent>() : nullptr)
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				// Never the attack montage: TryAttack returns early while in range, so
				// reaching here means we are closing, not swinging. Belt and braces.
				if (AnimInst->IsAnyMontagePlaying()
					&& AnimInst->GetCurrentActiveMontage() != AttackMontage)
				{
					AnimInst->Montage_Stop(0.15f);
				}
			}
		}

		/* VERBOSE, NOT DISPLAY - this was a real shipping bug and the meadow only made
		   it visible. A Refuser that CANNOT reach the player never enters a move, so it
		   never takes the early-out above: it re-requests a path and writes this line
		   every ChaseInterval, forever, in a packaged build, to the player's disk. In the
		   office there are one or two Refusers and nobody noticed. Standing 150 of them
		   on a hillside with no navmesh produced 43,800 lines in 145 seconds and dropped
		   the game to 3.7 fps.

		   Display survives a Shipping build. Verbose is compiled out of it, and stays
		   quiet in the editor until somebody asks for it with
		   `log LogSibeliusGame Verbose`. */
		const EPathFollowingRequestResult::Type Result = MoveToActor(PlayerPawn, AcceptanceRadius);
		UE_LOG(LogSibeliusGame, Verbose, TEXT("[RefuserChase] MoveToActor=%d PawnAt=%s PlayerAt=%s"),
			(int32)Result, *GetPawn()->GetActorLocation().ToString(), *PlayerPawn->GetActorLocation().ToString());
	}
}

bool ARefuserController::TryAttack(const APawn& PlayerPawn)
{
	ACharacter* Refuser = Cast<ACharacter>(GetPawn());
	UWorld* World = GetWorld();
	if (!Refuser || !World || !AttackMontage)
	{
		return false;   // no montage assigned == the old standing-there behaviour
	}

	// A frozen Refuser must not swing. The branch power freezes by zeroing the
	// pawn's CustomTimeDilation, which stops its animation and movement but NOT
	// this controller's timer — without this check a frozen Refuser would keep
	// silently restarting a montage that can never play, and would lunge the
	// instant you merged back out.
	if (FMath::IsNearlyZero(Refuser->CustomTimeDilation))
	{
		return false;
	}

	const float Distance = FVector::Dist(Refuser->GetActorLocation(), PlayerPawn.GetActorLocation());
	if (Distance > AttackRange)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return true;   // in range but reloading — hold position, do not re-path
	}
	LastAttackTime = Now;

	USkeletalMeshComponent* Mesh = Refuser->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[RefuserAttack] no AnimInstance on the Refuser mesh — montage skipped"));
		return false;
	}

	Anim->Montage_Play(AttackMontage);

	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AttackSound, Refuser->GetActorLocation());
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[RefuserAttack] swing at %.0fcm (range %.0f)"), Distance, AttackRange);
	return true;
}

void ARefuserController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ChaseTimerHandle);

	Super::EndPlay(EndPlayReason);
}
