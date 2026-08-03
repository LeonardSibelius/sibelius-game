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

		const EPathFollowingRequestResult::Type Result = MoveToActor(PlayerPawn, AcceptanceRadius);
		UE_LOG(LogSibeliusGame, Display, TEXT("[RefuserChase] MoveToActor=%d PawnAt=%s PlayerAt=%s"),
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
