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

protected:
	virtual void BeginPlay() override;

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

	int32 InputAttempts = 0;
	FTimerHandle InputRetryHandle;

	UPROPERTY()
	TObjectPtr<UPokerScreenWidget> Screen;

	TWeakObjectPtr<APlayerController> ScreenPC;
	bool bScreenOpen = false;   // SC2 latch
	bool bSeeded = false;       // SC10: seed once per session
};
