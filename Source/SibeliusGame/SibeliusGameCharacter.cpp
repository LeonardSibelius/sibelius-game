// Copyright Epic Games, Inc. All Rights Reserved.

#include "SibeliusGameCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InteractorComponent.h"
#include "CodeVisionComponent.h"
#include "RefactorComponent.h"
#include "InventoryComponent.h"
#include "BuildComponent.h"
#include "BranchPIEComponent.h"
#include "GenerateComponent.h"    // Ch6 Generate driver
#include "SibeliusHUD.h"          // SIB-39 dev-overlay toggle
#include "JournalWidget.h"        // SIB-41 journal panel
#include "GameMenuWidget.h"       // FUN-8 Tab game menu
#include "GenerateRequestWidget.h" // SIB-30 P1 typed-request panel
#include "Blueprint/UserWidget.h" // CreateWidget
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"       // EKeys / EInputEvent (debug branch keys)
#include "Kismet/KismetSystemLibrary.h" // SIB-42: Q-to-quit
#include "Kismet/GameplayStatics.h"     // O-to-office: OpenLevel
#include "Engine/World.h"               // GetMapName for the wander-world check
#include "TravelTransitionSubsystem.h"  // O-to-office travels through the transition cover
#include "ProgressionSubsystem.h"       // FUN-1: the power gates + sauce cheats
#include "MrsHallSubsystem.h"           // SPINE Move 2 moment 1: the opening ticket
#include "TimerManager.h"
#include "SauceShop.h"                  // FUN-3: re-apply bought upgrades on spawn
#include "SauceBowl.h"                  // temple ritual: C claims a filled pot in reach
#include "EngineUtils.h"                // TActorIterator (the pot scan)
#include "NavigationInvokerComponent.h" // forest roads: navmesh bubbles around agents
#include "SibeliusGame.h"

ASibeliusGameCharacter::ASibeliusGameCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// Navigation invoker: the huge World Partition forests generate navmesh
	// only in bubbles around invokers (see DefaultEngine.ini) — the player
	// carries one so AI goals at the player's feet are always on the mesh.
	UNavigationInvokerComponent* NavInvoker =
		CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
	NavInvoker->SetGenerationRadii(4000.f, 6000.f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// Interactor: camera line-trace + E-to-Interact (input bound below)
	InteractorComponent = CreateDefaultSubobject<UInteractorComponent>(TEXT("InteractorComponent"));

	// Code Vision: single source of truth for Ch1 reveal state (SIB-25, input bound below)
	CodeVisionComp = CreateDefaultSubobject<UCodeVisionComponent>(TEXT("CodeVisionComp"));

	// Refactor: Ch2 selection + trigger; lives on the pawn so there's no find-the-player race (SIB-26)
	RefactorComp = CreateDefaultSubobject<URefactorComponent>(TEXT("RefactorComp"));

	// Compile (Ch3): inventory is the single resource authority; build driver scans for proximate sites.
	// Both pawn-owned so neither has to find the player (SIB-27, generalizing the Ch1/R7 lesson).
	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComp"));
	BuildComp = CreateDefaultSubobject<UBuildComponent>(TEXT("BuildComp"));

	// Ch4 Test-Drive (SIB-36): PIE consumer of UBranchSubsystem (debug keys + signals)
	BranchPIEComp = CreateDefaultSubobject<UBranchPIEComponent>(TEXT("BranchPIEComp"));

	// Ch6 Generate (SIB-30): typed-request budget + catalog + resolve-and-spawn.
	GenerateComp = CreateDefaultSubobject<UGenerateComponent>(TEXT("GenerateComp"));

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void ASibeliusGameCharacter::BeginPlay()
{
	Super::BeginPlay();

	// FUN-3: purchased upgrades land on the fresh components (which just spawned
	// with authored defaults, so this is exact — never a double apply).
	FSauceShop::ApplyPersistentPurchases(this);

	ScheduleOpeningTicket();

	/* SPINE moment 2+: she notices the first time you USE each power.
	   One subscription rather than six call sites. OnPowerVerbUsed is the gated-input
	   chokepoint (see FinaleAltar.h) — it fires only when a press actually did something,
	   so she never reacts to a power the gate refused. Reacting to something that did not
	   happen would be worse than saying nothing. */
	OnPowerVerbUsed.AddUObject(this, &ASibeliusGameCharacter::HandlePowerUsedForHall);
}

void ASibeliusGameCharacter::HandlePowerUsedForHall(EPowerVerb Verb)
{
	// Stable token per verb. Local switch rather than a file-scope helper: generic names
	// at namespace scope collide across a unity blob (the ProceduralPcm.h lesson).
	const TCHAR* Token = nullptr;
	switch (Verb)
	{
	case EPowerVerb::CodeVision: Token = TEXT("CodeVision"); break;
	case EPowerVerb::Refactor:   Token = TEXT("Refactor");   break;
	case EPowerVerb::Compile:    Token = TEXT("Compile");    break;
	case EPowerVerb::TestDrive:  Token = TEXT("TestDrive");  break;
	case EPowerVerb::Deploy:     Token = TEXT("Deploy");     break;
	case EPowerVerb::Generate:   Token = TEXT("Generate");   break;
	default: return;
	}

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return;
	}

	/* Once per power, per save, on the existing saved grant machinery. Its OWN key, not
	   the tutorial's: Tutorial.Vision is already claimed on any save where the player has
	   used Vision, so reusing it would mean she stayed silent forever on every existing
	   save. */
	if (!Progression->ClaimOneTimeGrant(FName(*FString::Printf(TEXT("Hall.FirstUse.%s"), Token))))
	{
		return;
	}

	// Silent (with a log line) for any verb whose line is not written yet — see
	// Content/Data/MrsHallStory.csv and the RequiredReasons list in GenerateSmokeTest.
	if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
	{
		Hall->Say(FName(*FString::Printf(TEXT("Power.%s"), Token)));
	}
}

