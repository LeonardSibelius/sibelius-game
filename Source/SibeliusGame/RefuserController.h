// RefuserController.h
//
// The Refuser's AI: walk to the player, and once there, menace them.
//
// The chase half is deliberately dumb — a repeating MoveToActor, no behaviour
// tree. The attack half exists because the chase alone was not frightening:
// a Refuser reached you, stopped one metre away, and stood there. That also
// hollowed out the Test-Drive/branch power, whose whole payoff is FREEZING
// Refusers (UBranchPIEComponent::FreezeRefusers) — you cannot be rescued from
// something that was never doing anything. An attacking Refuser freezes
// mid-swing, which is the shot that power never had.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RefuserController.generated.h"

class UAnimMontage;
class USoundBase;

UCLASS()
class SIBELIUSGAME_API ARefuserController : public AAIController
{
	GENERATED_BODY()

public:
	ARefuserController();

	UPROPERTY(EditAnywhere, Category="Refuser")
	float AcceptanceRadius = 100.f;

	UPROPERTY(EditAnywhere, Category="Refuser")
	float ChaseInterval = 0.5f;

	// --- Menace -------------------------------------------------------------
	// Assign on the Refuser's AIControllerClass defaults, or on a BP subclass.
	// Everything here is optional: with no montage assigned the Refuser behaves
	// exactly as it did before, so this can never break an existing level.

	// Played when the Refuser is in range. Defaults in the constructor to Gideon's
	// OWN Primary_Attack_A_Medium_Montage — the Refusers in L_Office_v02 and
	// L_AI_Temple are Paragon Gideon, not the MetaHuman MH_Refuser, so his native
	// montage needs no retarget and is already a montage asset. Hard-referenced by
	// FObjectFinder rather than a soft path, so the cooker actually ships it
	// (SlapComponent does the same for Death_Back after the v0.7.4 soft-ref miss).
	// Override per-instance for a differently-skinned Refuser.
	UPROPERTY(EditAnywhere, Category="Refuser|Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, Category="Refuser|Attack")
	TObjectPtr<USoundBase> AttackSound;

	// Slightly wider than AcceptanceRadius so the Refuser is already in range the
	// moment the move ends, rather than shuffling on the boundary.
	UPROPERTY(EditAnywhere, Category="Refuser|Attack", meta=(ClampMin="0"))
	float AttackRange = 180.f;

	// Seconds between swings. Long enough to read as deliberate, short enough to
	// feel like being cornered.
	UPROPERTY(EditAnywhere, Category="Refuser|Attack", meta=(ClampMin="0"))
	float AttackCooldown = 2.5f;

	// Turn to face the player while chasing. Cheap, and a Refuser that tracks you
	// is markedly more unpleasant than one that walks past your shoulder.
	UPROPERTY(EditAnywhere, Category="Refuser|Attack")
	bool bFacePlayer = true;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	void ChasePlayer();

	// Returns true if a swing was started this call.
	bool TryAttack(const APawn& PlayerPawn);

private:
	FTimerHandle ChaseTimerHandle;
	float LastAttackTime = -BIG_NUMBER;   // so the first swing is never on cooldown
};
