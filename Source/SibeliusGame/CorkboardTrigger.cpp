// CorkboardTrigger.cpp

#include "CorkboardTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HallAlarmSubsystem.h"
#include "SibeliusGame.h"

ACorkboardTrigger::ACorkboardTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	RootComponent = InteractionVolume;

	InteractionVolume->SetBoxExtent(FVector(60.f, 60.f, 60.f));

	// Invisible at runtime (a box only renders in editor), but still blocks the
	// camera's Visibility line trace so the interactor can focus it.
	InteractionVolume->SetHiddenInGame(true);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ACorkboardTrigger::Interact_Implementation(AActor* Interactor)
{
	if (bTriggered)
	{
		return;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UHallAlarmSubsystem* Alarm = GameInstance ? GameInstance->GetSubsystem<UHallAlarmSubsystem>() : nullptr;
	if (!Alarm)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Corkboard] No HallAlarmSubsystem available; cannot fire alarm."));
		return;
	}

	bTriggered = true;
	UE_LOG(LogSibeliusGame, Display, TEXT("[Corkboard] Interacted - firing Hall alarm."));
	Alarm->TriggerAlarm();
}

FText ACorkboardTrigger::GetInteractionPrompt_Implementation() const
{
	// Once fired, stop advertising the prompt.
	return bTriggered ? FText::GetEmpty() : Prompt;
}
