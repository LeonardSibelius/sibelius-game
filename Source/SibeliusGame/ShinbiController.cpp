#include "ShinbiController.h"

#include "RefuserController.h"
#include "ShinbiCompanion.h"
#include "SlapComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

void AShinbiController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	GetWorldTimerManager().SetTimer(
		ThinkTimerHandle, this, &AShinbiController::Think,
		ThinkInterval, /*bLoop=*/true, /*FirstDelay=*/0.f);
}

void AShinbiController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ThinkTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void AShinbiController::Think()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
		return;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();

	// 1) A Refuser in range trumps everything: close in and slap.
	if (ACharacter* Enemy = FindNearestRefuser(MyLocation, EngageRange))
	{
		SetFocus(Enemy);

		const float Distance = FVector::Dist2D(MyLocation, Enemy->GetActorLocation());
		if (Distance <= SlapReach)
		{
			StopMovement();
			CurrentGoal = nullptr;

			const double Now = GetWorld()->GetTimeSeconds();
			if (Now - LastSlapTime >= SlapCooldown)
			{
				if (AShinbiCompanion* Shinbi = Cast<AShinbiCompanion>(MyPawn))
				{
					if (Shinbi->SlapComponent)
					{
						Shinbi->SlapComponent->DoSlap();
						LastSlapTime = Now;
					}
				}
			}
		}
		else if (CurrentGoal != Enemy || GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToActor(Enemy, SlapReach * 0.6f);
			CurrentGoal = Enemy;
		}
		return;
	}

	// 2) Otherwise: heel. Follow the player along whatever nav exists (the
	// forest road corridor), stopping at a respectful distance.
	ClearFocus(EAIFocusPriority::Gameplay);
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const float ToPlayer = FVector::Dist2D(MyLocation, PlayerPawn->GetActorLocation());
		if (ToPlayer > FollowAcceptanceRadius
			&& (CurrentGoal != PlayerPawn || GetMoveStatus() == EPathFollowingStatus::Idle))
		{
			MoveToActor(PlayerPawn, FollowAcceptanceRadius * 0.8f);
			CurrentGoal = PlayerPawn;
		}
	}
}

ACharacter* AShinbiController::FindNearestRefuser(const FVector& From, float MaxRange) const
{
	ACharacter* Nearest = nullptr;
	float NearestDistSq = MaxRange * MaxRange;

	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Candidate = *It;
		// A live Refuser is a Character possessed by ARefuserController;
		// slapped ones are unpossessed, so they stop being targets.
		if (!Candidate || !Cast<ARefuserController>(Candidate->GetController()))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(From, Candidate->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Candidate;
		}
	}
	return Nearest;
}
