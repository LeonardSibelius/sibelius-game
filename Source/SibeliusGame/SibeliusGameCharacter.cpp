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
#include "SibeliusHUD.h"          // SIB-39 dev-overlay toggle
#include "JournalWidget.h"        // SIB-41 journal panel
#include "Blueprint/UserWidget.h" // CreateWidget
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"       // EKeys / EInputEvent (debug branch keys)
#include "SibeliusGame.h"

ASibeliusGameCharacter::ASibeliusGameCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
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

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
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

		// Code Vision (hold) - Started reveals, Completed restores; drives the component directly
		if (CodeVisionAction && CodeVisionComp)
		{
			EnhancedInputComponent->BindAction(CodeVisionAction, ETriggerEvent::Started, CodeVisionComp.Get(), &UCodeVisionComponent::ActivateCodeVision);
			EnhancedInputComponent->BindAction(CodeVisionAction, ETriggerEvent::Completed, CodeVisionComp.Get(), &UCodeVisionComponent::DeactivateCodeVision);
		}

		// Refactor (R) - Started toggles whatever refactorable is under the crosshair
		if (RefactorAction && RefactorComp)
		{
			EnhancedInputComponent->BindAction(RefactorAction, ETriggerEvent::Started, RefactorComp.Get(), &URefactorComponent::TriggerRefactor);
		}

		// Build (B) - Started builds the proximate affordable site; .Get() on the TObjectPtr (Ch1 lesson)
		if (BuildAction && BuildComp)
		{
			EnhancedInputComponent->BindAction(BuildAction, ETriggerEvent::Started, BuildComp.Get(), &UBuildComponent::TriggerBuild);
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
	if (BranchPIEComp)
	{
		PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, BranchPIEComp.Get(), &UBranchPIEComponent::Debug_Enter);
		PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, BranchPIEComp.Get(), &UBranchPIEComponent::Debug_Merge);
		PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, BranchPIEComp.Get(), &UBranchPIEComponent::Debug_Discard);
		PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, BranchPIEComp.Get(), &UBranchPIEComponent::Debug_ClearDeploy);
		PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, BranchPIEComp.Get(), &UBranchPIEComponent::Debug_Deploy);
	}

	// SIB-39 dev-overlay toggle: H (hide/help). V was taken by Code Vision; H is free
	// and off the F-row, gizmo keys 1-5, the 6-9/0 branch block, and F/E/V/R/B/WASD.
	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &ASibeliusGameCharacter::ToggleDevOverlay);

	// SIB-41 journal/story panel toggle: J.
	PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ASibeliusGameCharacter::ToggleJournal);
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