void ASibeliusGameCharacter::ScheduleOpeningTicket()
{
	/* MOMENT 1 (docs/SPINE.md Move 2): the job.
	   The player wakes in the living room already holding Code Vision and owning nothing
	   else. Before this, nobody wanted anything from him — which is why the game read as
	   a tour. Mrs. Hall hands him a ticket against the legacy system and forbids the one
	   thing that would help, and every room after has a reason to be entered. */

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return;   // headless / no save — nothing to schedule
	}

	/* Only for a player who has not earned anything yet. A fresh state holds Code Vision
	   and nothing more, so NumUnlocked() > 1 means this is a save in progress: claim the
	   grant silently so a returning player is never handed "here is your first assignment"
	   halfway through the game. */
	if (Progression->GetStateForRead().NumUnlocked() > 1)
	{
		Progression->ClaimOneTimeGrant(TEXT("Hall.Ticket"));
		return;
	}

	if (!Progression->ClaimOneTimeGrant(TEXT("Hall.Ticket")))
	{
		return;   // already delivered on this save
	}

	// A beat before she speaks. The player needs a moment to find their feet and read the
	// objective line before the first thing that happens to them is their boss.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(OpeningTicketTimer, this,
			&ASibeliusGameCharacter::DeliverOpeningTicket, OpeningTicketDelay, /*bLoop=*/false);
	}
}

void ASibeliusGameCharacter::DeliverOpeningTicket()
{
	if (UMrsHallSubsystem* Hall = UMrsHallSubsystem::Get(this))
	{
		Hall->Say(TEXT("Ticket"));
	}
}

void ASibeliusGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASibeliusGameCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASibeliusGameCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASibeliusGameCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASibeliusGameCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASibeliusGameCharacter::LookInput);

		// Interacting (E) - mirrors how the other actions are wired
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ASibeliusGameCharacter::DoInteract);
		}

		// Code Vision (hold) - Started reveals, Completed restores. FUN-1: routes
		// through the character's power gate (Completed stays ungated — releasing
		// the key must always restore, whatever the unlock state).
		if (CodeVisionAction && CodeVisionComp)
		{
			EnhancedInputComponent->BindAction(CodeVisionAction, ETriggerEvent::Started, this, &ASibeliusGameCharacter::OnCodeVisionStarted);
			EnhancedInputComponent->BindAction(CodeVisionAction, ETriggerEvent::Completed, this, &ASibeliusGameCharacter::OnCodeVisionCompleted);
		}

		// Refactor (R) - Started toggles whatever refactorable is under the crosshair (FUN-1: gated)
		if (RefactorAction && RefactorComp)
		{
			EnhancedInputComponent->BindAction(RefactorAction, ETriggerEvent::Started, this, &ASibeliusGameCharacter::OnRefactorPressed);
		}

		// Build (B) - Started builds the proximate affordable site (FUN-1: gated on the Compile verb)
		if (BuildAction && BuildComp)
		{
			EnhancedInputComponent->BindAction(BuildAction, ETriggerEvent::Started, this, &ASibeliusGameCharacter::OnBuildPressed);
		}
	}
	else
	{
		UE_LOG(LogSibeliusGame, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	// Debug branch keys (raw BindKey works regardless of Enhanced Input). Number row,
	// not the F-row (F8 collided with the editor's Eject/Possess) and not gizmo keys
	// 1-5: 6 Enter · 7 Merge · 8 Discard (Ch4 branch ops) · 9 Clear deploy save (dev) ·
	// 0 Deploy (Ch5).
	// FUN-1: 6/7/8 gate on Test-Drive and 0 on Deploy; 9 (clear-deploy) stays an
	// ungated dev key.
	if (BranchPIEComp)
	{
		PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ASibeliusGameCharacter::OnBranchEnterPressed);
		PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ASibeliusGameCharacter::OnBranchMergePressed);
		PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ASibeliusGameCharacter::OnBranchDiscardPressed);
		PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, BranchPIEComp.Get(), &UBranchPIEComponent::Debug_ClearDeploy);
		PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ASibeliusGameCharacter::OnDeployPressed);
	}

	// SIB-39 dev-overlay toggle: H (hide/help). V was taken by Code Vision; H is free
	// and off the F-row, gizmo keys 1-5, the 6-9/0 branch block, and F/E/V/R/B/WASD.
	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &ASibeliusGameCharacter::ToggleDevOverlay);

	// SIB-41 journal/story panel toggle: J.
	PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ASibeliusGameCharacter::ToggleJournal);

	// FUN-8 game menu (STATUS/CONTROLS): M, plus Tab as a courtesy. M is primary
	// because Slate claims Tab for focus navigation and PIE eats it before raw
	// BindKey ever sees it (Walt: "i press tab and nothing happens") — Tab may
	// still work in a packaged build, so both stay bound.
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ASibeliusGameCharacter::ToggleGameMenu);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ASibeliusGameCharacter::ToggleGameMenu);

	// SIB-30 P1 generate/ask panel toggle: G.
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &ASibeliusGameCharacter::ToggleGenerate);

	// SIB-42: Q to quit, double-press confirm. Packaged builds had NO exit
	// (Walt alt-tabbed to Task Manager like a hostage).
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ASibeliusGameCharacter::RequestQuit);

	// Wander-world return: O -> back to the office. Raw BindKey (no Input Action / IMC),
	// same as H/J/G/Q above. ReturnToOffice no-ops unless we're standing in a wander world,
	// so the binding is harmless in the office / other levels.
	PlayerInputComponent->BindKey(EKeys::O, IE_Pressed, this, &ASibeliusGameCharacter::ReturnToOffice);

	// New Game: N, double-press confirm (Q-quit pattern) — the player-facing
	// ResetProgression. (N is free here; the Carousel level's N lives on a
	// different pawn.)
	PlayerInputComponent->BindKey(EKeys::N, IE_Pressed, this, &ASibeliusGameCharacter::RequestNewGame);
}


void ASibeliusGameCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ASibeliusGameCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ASibeliusGameCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ASibeliusGameCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ASibeliusGameCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void ASibeliusGameCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void ASibeliusGameCharacter::DoInteract()
{
	if (InteractorComponent)
	{
		InteractorComponent->TryInteract();
	}
}

