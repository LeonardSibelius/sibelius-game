// PokerMachine.cpp — SIDE_GAMES G5. See header.

#include "PokerMachine.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/InputComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"
#include "PokerScreenWidget.h"
#include "ProgressionSubsystem.h"
#include "CodeVisionComponent.h"   // the cabinet inherits the library door's tutorial
#include "InteractorComponent.h"   // ...and stands down where an aim tracer already exists
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPokerMachine, Log, All);

APokerMachine::APokerMachine()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CabinetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinetMesh"));
	CabinetMesh->SetupAttachment(SceneRoot);
	CabinetMesh->SetCollisionProfileName(TEXT("BlockAll"));   // SC9: focus trace must land

	/* THE DECK'S PROPORTIONS ARE A COMPONENT DEFAULT, NOT A RUNTIME WRITE.

	   18 cm thick, 114 cm wide, 160 cm tall — a poker card's 2.5:3.5, standing on the
	   floor with its centre at half height.

	   They used to be set in ApplyDeckVisual, which runs at BeginPlay, so the code
	   overwrote this component's transform every single time the game started. That was
	   invisible while the cabinet sat in a level nobody adjusted it in. The moment Walt
	   dragged it into place in the living room it became the whole problem: he moved it,
	   saved, pressed Play, and watched it jump back up behind the curtain.

	   A default belongs in the constructor. An instance's transform belongs to whoever
	   placed it, and nothing at runtime may take it back. */
	CabinetMesh->SetRelativeScale3D(FVector(0.18f, 1.14f, 1.60f));
	CabinetMesh->SetRelativeLocation(FVector(0.f, 0.f, 80.f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	// 50 cm in FRONT of the deck at mid-height, so the felt green lands on the card face
	// rather than inside it. ApplyDeckVisual used to move it here at BeginPlay; it is a
	// default like the deck's own proportions, and defaults belong in the constructor.
	Glow->SetRelativeLocation(FVector(50.f, 0.f, 80.f));
	Glow->SetLightColor(FLinearColor(0.25f, 0.85f, 0.45f));   // felt green
	Glow->SetIntensity(2500.f);
	Glow->SetAttenuationRadius(500.f);
	Glow->CastShadows = false;
}

void APokerMachine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// See the header. CloseScreen is idempotent-safe behind the bScreenOpen latch, and it
	// is the only path that restores GameOnly and hides the cursor.
	if (bScreenOpen)
	{
		UE_LOG(LogPokerMachine, Display,
			TEXT("[PokerMachine] torn down with the screen open (%d) - restoring input."),
			int32(EndPlayReason));
		CloseScreen();
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

void APokerMachine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// The deck, so the editor has something to show. NOT the reveal state — see the header.
	ApplyDeckVisual();
}

void APokerMachine::BeginPlay()
{
	Super::BeginPlay();
	ApplyDeckVisual();
	TryEnableInput();

	// See bHiddenUntilCodeVision: in the living room this cabinet inherits the tutorial
	// the library door used to carry. Everywhere else it is simply visible, as before.
	ApplyRevealState();
	if (bHiddenUntilCodeVision && !bRevealed)
	{
		BindToCodeVision();
	}
}

/* ===================================================================================
   HOLD V AND IT IS THERE — the Chapter 1 lesson, kept when the library was cut.
   =================================================================================== */

void APokerMachine::ApplyRevealState()
{
	const bool bVisible = (!bHiddenUntilCodeVision || bRevealed);

	// Hide the BODY, not the actor. SetActorHiddenInGame would take the glow with it, and
	// a felt-green light with nothing under it is a stranger sight than an empty corner.
	if (CabinetMesh)
	{
		CabinetMesh->SetVisibility(bVisible, /*bPropagateToChildren=*/true);
		// Collision follows visibility so the interactor's focus trace cannot land on an
		// invisible cabinet — E must not work on something he has not found yet.
		CabinetMesh->SetCollisionEnabled(bVisible
			? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (Glow)
	{
		Glow->SetVisibility(bVisible);
	}
}

void APokerMachine::Reveal()
{
	if (bRevealed)
	{
		return;
	}
	bRevealed = true;
	ApplyRevealState();
	UE_LOG(LogTemp, Display, TEXT("[Poker] Revealed by Code Vision."));
}

void APokerMachine::HandleCodeVisionChanged(bool bIsActive)
{
	if (bIsActive)
	{
		Reveal();
	}
	// Releasing V does NOT hide it again. See bHiddenUntilCodeVision.
}

void APokerMachine::BindToCodeVision()
{
	if (bRevealed)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UCodeVisionComponent* CV = Pawn ? Pawn->FindComponentByClass<UCodeVisionComponent>() : nullptr;
	if (CV)
	{
		CV->OnCodeVisionChanged.AddDynamic(this, &APokerMachine::HandleCodeVisionChanged);
		if (CV->IsCodeVisionActive())
		{
			Reveal();   // he is already holding V as the level comes up
		}
		return;
	}

	/* RETRY, THEN GIVE UP BY REVEALING. AHiddenDoor's lesson: the pawn is not always there
	   when something wants to bind to it. Unlike that door, failing here would hide the
	   game's FIRST objective behind a power that can never fire, so the fallback shows the
	   cabinet rather than concealing it. A missed flourish beats an unplayable tutorial. */
	if (++VisionBindAttempts < 20)
	{
		World->GetTimerManager().SetTimer(VisionBindHandle, this,
			&APokerMachine::BindToCodeVision, 0.5f, /*bLoop=*/false);
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Poker] No Code Vision component after %d attempts - revealing the cabinet so "
		     "the opening tutorial is never stuck."), VisionBindAttempts);
	Reveal();
}

void APokerMachine::ApplyDeckVisual()
{
	if (!CabinetMesh) { return; }

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Ace = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cards/M_CardAce.M_CardAce"));
	if (Cube)
	{
		CabinetMesh->SetStaticMesh(Cube);
	}
	/* NO TRANSFORM WRITES HERE. The deck's proportions moved to the constructor, where a
	   default belongs — see the comment there. This function assigns the LOOK (mesh and
	   material) and nothing else, so a cabinet someone placed by hand stays where they
	   put it when the game starts.

	   Walt found this the only way it can be found: he dragged it into the living room,
	   saved, pressed Play, and it jumped back up behind the curtain. */
	if (Ace)
	{
		CabinetMesh->SetMaterial(0, Ace);
	}
}

void APokerMachine::TryEnableInput()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		if (++InputAttempts < 40)
		{
			GetWorldTimerManager().SetTimer(InputRetryHandle, this, &APokerMachine::TryEnableInput, 0.25f, false);
		}
		return;
	}

	/* NOT IN A LEVEL THAT HAS AN INTERACTION TRACER (Walt, 2026-09-07: "poker keeps
	   popping up").

	   This raw bind exists for the LIBRARY, whose plain DefaultPawn has no tracer, so
	   IInteractable::Interact never fired there. The header always said the office should
	   use the IInteractable path instead — but nothing enforced it, so in the office BOTH
	   paths were live, and this one asks only "is he within 450 cm", never "is he looking
	   at me".

	   That was harmless while the cabinet stood alone in a library. The moment it moved
	   into the living room it became: press E for the legacy machine, a book, the cauldron,
	   the cathedral door — and get poker as well, because all of them are inside 4.5 m of
	   the deck.

	   The tracer is the right arbiter: it asks what he is AIMING at. So where one exists,
	   this bind stands down entirely and Interact_Implementation does the work. */
	if (const APawn* Pawn = PC->GetPawn())
	{
		if (Pawn->FindComponentByClass<UInteractorComponent>())
		{
			UE_LOG(LogPokerMachine, Display,
				TEXT("[PokerMachine] tracer level - raw E bind stood down; E is by aim."));
			return;
		}
	}

	EnableInput(PC);
	if (InputComponent)
	{
		// bConsumeInput=false — Walt's dead-E catch: a consumed bind eats the
		// key for EVERY machine on the floor even when this one ignores it
		// (too far). Distance gates arbitrate; nobody consumes.
		FInputKeyBinding& KB = InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APokerMachine::OnInteractKey);
		KB.bConsumeInput = false;
	}
}

bool APokerMachine::IsPlayerNear(float Radius) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn && FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation()) <= Radius;
}

void APokerMachine::OnInteractKey()
{
	if (bScreenOpen || !IsPlayerNear(SitDownRadius)) { return; }
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		OpenScreen(PC);
	}
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
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->ClaimOneTimeGrant(TEXT("Tutorial.Poker"));
	}
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
