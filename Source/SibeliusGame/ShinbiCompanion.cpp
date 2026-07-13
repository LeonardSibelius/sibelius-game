#include "ShinbiCompanion.h"

#include "ShinbiController.h"
#include "SlapComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AShinbiCompanion::AShinbiCompanion()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = AShinbiController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Face where she's running (looks natural with Paragon locomotion); the
	// slap aim comes from the controller's focus, not the body yaw.
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 540.f, 0.f);
		Move->MaxWalkSpeed = 600.f;
	}

	SlapComponent = CreateDefaultSubobject<USlapComponent>(TEXT("SlapComponent"));
}