void ASibeliusGameCharacter::ToggleDevOverlay()
{
	ASibeliusHUD::bOverlayVisible = !ASibeliusHUD::bOverlayVisible;
}

void ASibeliusGameCharacter::RequestQuit()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastQuitPressTime <= 2.0f)
	{
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		return;
	}
	LastQuitPressTime = Now;

	// Unlike the New Game and power-refusal messages below, this one had NO HUD path at
	// all — so in a shipped build Q did nothing visible the first time, and quit the game
	// the second. The confirmation the double-press relies on was never on screen.
	ASibeliusHUD::Toast(this, TEXT("Press Q again to quit"), 2.0f, SibeliusToast::Prize);
}

void ASibeliusGameCharacter::RequestNewGame()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastNewGamePressTime <= 3.0f)
	{
		LastNewGamePressTime = -100.0f;
		if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
		{
			Progression->ResetProgression();   // powers, sauce, claims, purchases + save file
		}
		if (BranchPIEComp)
		{
			BranchPIEComp->Debug_ClearDeploy();   // deployed world edits too — authored-clean
		}
		UTravelTransitionSubsystem::Travel(this, TEXT("L_Office_v02"));   // home, fresh
		return;
	}
	LastNewGamePressTime = Now;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->ShowBanner(TEXT("NEW GAME — press N again to ERASE ALL PROGRESS"), 3.0f);
			return;
		}
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0xACE0F3, 3.0f, FColor::Orange, TEXT("NEW GAME - press N again to ERASE ALL PROGRESS"));
	}
}

bool ASibeliusGameCharacter::IsAwayFromOfficeLevelName(const FString& RawLevelName) const
{
	// UWorld::GetMapName() KEEPS the PIE streaming prefix in PIE ("UEDPIE_0_L_AI_Temple");
	// strip it so PIE and a packaged build compare equal. Away == anything but the office.
	return FName(*UWorld::RemovePIEPrefix(RawLevelName)) != OfficeLevelName;
}

bool ASibeliusGameCharacter::IsAwayFromOffice() const
{
	const UWorld* World = GetWorld();
	return World && IsAwayFromOfficeLevelName(World->GetMapName());
}

void ASibeliusGameCharacter::ReturnToOffice()
{
	// O acts in EVERY away-from-office level (forest, temple, cathedral, future worlds); in
	// the office it's a no-op, so a single press is safe to leave bound everywhere.
	if (IsAwayFromOffice())
	{
		UTravelTransitionSubsystem::Travel(this, OfficeLevelName);
	}
}

void ASibeliusGameCharacter::ToggleJournal()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Open -> close.
	if (JournalWidget && JournalWidget->IsInViewport())
	{
		JournalWidget->RemoveFromParent();
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		return;
	}

	// Create once, then show. Reload the narrative each open so edits show live in PIE.
	if (!JournalWidget)
	{
		JournalWidget = CreateWidget<UJournalWidget>(PC, UJournalWidget::StaticClass());
	}
	if (JournalWidget)
	{
		JournalWidget->AddToViewport(50);      // builds the Slate tree (NativeConstruct loads once)
		JournalWidget->RefreshFromNarrative(); // reload each open — BodyText is valid now

		// Show the cursor + UI input so the scroll box is usable; J still closes it.
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(JournalWidget->TakeWidget());
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
}

void ASibeliusGameCharacter::ToggleGameMenu()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[GameMenu] toggle (open=%s)"),
		(GameMenuWidget && GameMenuWidget->IsInViewport()) ? TEXT("true") : TEXT("false"));

	// Open -> close (mirrors ToggleJournal).
	if (GameMenuWidget && GameMenuWidget->IsInViewport())
	{
		GameMenuWidget->CloseMenu();
		return;
	}

	if (!GameMenuWidget)
	{
		GameMenuWidget = CreateWidget<UGameMenuWidget>(PC, UGameMenuWidget::StaticClass());
	}
	if (GameMenuWidget)
	{
		GameMenuWidget->AddToViewport(55);

		// UIOnly + focus so Tab/Esc land in the widget's NativeOnKeyDown and the
		// tab buttons are clickable (the shop's pattern).
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(GameMenuWidget->TakeWidget());
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		GameMenuWidget->SetFocus();
	}
}

