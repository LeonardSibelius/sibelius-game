#include "CathedralDoor.h"
#include "BranchSubsystem.h"
#include "GenerateComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ACathedralDoor::ACathedralDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorMesh->SetCanEverAffectNavigation(false);
}

void ACathedralDoor::BeginPlay()
{
	Super::BeginPlay();

	// Headless no-op (house rule): a commandlet world has no player to poll for and
	// must never see authored visibility flipped under the gate.
	if (IsRunningCommandlet())
	{
		return;
	}

	if (bRequireGenerateUse)
	{
		ApplyRevealed(false);
		GetWorldTimerManager().SetTimer(GatePollHandle, this, &ACathedralDoor::PollGenerateGate,
			0.5f, /*bLoop=*/true);
	}
}

void ACathedralDoor::Interact_Implementation(AActor* Interactor)
{
	// Gated door: no collision means the focus trace can't reach us, but guard
	// against a direct Execute_Interact regardless.
	if (!bRevealed)
	{
		return;
	}

	if (!IsTravelAllowed(GetBranch()))
	{
		// D1. Refusal must be legible to the player, not just the log (the
		// RequestDeploy lesson).
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red,
				TEXT("Merge or discard your branches before ascending"));
		}
		UE_LOG(LogTemp, Display, TEXT("[CathedralDoor] travel refused: branched"));
		return;
	}

	if (TargetLevelName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CathedralDoor] TargetLevelName not set — staying put"));
		return;
	}

	UGameplayStatics::OpenLevel(this, TargetLevelName);
}

FText ACathedralDoor::GetInteractionPrompt_Implementation() const
{
	return bRevealed
		? NSLOCTEXT("Sibelius", "CathedralDoorPrompt", "Enter the cathedral [E]")
		: FText::GetEmpty();
}

bool ACathedralDoor::IsTravelAllowed(const UBranchSubsystem* Branch)
{
	// Same reasoning as CanDeploy (depth 0 only): traveling while branched would
	// strand the snapshot stack — the branches must be merged or discarded first.
	return !Branch || Branch->GetDepth() == 0;
}

UBranchSubsystem* ACathedralDoor::GetBranch() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UBranchSubsystem>() : nullptr;
}

void ACathedralDoor::ApplyRevealed(bool bNowRevealed)
{
	bRevealed = bNowRevealed;
	if (DoorMesh)
	{
		DoorMesh->SetHiddenInGame(!bNowRevealed);
		DoorMesh->SetCollisionEnabled(bNowRevealed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

void ACathedralDoor::PollGenerateGate()
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const UGenerateComponent* Generate = Pawn ? Pawn->FindComponentByClass<UGenerateComponent>() : nullptr;
	if (Generate && Generate->HasSpawnedThisSession())
	{
		ApplyRevealed(true);
		GetWorldTimerManager().ClearTimer(GatePollHandle);
	}
}
