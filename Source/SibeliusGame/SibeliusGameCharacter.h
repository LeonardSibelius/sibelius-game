// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "SibeliusGameCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInteractorComponent;
class UCodeVisionComponent;
class URefactorComponent;
class UInventoryComponent;
class UBuildComponent;
class UBranchPIEComponent;
class UJournalWidget;
class UGenerateComponent;
class UGenerateRequestWidget;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class ASibeliusGameCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Line-traces from the camera and activates IInteractable actors on E */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInteractorComponent> InteractorComponent;

	/** Single source of truth for Ch1 Code Vision on/off state (SIB-25) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCodeVisionComponent> CodeVisionComp;

	/** Ch2 Refactor: camera-traces for refactorables and toggles them (SIB-26) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URefactorComponent> RefactorComp;

	/** Ch3 Compile: single-authority resource inventory, lives on the pawn (SIB-27) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComp;

	/** Ch3 Compile: build/dismantle driver; pawn-owned so there's no find-the-player race (SIB-27) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBuildComponent> BuildComp;

	/** Ch4 Test-Drive (SIB-36): PIE consumer of the branch subsystem — debug keys + desaturate/HUD/freeze */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBranchPIEComponent> BranchPIEComp;

	/** Ch6 Generate (SIB-30): budget + catalog + resolve-and-spawn for typed requests. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGenerateComponent> GenerateComp;

	/** SIB-41: the Journal/story panel widget, created on first J press. */
	UPROPERTY()
	TObjectPtr<UJournalWidget> JournalWidget;

	/** SIB-30 P1: the typed-request panel, created on first G press. */
	UPROPERTY()
	TObjectPtr<UGenerateRequestWidget> GenerateWidget;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Interact Input Action (E) */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* InteractAction;

	/** Code Vision Input Action (hold) — Started activates, Completed deactivates */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* CodeVisionAction;

	/** Refactor Input Action (R) — Started toggles the targeted refactorable */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* RefactorAction;

	/** Build Input Action (B) — Started builds the proximate affordable site (SIB-27) */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* BuildAction;

	/** "Wander world" levels (the Poplar forest etc.): the O key returns to the office and
	    the "[O] Back to Office" HUD hint appear ONLY while standing in one of these. Editable
	    so adding a future forest is just another entry — no level-name-prefix guesswork. */
	UPROPERTY(EditAnywhere, Category = "Wander World")
	TArray<FName> WanderWorldLevels = { TEXT("L_Poplar_Forest") };

public:
	ASibeliusGameCharacter();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Forwards an interact press to the interactor component */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoInteract();

	/** SIB-39: toggles the developer/HELP overlay (bound to H). */
	void ToggleDevOverlay();

	/** SIB-41: opens/closes the Journal story panel (bound to J). */
	void ToggleJournal();

	/** SIB-30 P1: opens/closes the Generate typed-request panel (bound to G). */
	void ToggleGenerate();
	void HandleGenerateSubmit(const FString& Text); // Enter in the panel
	void CloseGenerate();                           // Esc / after submit

	/** SIB-42: quit the game (bound to Q, double-press within 2s to confirm —
	    a packaged build with no exit is a hostage situation). */
	void RequestQuit();

	/** O key: from a wander world, OpenLevel back to the office (L_Office_v02). A no-op
	    anywhere else, so the same key is harmless in the office / other levels. */
	void ReturnToOffice();

private:
	float LastQuitPressTime = -100.0f;   // double-press confirm window

public:

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** True if LevelName is in the wander-world allowlist (pure membership — no world; safe
	    to call on the CDO, which the Elsewhere smoke gate does). */
	bool IsWanderWorldLevel(FName LevelName) const { return WanderWorldLevels.Contains(LevelName); }

	/** True if the player is standing in a wander world right now (current map name vs the
	    allowlist). Drives both the O-key travel and the "[O] Back to Office" HUD hint. */
	bool IsInWanderWorld() const;

};

