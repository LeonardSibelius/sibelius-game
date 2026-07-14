#include "ShinbiCompanion.h"

#include "ShinbiController.h"
#include "SlapComponent.h"
#include "ChaosCloth/ChaosClothingSimulationInteractor.h"
#include "ClothingAssetBase.h"
#include "ClothingSimulationInteractor.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

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

void AShinbiCompanion::BeginPlay()
{
	Super::BeginPlay();

	// Heartbeat, not one-shot: the engine recreates the cloth simulation on
	// LOD changes / streaming, which silently resets damping to the asset's
	// factory values (= hummingbird ribbons). Re-assert every second.
	GetWorldTimerManager().SetTimer(
		ClothTuneRetryHandle, this, &AShinbiCompanion::ApplyClothTuning,
		1.0f, /*bLoop=*/true, /*FirstDelay=*/0.f);
}

void AShinbiCompanion::ApplyClothTuning()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (bSuspendClothEntirely)
	{
		MeshComp->SuspendClothingSimulation();
		GetWorldTimerManager().ClearTimer(ClothTuneRetryHandle);
		return;
	}

	if (!bTameClothFlutter)
	{
		GetWorldTimerManager().ClearTimer(ClothTuneRetryHandle);
		return;
	}

	UClothingSimulationInteractor* Sim = MeshComp->GetClothingSimulationInteractor();
	USkeletalMesh* MeshAsset = MeshComp->GetSkeletalMeshAsset();
	if (Sim && MeshAsset)
	{
		for (UClothingAssetBase* ClothAsset : MeshAsset->GetMeshClothingAssets())
		{
			if (!ClothAsset)
			{
				continue;
			}
			if (UChaosClothingInteractor* Cloth =
				Cast<UChaosClothingInteractor>(Sim->GetClothingInteractor(ClothAsset->GetFName())))
			{
				Cloth->SetDamping(ClothDamping, ClothLocalDamping);
			}
		}
	}
}
