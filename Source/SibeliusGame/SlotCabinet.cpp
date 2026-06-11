// SlotCabinet.cpp — SIB-34 S3. See header + docs/sib-34-s2-s3-slot-cabinet-notes.md.

#include "SlotCabinet.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/DateTime.h"
#include "SlotScreenWidget.h"
#include "SlotWebScreenWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlotCabinet, Log, All);

ASlotCabinet::ASlotCabinet()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CabinetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinetMesh"));
	CabinetMesh->SetupAttachment(SceneRoot);
	CabinetMesh->SetCollisionProfileName(TEXT("BlockAll"));   // SC9: focus trace must land
}

void ASlotCabinet::Interact_Implementation(AActor* Interactor)
{
	if (bScreenOpen) { return; }   // SC2: re-entrant E is a no-op

	APlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}
	if (!PC)
	{
		UE_LOG(LogSlotCabinet, Warning, TEXT("[SlotCabinet] Interact without a player controller — ignored."));
		return;
	}
	OpenScreen(PC);
}

FText ASlotCabinet::GetInteractionPrompt_Implementation() const
{
	return FText::FromString(TEXT("Play the machine [E]"));
}

void ASlotCabinet::OpenScreen(APlayerController* PC)
{
	ScreenPC = PC;

	// SC1: UIOnly + focus so keys go to the screen, never to WASD.
	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	if (bUseWebScreen)
	{
		// Path A: the real Celestial Fortune in Chromium.
		if (!WebScreen)
		{
			WebScreen = CreateWidget<USlotWebScreenWidget>(PC, USlotWebScreenWidget::StaticClass());
			if (!WebScreen)
			{
				UE_LOG(LogSlotCabinet, Error, TEXT("[SlotCabinet] failed to create USlotWebScreenWidget."));
				return;
			}
			WebScreen->OnClosed.BindUObject(this, &ASlotCabinet::CloseScreen);
		}
		WebScreen->AddToViewport(60);
		WebScreen->LoadGame(WebGameURL);

		// Focus the browser itself so Space reaches the page's spin handler (SW3).
		if (TSharedPtr<SWidget> Focus = WebScreen->GetFocusTarget())
		{
			Mode.SetWidgetToFocus(Focus);
		}
		else
		{
			Mode.SetWidgetToFocus(WebScreen->TakeWidget());
		}
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	else
	{
		// Fallback: the native S2 screen (model-driven, dependency-free).
		if (!Screen)
		{
			Screen = CreateWidget<USlotScreenWidget>(PC, USlotScreenWidget::StaticClass());
			if (!Screen)
			{
				UE_LOG(LogSlotCabinet, Error, TEXT("[SlotCabinet] failed to create USlotScreenWidget."));
				return;
			}
			Screen->OnClosed.BindUObject(this, &ASlotCabinet::CloseScreen);
		}
		if (!bSeeded)
		{
			// SC10: one seed per session; determinism stays the smoke test's business.
			Screen->InitModel(static_cast<int32>(FDateTime::Now().GetTicks() & 0x7FFFFFFF));
			bSeeded = true;
		}
		Screen->AddToViewport(60);
		Mode.SetWidgetToFocus(Screen->TakeWidget());
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
		Screen->SetKeyboardFocus();
	}

	bScreenOpen = true;
	UE_LOG(LogSlotCabinet, Display, TEXT("[SlotCabinet] screen opened (%s)."), bUseWebScreen ? TEXT("web") : TEXT("native"));
}

void ASlotCabinet::CloseScreen()
{
	// SC1: the ONE close path — restore the game's input no matter how we got here.
	if (Screen) { Screen->RemoveFromParent(); }
	if (WebScreen) { WebScreen->RemoveFromParent(); }
	if (APlayerController* PC = ScreenPC.Get())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	bScreenOpen = false;
	UE_LOG(LogSlotCabinet, Display, TEXT("[SlotCabinet] screen closed; input restored."));
}
