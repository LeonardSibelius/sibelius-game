// PokerMachine.h
//
// SIDE_GAMES G5 — the video poker cabinet on the library floor. E opens the
// UPokerScreenWidget (native, dependency-free), which presents UPokerGameModel
// with the player's real sauce riding every hand.
//
// ASlotCabinet's proven shape (SC1/SC2), minus the web path: this actor owns
// the input-mode transitions — the widget's Esc fires OnClosed, and ONLY
// CloseScreen() restores GameOnly + hides the cursor.
//
// Floor identity (SIDE_GAMES_PLAN): each machine glows its signature color —
// this one is felt green. Walt assigns the cabinet mesh and drags placement.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "PokerMachine.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPokerScreenWidget;

UCLASS()
class SIBELIUSGAME_API APokerMachine : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APokerMachine();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	bool IsScreenOpen() const { return bScreenOpen; }

	// Sauce per hand. Wins pay by the 9/6 paytable × this.
	UPROPERTY(EditAnywhere, Category = "Poker Machine", meta = (ClampMin = "1"))
	int32 BetPerHand = 10;

	UPROPERTY(VisibleAnywhere, Category = "Poker Machine")
	TObjectPtr<USceneComponent> SceneRoot;

	// Standing deck of cards (Engine cube, card proportions, M_CardAce / M_CardBack).
	// BlockAll so the interactor's ECC_Visibility focus trace lands (SC9).
	UPROPERTY(VisibleAnywhere, Category = "Poker Machine")
	TObjectPtr<UStaticMeshComponent> CabinetMesh;

	// The machine's signature color on the floor: felt green.
	UPROPERTY(VisibleAnywhere, Category = "Poker Machine")
	TObjectPtr<UPointLightComponent> Glow;

	/* HIDDEN UNTIL HE LOOKS PROPERLY (Walt, 2026-09-06).

	   The library level is being cut — he never went into it — and the poker cabinet moves
	   to the spot the door to it occupied: the south sliding glass behind his spawn.

	   THAT DOOR WAS THE CODE VISION TUTORIAL. The first two objectives in the game are
	   "Vision [V] shows hidden doors. Turn around and look at the glass" and "Poker is
	   through the glass behind you", and Tutorial.Poker is claimed by this actor when he
	   plays. Standing a plainly visible cabinet there would tell a new player to hold V at
	   something already in front of him, which teaches the opposite of the lesson.

	   So the cabinet inherits the door's trick. Hold V and it appears; once seen it stays
	   seen. Chapter 1 keeps its exact shape — hold V, something is there, go and use it —
	   and the game loses a level instead of a lesson.

	   Same mechanism as ASpaceport's Grok portal and AHiddenDoor before it, and the same
	   two rules: it does NOT re-hide when V is released (a player holding V with one hand
	   and pressing E with the other has been given a chore), and a failed bind REVEALS it
	   rather than leaving it hidden, because an unreachable poker table is worse than an
	   un-dramatic one.

	   Off by default, so the cabinet in any other level behaves exactly as it always has. */
	UPROPERTY(EditAnywhere, Category = "Poker Machine")
	bool bHiddenUntilCodeVision = false;

	bool IsRevealed() const { return bRevealed; }

protected:
	virtual void BeginPlay() override;

	/* SO IT CAN BE SEEN IN THE EDITOR, WHICH IS WHERE IT GETS PLACED.

	   ApplyDeckVisual runs at BeginPlay, so until the game started this actor had no mesh
	   at all — an empty node in the viewport. That was harmless while the cabinet lived in
	   a level nobody dragged it around in; it stopped being harmless the moment placing it
	   by eye in the living room became the job, because you cannot nudge what you cannot
	   see. OnConstruction runs in the editor and on spawn, so the deck is now there to aim
	   at.

	   IT DELIBERATELY DOES NOT APPLY THE HIDDEN STATE. bHiddenUntilCodeVision is a RUNTIME
	   secret; hiding the cabinet in the editor too would put us straight back to placing an
	   invisible object. ApplyRevealState stays in BeginPlay alone. */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Replace the default cube with a standing poker deck (ace face, card aspect). */
	void ApplyDeckVisual();

private:
	void OpenScreen(APlayerController* PC);
	void CloseScreen();   // SC1: the one place input mode is restored

	// The library level has no interaction tracer (plain DefaultPawn, raw key
	// binds — the CarouselMachine pattern), so IInteractable::Interact never
	// fires there. The cabinet binds E itself, gated to standing right at it.
	// In tracer levels (the office character) the IInteractable path still works.
	void TryEnableInput();
	void OnInteractKey();
	bool IsPlayerNear(float Radius) const;
	static constexpr float SitDownRadius = 450.0f;

	/** Bind to the player's Code Vision, retrying while the pawn is not ready. */
	void BindToCodeVision();

	/** UFUNCTION because OnCodeVisionChanged is a DYNAMIC multicast. */
	UFUNCTION()
	void HandleCodeVisionChanged(bool bIsActive);

	/** Show it for good. Idempotent. */
	void Reveal();

	void ApplyRevealState();

	bool bRevealed = false;
	int32 VisionBindAttempts = 0;
	FTimerHandle VisionBindHandle;

	int32 InputAttempts = 0;
	FTimerHandle InputRetryHandle;

	UPROPERTY()
	TObjectPtr<UPokerScreenWidget> Screen;

	TWeakObjectPtr<APlayerController> ScreenPC;
	bool bScreenOpen = false;   // SC2 latch
	bool bSeeded = false;       // SC10: seed once per session
};
