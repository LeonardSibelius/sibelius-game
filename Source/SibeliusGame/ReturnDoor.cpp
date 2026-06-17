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
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
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