void ASibeliusGameCharacter::ToggleGenerate()
{
	// FUN-1: the Generate verb must be earned before the panel opens. (Closing an
	// already-open panel is below this gate and unreachable while locked.)
	if (!CheckPowerUnlocked(EPowerVerb::Generate))
	{
		return;
	}
	OnPowerVerbUsed.Broadcast(EPowerVerb::Generate);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Open -> close.
	if (GenerateWidget && GenerateWidget->IsInViewport()
		&& GenerateWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		CloseGenerate();
		return;
	}

	// Create once; reuse (toggle visibility, never destroy mid-callback).
	if (!GenerateWidget)
	{
		GenerateWidget = CreateWidget<UGenerateRequestWidget>(PC, UGenerateRequestWidget::StaticClass());
		if (GenerateWidget)
		{
			GenerateWidget->OnSubmit.BindUObject(this, &ASibeliusGameCharacter::HandleGenerateSubmit);
			GenerateWidget->OnCancel.BindUObject(this, &ASibeliusGameCharacter::CloseGenerate);
		}
	}
	if (GenerateWidget)
	{
		if (!GenerateWidget->IsInViewport())
		{
			GenerateWidget->AddToViewport(60);
		}
		GenerateWidget->SetVisibility(ESlateVisibility::Visible);

		// UIOnly so keystrokes enter the text box and WASD no longer moves the pawn.
		// Focus the text box's live Slate widget specifically. (In UIOnly the game won't
		// get G/Esc — the panel closes itself: Esc in its OnKeyDown, or Enter-submit.)
		FInputModeUIOnly Mode;
		if (TSharedPtr<SWidget> InputSlate = GenerateWidget->GetInputSlate())
		{
			Mode.SetWidgetToFocus(InputSlate);
		}
		else
		{
			Mode.SetWidgetToFocus(GenerateWidget->TakeWidget());
		}
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);

		GenerateWidget->FocusInput(); // also SetKeyboardFocus on the text box
	}
}

void ASibeliusGameCharacter::HandleGenerateSubmit(const FString& Text)
{
	if (GenerateComp)
	{
		GenerateComp->SubmitRequest(Text);
	}
	CloseGenerate();
}

void ASibeliusGameCharacter::CloseGenerate()
{
	if (GenerateWidget)
	{
		GenerateWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

// --- FUN-1: the power gate + gated input handlers -------------------------

bool ASibeliusGameCharacter::CheckPowerUnlocked(EPowerVerb Verb) const
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression || Progression->IsUnlocked(Verb))
	{
		// No subsystem (headless/teardown) fails open: gating is a game-feel
		// feature, never something that should brick a session or a smoke test.
		return true;
	}
	// Walt's first playtest lesson: a small top-left line drowns under the dev
	// overlay and the refusal reads as a bug. Use the HUD's centered ceremony
	// banner so the player can't miss WHY nothing happened.
	const FString Refusal = FString::Printf(TEXT("%s IS NOT YET YOURS — SEEK THE PLACE IT IS GRANTED"),
		*PowerVerbDisplayName(Verb));
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ShowBanner(Refusal, 3.5f);
	}
	else if (GEngine)
	{
		// Stable key per verb so mashing the key updates one line instead of stacking.
		GEngine->AddOnScreenDebugMessage(0xF0B0 + static_cast<uint64>(Verb), 3.0f, FColor::Orange, Refusal);
	}
	return false;
}

void ASibeliusGameCharacter::OnCodeVisionStarted()
{
	if (CodeVisionComp && CheckPowerUnlocked(EPowerVerb::CodeVision))
	{
		CodeVisionComp->ActivateCodeVision();
		OnPowerVerbUsed.Broadcast(EPowerVerb::CodeVision);
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UProgressionSubsystem* Progression = GI->GetSubsystem<UProgressionSubsystem>())
				{
					Progression->ClaimOneTimeGrant(TEXT("Tutorial.Vision"));
				}
			}
		}
	}
}

