// CathedralDoor.h
//
// SIB-31 — Ch7 entry. The attic door into the cathedral. E (shared IInteractable
// system) travels to TargetLevelName — REFUSED while branched, same guard reasoning
// as RequestDeploy: level travel would strand the snapshot stack (merge or discard
// first).
//
// Deliberately NOT IBranchable: the door must never appear in deploy saves (no
// GUID, no orphan noise on reload).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "Interactable.h"
#include "CathedralDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBranchSubsystem;

UCLASS()
class SIBELIUSGAME_API ACathedralDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACathedralDoor();

	// IInteractable: travel on E (guarded); prompt while focused.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	// The travel guard, factored static so the smoke test can drive it against a
	// NewObject'd subsystem headless (the BranchSmoke pattern). Null subsystem
	// (headless / non-game world) = allowed.
	static bool IsTravelAllowed(const UBranchSubsystem* Branch);

	bool IsRevealed() const { return bRevealed; }

	// Level to open on use. D2: the smoke test asserts this package exists on disk,
	// because OpenLevel on a bad name is a silent no-op.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	FName TargetLevelName = TEXT("L_Cathedral");

	/* WHERE IN THE TARGET LEVEL HE LANDS. None = the level's default PlayerStart, which
	   is every door's behaviour up to now and stays the default here.

	   Set it and ASibeliusGameGameMode::ChoosePlayerStart looks for a PlayerStart wearing
	   that tag instead — the same mechanism AHiddenDoor already passes and the carousel
	   already honours. The deli's return door uses it so that stepping out of Jacob's puts
	   him back on the pavement he went in from, rather than at the plaza spawn a street
	   away, which is what makes "Nyra waiting for him outside" mean anything. */
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	FName ArrivalTag;

	// SIB-43 / CL10: prompt promoted to a property so the cathedral's RETURN
	// door can say where it goes. Default preserves the attic door verbatim.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	FText PromptText = NSLOCTEXT("Sibelius", "CathedralDoorPrompt", "Enter the cathedral [E]");

	/**
	 * Turn this door off entirely — no prompt, and E does nothing.
	 *
	 * For the cathedral's RETURN door. There used to be a wooden door there that you
	 * opened with E; the door is gone and O has been the way home from any world for a
	 * long time, so the surviving prompt offered a second, undocumented exit that
	 * contradicted the HUD's own "[O] Back to Office" hint.
	 *
	 * Walt, 2026-08-19: "I use [o] now to get home so drop that prompt and that action
	 * there, [E] is saved for playing the slot machine when you get past the altar."
	 *
	 * Emptying PromptText alone would NOT have been enough: the interactor still focuses
	 * any IInteractable, so E would have kept travelling with nothing on screen to say so
	 * — an invisible exit is worse than a redundant one. This kills the action too.
	 */
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	bool bInteractive = true;

	// SIB-43 (Walt QA): arriving players spawn within reach of the return
	// door — a reflexive E bounced him straight back. The door refuses to
	// travel for this many seconds after level load. Invisible in normal play.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	float TravelGraceSeconds = 2.5f;

	// When true the door stays hidden + non-interactable until the player's
	// UGenerateComponent has spawned at least once this session. v1 ships ungated.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	bool bRequireGenerateUse = false;

	/** THE TOLL DOOR. Hidden and unusable until the cathedral machine has paid out
	 *  FProgressionState::BattleQualifyingCoinOut — 5,000 credits paid out.
	 *
	 *  Same shape as bRequireGenerateUse above, deliberately: hide, poll, reveal. A
	 *  second gating mechanism would be a second thing to get wrong, and this one has
	 *  already survived a shipped chapter.
	 *
	 *  The door being INVISIBLE rather than locked is the point. A locked door tells the
	 *  player there is somewhere they cannot go; a wall that becomes a door tells them
	 *  the machine did something. Mrs. Hall's Battle.Open line fires on the same crossing,
	 *  so she names the Architects in the same breath the way opens. */
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	bool bRequireBattleToll = false;

	UPROPERTY(VisibleAnywhere, Category = "Cathedral Door")
	TObjectPtr<USceneComponent> SceneRoot;

	// Mesh assigned in editor (SM_Door_Cathedral_Huge). BlockAll, so it blocks the
	// interactor's ECC_Visibility focus trace.
	UPROPERTY(VisibleAnywhere, Category = "Cathedral Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

protected:
	virtual void BeginPlay() override;

private:
	// ONE path drives visual + collision together (the CV4/CV8 lesson): a gated
	// door drops collision too, so the focus trace can't reach it.
	void ApplyRevealed(bool bNowRevealed);

	// Low-rate poll for the generate gate (runs only while bRequireGenerateUse
	// keeps the door hidden; cleared on reveal).
	void PollGenerateGate();
	void PollBattleTollGate();

	UBranchSubsystem* GetBranch() const;

	bool bRevealed = true;

	double TravelReadyTime = 0.0;   // world seconds; set in BeginPlay

	FTimerHandle GatePollHandle;
};
