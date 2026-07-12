// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ProgressionTypes.h"   // FUN-1: EPowerVerb for the input gates
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

	/** The home office level. The O key + "[O] Back to Office" HUD hint are live in EVERY
	    other level and no-op here — so any new away-from-office world is covered automatically,
	    with no allowlist to maintain. Editable in case the office map is ever renamed. */
	UPROPERTY(EditAnywhere, Category = "Travel")
	FName OfficeLevelName = TEXT("L_Office_v02");

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

	/** FUN-1 — the power gate. True when the verb is earned; otherwise shows a
	    "not yet yours" line and returns false. EVERY power input routes through
	    this (the components stay ungated so smoke tests drive them directly). */
	bool CheckPowerUnlocked(EPowerVerb Verb) const;

	/** FUN-1 gated input handlers — thin wrappers: gate, then forward to the
	    component exactly as the old direct bindings did. */
	void OnCodeVisionStarted();
	void OnCodeVisionCompleted();   // ungated: releasing the key must always restore
	void OnRefactorPressed();
	void OnBuildPressed();          // the Compile chapter's verb
	void OnBranchEnterPressed();    // Test-Drive verb (key 6)
	void OnBranchMergePressed();    //                 (key 7)
	void OnBranchDiscardPressed();  //                 (key 8)
	void OnDeployPressed();         // Deploy verb     (key 0)

public:
	/** FUN-1 dev cheats (console, backtick). GrantPower accepts loose names
	    ("refactor", "test-drive"). ResetProgression wipes powers + sauce. */
	UFUNCTION(Exec) void GrantPower(const FString& PowerName);
	UFUNCTION(Exec) void GrantSauce(int32 Amount);
	UFUNCTION(Exec) void UnlockAllPowers();
	UFUNCTION(Exec) void ResetProgression();

protected:
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

	/** FUN-3: re-apply bought cauldron upgrades to this fresh pawn (components
	    spawn with authored defaults; the purchase record lives in the save). */
	virtual void BeginPlay() override;

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** True if RawLevelName (possibly PIE-prefixed) is NOT the office — i.e. an away level
	    where O / the hint are live. Strips any PIE prefix ("UEDPIE_0_") first, so PIE and
	    packaged both resolve. Pure (no world) so it's safe on the CDO, which the smoke gate
	    uses. Exported per-member: the class isn't SIBELIUSGAME_API, but the editor-module
	    gate links this (non-inline) symbol across the DLL boundary. */
	SIBELIUSGAME_API bool IsAwayFromOfficeLevelName(const FString& RawLevelName) const;

	/** True if the player is in ANY non-office level right now (current map name vs the office,
	    PIE-prefix-safe). Drives both the O-key travel and the "[O] Back to Office" HUD hint. */
	bool IsAwayFromOffice() const;

};

