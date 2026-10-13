// NotHumanTerminal.cpp — see the header.

#include "NotHumanTerminal.h"

#include "FreakbookWidget.h"
#include "NotHumanWidget.h"
#include "ProgressionSubsystem.h"
#include "SibeliusGame.h"   // LogSibeliusGame

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace NotHumanTerminalDetail
{
	// The open laptop, for the dev command below.
	TWeakObjectPtr<ANotHumanTerminal> OpenLaptop;

	FAutoConsoleCommand FreakbookCommand(
		TEXT("sib.Freakbook.Open"),
		TEXT("With the laptop open: Freakbook at page V (0 login, 1 feed, 2 profile, 3 thread, 4 dashboard, 5 bank) for person P (0 Dee .. 4 Jaxx)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (ANotHumanTerminal* Laptop = OpenLaptop.Get())
			{
				Laptop->DebugFreakbook(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0);
			}
		}));
}

void ANotHumanTerminal::DebugFreakbook(int32 View, int32 Person)
{
	if (!bOpen)
	{
		return;
	}
	OpenFreakbook();
	if (Book)
	{
		Book->DebugShow(View, Person);
	}
}
#endif

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
		Screen->OnFreakbook.BindUObject(this, &ANotHumanTerminal::OpenFreakbook);
	}
	if (!Screen->IsInViewport())
	{
		Screen->AddToViewport(70);
	}
	Screen->SetVisibility(ESlateVisibility::Visible);
	if (Book)
	{
		Book->SetVisibility(ESlateVisibility::Collapsed);
	}

	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	Screen->Open(Progression && Progression->IsUnlocked(EPowerVerb::CodeVision));

	UsingPC = PC;
	bOpen = true;
	Focus(Screen);
#if !UE_BUILD_SHIPPING
	NotHumanTerminalDetail::OpenLaptop = this;
#endif
	UE_LOG(LogSibeliusGame, Display, TEXT("[NotHuman] the laptop is open."));
}

void ANotHumanTerminal::OpenFreakbook()
{
	APlayerController* PC = UsingPC.Get();
	if (!bOpen || !PC)
	{
		return;
	}
	if (!Book)
	{
		Book = CreateWidget<UFreakbookWidget>(PC, UFreakbookWidget::StaticClass());
		if (!Book)
		{
			return;
		}
		Book->OnClosed.BindUObject(this, &ANotHumanTerminal::Close);
	}
	if (!Book->IsInViewport())
	{
		Book->AddToViewport(70);
	}
	if (Screen)
	{
		Screen->SetVisibility(ESlateVisibility::Collapsed);   // collapsed, never destroyed mid-callback
	}
	Book->SetVisibility(ESlateVisibility::Visible);
	Book->Open();
	Focus(Book);
	UE_LOG(LogSibeliusGame, Display, TEXT("[NotHuman] Freakbook is open."));
}

void ANotHumanTerminal::Focus(UUserWidget* Widget)
{
	APlayerController* PC = UsingPC.Get();
	if (!PC || !Widget)
	{
		return;
	}
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
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
	if (Book)
	{
		Book->SetVisibility(ESlateVisibility::Collapsed);
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
	if (Book)
	{
		Book->RemoveFromParent();
	}
	Super::EndPlay(EndPlayReason);
}
