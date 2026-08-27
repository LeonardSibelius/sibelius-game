// SlotCabinet.cpp — SIB-34 S3. See header + docs/sib-34-s2-s3-slot-cabinet-notes.md.

#include "SlotCabinet.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "MrsHallSubsystem.h"
#include "ProgressionSubsystem.h"   // lifetime hard meters — NOT SibeliusProgressSubsystem
#include "SibeliusProgressSubsystem.h"
#include "SlotGameModel.h"
#include "SlotScreenWidget.h"
#include "SlotTechPanelWidget.h"
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
		WebScreen->LoadGame(ResolveWebGameURL());

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
			Screen->OnTechPanelRequested.BindUObject(this, &ASlotCabinet::OpenTechPanel);
		}
		if (!bSeeded)
		{
			// SC10: one seed per session; determinism stays the smoke test's business.
			Screen->InitModel(static_cast<int32>(FDateTime::Now().GetTicks() & 0x7FFFFFFF));
			bSeeded = true;

			// The machine is the PLAYER'S machine from the moment they walk up — their
			// saved dials applied to the current factory sheet. Doing this only when the
			// panel opens would mean the first spins of every session ran the shipped
			// par sheet while the panel claimed otherwise.
			if (USlotGameModel* M = Screen->GetModel())
			{
				M->SetParSheet(USlotTechPanelWidget::ComposeSavedParSheet(this));
			}
		}
		Screen->AddToViewport(60);
		Mode.SetWidgetToFocus(Screen->TakeWidget());
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
		Screen->SetKeyboardFocus();
	}

	bScreenOpen = true;

	// SIB-43 / CL2: the moment the machine is played, the clue chain advances —
	// recorded on the GameInstance so it survives the trip home.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USibeliusProgressSubsystem* Progress = GI->GetSubsystem<USibeliusProgressSubsystem>())
		{
			Progress->bSlotPlayed = true;
		}
	}

	UE_LOG(LogSlotCabinet, Display, TEXT("[SlotCabinet] screen opened (%s)."), bUseWebScreen ? TEXT("web") : TEXT("native"));
}

FString ASlotCabinet::ResolveWebGameURL() const
{
	// PK1: the staged copy ships with the game (NonUFS loose file — Chromium
	// needs a real on-disk path, it cannot read out of a .pak). Works in
	// editor AND packaged builds; the EditAnywhere URL is the dev fallback.
	const FString Staged = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("WebGame/index.html"));
	if (FPaths::FileExists(Staged))
	{
		FString Abs = FPaths::ConvertRelativePathToFull(Staged);
		Abs.ReplaceInline(TEXT("\\"), TEXT("/"));
		return FString::Printf(TEXT("file:///%s"), *Abs);
	}
	UE_LOG(LogSlotCabinet, Warning, TEXT("[SlotCabinet] no staged Content/WebGame/index.html — falling back to dev URL %s"), *WebGameURL);
	return WebGameURL;
}

void ASlotCabinet::OpenTechPanel()
{
	APlayerController* PC = ScreenPC.Get();
	if (!PC || !Screen || TechPanel)
	{
		return;
	}

	TechPanel = CreateWidget<USlotTechPanelWidget>(PC, USlotTechPanelWidget::StaticClass());
	if (!TechPanel)
	{
		UE_LOG(LogSlotCabinet, Error, TEXT("[SlotCabinet] failed to create the technician's panel."));
		return;
	}

	TechPanel->Setup(Screen->GetModel());
	TechPanel->OnClosed.AddDynamic(this, &ASlotCabinet::CloseTechPanel);

	// Layered ABOVE the screen (60), so the machine stays visible behind the report —
	// the player should see what they are editing.
	TechPanel->AddToViewport(70);
	TechPanel->SetKeyboardFocus();

	UE_LOG(LogSlotCabinet, Display, TEXT("[SlotCabinet] technician's panel open."));
}

void ASlotCabinet::CloseTechPanel()
{
	if (TechPanel)
	{
		TechPanel->RemoveFromParent();
		TechPanel = nullptr;
	}

	// Focus back to the machine, or the player would be left with a screen that
	// ignores the spacebar.
	if (Screen)
	{
		Screen->SetKeyboardFocus();
	}
}

void ASlotCabinet::CloseScreen()
{
	// SC1: the ONE close path — restore the game's input no matter how we got here.
	CloseTechPanel();   // the panel must never outlive the machine it edits

	/* Bank the session into the lifetime hard meters (docs/FLOOR_REPORT.md step 2).
	   Here PRECISELY BECAUSE this is the one close path: however the player leaves —
	   Esc, the close button, walking away — the play is recorded exactly once.
	   It is a delta, so calling it on every close is safe; an empty one writes nothing. */
	if (Screen)
	{
		if (USlotGameModel* M = Screen->GetModel())
		{
			if (UProgressionSubsystem* Prog = UProgressionSubsystem::Get(this))
			{
				/* THE TOLL IS PAID ON THE WAY OUT, and that is the right beat rather
				   than a convenient one. The meters only bank on close, so mid-session
				   there is nothing to cross; and being told you have earned your way to
				   a war WHILE staring at a reel is the wrong moment for it. You stand
				   up, you turn round, and she is there. */
				const bool bWasShort = !Prog->GetStateForRead().IsBattleQualified();
				Prog->CommitSlotMeters(M->TakePendingMeters());

				if (bWasShort && Prog->GetStateForRead().IsBattleQualified()
					&& Prog->ClaimOneTimeGrant(TEXT("Battle.Qualified")))
				{
					if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
					{
						Hall->Say(TEXT("Battle.Open"));
					}
				}
			}
		}
	}

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
