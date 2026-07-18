// PokerMachine.cpp — SIDE_GAMES G5. See header.

#include "PokerMachine.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/DateTime.h"
#include "PokerScreenWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPokerMachine, Log, All);

APokerMachine::APokerMachine()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CabinetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinetMesh"));
	CabinetMesh->SetupAttachment(SceneRoot);
	CabinetMesh->SetCollisionProfileName(TEXT("BlockAll"));   // SC9: focus trace must land

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	Glow->SetLightColor(FLinearColor(0.25f, 0.85f, 0.45f));   // felt green
	Glow->SetIntensity(2500.f);
	Glow->SetAttenuationRadius(500.f);
	Glow->CastShadows = false;
}

void APokerMachine::Interact_Implementation(AActor* Interactor)
{
	if (bScreenOpen) { return; }   // SC2: re-entrant E is a no-op

	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC)
	{
		UE_LOG(LogPokerMachine, Warning, TEXT("[PokerMachine] Interact without a player controller — ignored."));
		return;
	}
	OpenScreen(PC);
}

FText APokerMachine::GetInteractionPrompt_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("Video poker [E] — %d sauce a hand"), BetPerHand));
}

void APokerMachine::OpenScreen(APlayerController* PC)
{
	ScreenPC = PC;

	if (!Screen)
	{
		Screen = CreateWidget<UPokerScreenWidget>(PC, UPokerScreenWidget::StaticClass());
		if (!Screen)
		{
			UE_LOG(LogPokerMachine, Error, TEXT("[PokerMachine] failed to create UPokerScreenWidget."));
			return;
		}
		Screen->OnClosed.BindUObject(this, &APokerMachine::CloseScreen);
	}
	if (!bSeeded)
	{
		// SC10: one seed per session; determinism stays the smoke test's business.
		Screen->InitModel(static_cast<int32>(FDateTime::Now().GetTicks() & 0x7FFFFFFF));
		bSeeded = true;
	}
	Screen->SetStake(BetPerHand);
	Screen->AddToViewport(60);

	// SC1: UIOnly + focus so keys go to the cards, never to WASD.
	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetWidgetToFocus(Screen->TakeWidget());
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
	Screen->SetKeyboardFocus();

	bScreenOpen = true;
	UE_LOG(LogPokerMachine, Display, TEXT("[PokerMachine] screen opened (stake %d)."), BetPerHand);
}

void APokerMachine::CloseScreen()
{
	// SC1: the ONE close path — restore the game's input no matter how we got here.
	if (Screen) { Screen->RemoveFromParent(); }
	if (APlayerController* PC = ScreenPC.Get())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	bScreenOpen = false;
	UE_LOG(LogPokerMachine, Display, TEXT("[PokerMachine] screen closed; input restored."));
}
