#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShinbiController.generated.h"

class ACharacter;

// Companion brain: follow the player at a respectful distance; when a Refuser
// comes within EngageRange, run to it and slap (via the pawn's USlapComponent).
// Timer-driven like ARefuserController — no behavior tree, same house style.
UCLASS()
class SIBELIUSGAME_API AShinbiController : public AAIController
{
	GENERATED_BODY()

public:
	// How often she re-thinks (seconds).
	UPROPERTY(EditAnywhere, Category="Companion")
	float ThinkInterval = 0.4f;

	// Stop following when this close to the player.
	UPROPERTY(EditAnywhere, Category="Companion")
	float FollowAcceptanceRadius = 250.f;

	// A Refuser inside this range becomes her target.
	UPROPERTY(EditAnywhere, Category="Companion")
	float EngageRange = 1200.f;

	// Close enough to swing (the slap itself sweeps SlapRange from her view).
	UPROPERTY(EditAnywhere, Category="Companion")
	float SlapReach = 200.f;

	UPROPERTY(EditAnywhere, Category="Companion")
	float SlapCooldown = 1.2f;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Think();
	ACharacter* FindNearestRefuser(const FVector& From, float MaxRange) const;

	FTimerHandle ThinkTimerHandle;
	double LastSlapTime = -1000.0;

	// Last movement goal, so we only re-issue MoveToActor when it changes.
	TWeakObjectPtr<AActor> CurrentGoal;
};
