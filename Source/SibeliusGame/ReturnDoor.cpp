// ReturnDoor.cpp — see header.

#include "ReturnDoor.h"
#include "ElsewhereSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogReturnDoor, Log, All);

AReturnDoor::AReturnDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	SetRootComponent(DoorMesh);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DoorMesh->SetCollisionResponseToAllChannels(ECR_Block);   // takes the interact trace

	// A real kit door frame so the way home reads as a door, not a checkered placeholder
	// cube. Falls back to an engine slab if the kit isn't installed (never a bare cube in
	// the player's face — the builder also seats it in the wall gap, not mid-hall).
	if (UStaticMesh* Door = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/ModularSciFiEnv_K/Meshes/Doors/SM_Door_A_3x4m_Base.SM_Door_A_3x4m_Base")))
	{
		DoorMesh->SetStaticMesh(Door);   // kit mesh authored to grid; scale 1
	}
	else if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		DoorMesh->SetStaticMesh(Cube);
		DoorMesh->SetRelativeScale3D(FVector(0.2f, 1.2f, 2.2f));
	}
}

void AReturnDoor::Interact_Implementation(AActor* /*Interactor*/)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (UElsewhereSubsystem* Elsewhere = GI->GetSubsystem<UElsewhereSubsystem>())
		{
			Elsewhere->DiscardStagedElsewhere();   // the room is throwaway (§4)
		}
	}

	UE_LOG(LogReturnDoor, Display, TEXT("[%s] returning home to %s; Elsewhere discarded."),
		*GetName(), *HomeLevelName.ToString());
	UGameplayStatics::OpenLevel(this, HomeLevelName);
}

FText AReturnDoor::GetInteractionPrompt_Implementation() const
{
	return ReturnPromptText;
}
