#include "RefuserController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

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
		MoveToActor(PlayerPawn, AcceptanceRadius);
	}
}

void ARefuserController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ChaseTimerHandle);

	Super::EndPlay(EndPlayReason);
}
