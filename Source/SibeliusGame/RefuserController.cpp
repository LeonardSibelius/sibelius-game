#include "RefuserController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AITypes.h"        // EPathFollowingRequestResult
#include "SibeliusGame.h"   // LogSibeliusGame

void ARefuserController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

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
		const EPathFollowingRequestResult::Type Result = MoveToActor(PlayerPawn, AcceptanceRadius);
		UE_LOG(LogSibeliusGame, Display, TEXT("[RefuserChase] MoveToActor=%d PawnAt=%s PlayerAt=%s"),
			(int32)Result, *GetPawn()->GetActorLocation().ToString(), *PlayerPawn->GetActorLocation().ToString());
	}
}

void ARefuserController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ChaseTimerHandle);

	Super::EndPlay(EndPlayReason);
}
