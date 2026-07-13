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

	ApplyClothTuning();
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
		return;
	}

	if (!bTameClothFlutter)
	{
		return;
	}

	// The cloth simulation (and its per-cloth interactors) is created lazily —
	// sometimes well after BeginPlay. Keep retrying until the damping actually
	// lands on at least one cloth (a single retry provably loses the race:
	// two of three placed Shinbis got tuned, the third kept flapping).
	int32 Applied = 0;
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
				++Applied;
			}
		}
	}

	if (Applied == 0 && ++ClothTuneAttempts < 20)
	{
		GetWorldTimerManager().SetTimer(
			ClothTuneRetryHandle, this, &AShinbiCompanion::ApplyClothTuning,
			0.5f, /*bLoop=*/false);
	}
}
