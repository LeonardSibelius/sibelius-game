// NotHumanTerminal.cpp — see the header.

#include "NotHumanTerminal.h"

#include "NotHumanWidget.h"
#include "ProgressionSubsystem.h"
#include "SibeliusGame.h"   // LogSibeliusGame

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ANotHumanTerminal::ANotHumanTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	Shell = CreateDefaultSubobject<UBoxComponent>(TEXT("Shell"));
	SetRootComponent(Shell);
	Shell->SetBoxExtent(FVector(25.0, 28.0, 16.0));   // a laptop's size; place_not_human_terminal.py fits it to the mesh
	Shell->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Shell->SetCollisionResponseToAllChannels(ECR_Ignore);
	Shell->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Shell->SetGenerateOverlapEvents(false);
	Shell->SetHiddenInGame(true);
}

FText ANotHumanTerminal::GetInteractionPrompt_Implementation() const
{
	return bOpen ? FText::GetEmpty() : PromptText;
}

void ANotHumanTerminal::Interact_Implementation(AActor* Interactor)
{
	if (bOpen)
	{
		return;
	}
	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC && GetWorld())
	{
		PC = GetWorld()->GetFirstPlayerController();
	}
	if (!PC)
	{
		return;
	}
	if (!Screen)
	{
		Screen = CreateWidget<UNotHumanWidget>(PC, UNotHumanWidget::StaticClass());
		if (!Screen)
		{
			return;
		}
		Screen->OnClosed.BindUObject(this, &ANotHumanTerminal::Close);
	}
	if (!Screen->IsInViewport())
	{
		Screen->AddToViewport(70);
	}
	Screen->SetVisibility(ESlateVisibility::Visible);

	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	Screen->Open(Progression && Progression->IsUnlocked(EPowerVerb::CodeVision));

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Screen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	UsingPC = PC;
	bOpen = true;
	UE_LOG(LogSibeliusGame, Display, TEXT("[NotHuman] the laptop is open."));
}

void ANotHumanTerminal::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	if (Screen)
	{
		Screen->SetVisibility(ESlateVisibility::Collapsed);   // collapsed, never destroyed mid-callback
	}
	if (APlayerController* PC = UsingPC.Get())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	// The other half of SetInputMode: a collapsed widget can still hold the keyboard.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
	UsingPC.Reset();
	UE_LOG(LogSibeliusGame, Display, TEXT("[NotHuman] the laptop is closed."));
}

void ANotHumanTerminal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Close();
	if (Screen)
	{
		Screen->RemoveFromParent();
	}
	Super::EndPlay(EndPlayReason);
}
