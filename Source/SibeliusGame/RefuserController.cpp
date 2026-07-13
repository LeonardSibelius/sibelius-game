#include "RefuserController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AITypes.h"        // EPathFollowingRequestResult
#include "Navigation/PathFollowingComponent.h" // EPathFollowingStatus / GetMoveStatus
#include "NavigationInvokerComponent.h" // forest roads: navmesh bubbles around agents
#include "SibeliusGame.h"   // LogSibeliusGame

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
		if (GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			return; // move in progress — MoveToActor already tracks the goal actor
		}

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