void ASibeliusGameCharacter::OnCodeVisionCompleted()
{
	// Deliberately ungated: key-up must always restore normal vision.
	if (CodeVisionComp)
	{
		CodeVisionComp->DeactivateCodeVision();
	}
}

void ASibeliusGameCharacter::OnRefactorPressed()
{
	if (RefactorComp && CheckPowerUnlocked(EPowerVerb::Refactor))
	{
		RefactorComp->TriggerRefactor();
		OnPowerVerbUsed.Broadcast(EPowerVerb::Refactor);
	}
}

void ASibeliusGameCharacter::OnBuildPressed()
{
	if (!CheckPowerUnlocked(EPowerVerb::Compile))
	{
		return;
	}

	// One verb, disambiguated by state (Raymond's rule): a filled sauce pot
	// within reach compiles FIRST — the temple ritual; otherwise the press
	// goes to the build sites as always. (The pot's own raw BindKey lost the
	// argument with Enhanced Input and never fired.)
	for (TActorIterator<ASauceBowl> It(GetWorld()); It; ++It)
	{
		if (It->TryClaim(this))
		{
			OnPowerVerbUsed.Broadcast(EPowerVerb::Compile);
			return;
		}
	}

	if (BuildComp)
	{
		BuildComp->TriggerBuild();
		OnPowerVerbUsed.Broadcast(EPowerVerb::Compile);
	}
}

void ASibeliusGameCharacter::OnBranchEnterPressed()
{
	if (BranchPIEComp && CheckPowerUnlocked(EPowerVerb::TestDrive))
	{
		BranchPIEComp->Debug_Enter();
		OnPowerVerbUsed.Broadcast(EPowerVerb::TestDrive);
	}
}

void ASibeliusGameCharacter::OnBranchMergePressed()
{
	if (BranchPIEComp && CheckPowerUnlocked(EPowerVerb::TestDrive))
	{
		BranchPIEComp->Debug_Merge();
		OnPowerVerbUsed.Broadcast(EPowerVerb::TestDrive);
	}
}

void ASibeliusGameCharacter::OnBranchDiscardPressed()
{
	if (BranchPIEComp && CheckPowerUnlocked(EPowerVerb::TestDrive))
	{
		BranchPIEComp->Debug_Discard();
		OnPowerVerbUsed.Broadcast(EPowerVerb::TestDrive);
	}
}

void ASibeliusGameCharacter::OnDeployPressed()
{
	if (BranchPIEComp && CheckPowerUnlocked(EPowerVerb::Deploy))
	{
		BranchPIEComp->Debug_Deploy();
		OnPowerVerbUsed.Broadcast(EPowerVerb::Deploy);
	}
}

// --- FUN-1: dev cheats (console) -------------------------------------------

void ASibeliusGameCharacter::GrantPower(const FString& PowerName)
{
	EPowerVerb Verb;
	if (!ParsePowerVerb(PowerName, Verb))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red,
				FString::Printf(TEXT("GrantPower: unknown power '%s' (try: vision, refactor, compile, testdrive, deploy, generate)"), *PowerName));
		}
		return;
	}
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->UnlockPower(Verb);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
				FString::Printf(TEXT("GrantPower: %s unlocked"), *PowerVerbDisplayName(Verb)));
		}
	}
}

void ASibeliusGameCharacter::GrantSauce(int32 Amount)
{
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->GrantSauce(Amount);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Emerald,
				FString::Printf(TEXT("GrantSauce: +%d (total %d)"), Amount, Progression->GetSauce()));
		}
	}
}

void ASibeliusGameCharacter::UnlockAllPowers()
{
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->UnlockAllPowers();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan, TEXT("UnlockAllPowers: all six verbs unlocked"));
		}
	}
}

void ASibeliusGameCharacter::ResetProgression()
{
	if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
	{
		Progression->ResetProgression();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Orange,
				TEXT("ResetProgression: back to Code Vision only, 50 sauce"));
		}
	}
}
