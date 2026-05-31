#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RefuserController.generated.h"

UCLASS()
class SIBELIUSGAME_API ARefuserController : public AAIController
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Refuser")
	float AcceptanceRadius = 100.f;

	UPROPERTY(EditAnywhere, Category="Refuser")
	float ChaseInterval = 0.5f;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	void ChasePlayer();

private:
	FTimerHandle ChaseTimerHandle;
};
