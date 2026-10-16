// SlotCabinet.h
//
// SIB-34 / S3 — the machine at the altar. An interactable cabinet at the
// cathedral apse: E opens the USlotScreenWidget (S2), which presents the
// USlotGameModel (S1). The first thing Leonard builds with god-powers is a
// game, for someone he loves.
//
// Decisions (Walt, June 11): activation = E via the shared IInteractable
// system (supersedes the June 10 "P binding" note); free play, session-only.
//
// Deliberately NOT IBranchable (CathedralDoor reasoning): never enters deploy
// saves. Owns the SC1 input-mode transitions — the widget's Esc fires OnClosed,
// and ONLY this actor's CloseScreen() restores GameOnly + hides the cursor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "SlotCabinet.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USlotScreenWidget;

UCLASS()
class SIBELIUSGAME_API ASlotCabinet : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ASlotCabinet();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	bool IsScreenOpen() const { return bScreenOpen; }

	UPROPERTY(VisibleAnywhere, Category = "Slot Cabinet")
	TObjectPtr<USceneComponent> SceneRoot;

	// Assign in editor (the placeholder cube, later a real cabinet mesh).
	// BlockAll so the interactor's ECC_Visibility focus trace lands (SC9).
	UPROPERTY(VisibleAnywhere, Category = "Slot Cabinet")
	TObjectPtr<UStaticMeshComponent> CabinetMesh;

	/* WHICH MACHINE THIS IS (2026-10-16). The cathedral's machine is part of the story:
	   Mrs. Hall's line the first time he sits down, the paid-out toll that opens the
	   Architects' battle, and the clue chain's "he played the machine". A copy anywhere
	   else - the office's, which Walt asked for so a visitor meets the slot in the first
	   minutes - is an arcade machine: it plays for fun and sauce and touches none of that,
	   or a player could pay the endgame toll from the hall. */
	UPROPERTY(EditAnywhere, Category = "Slot Cabinet")
	bool bIsTheCathedralMachine = true;

	/* An arcade machine's prize: this much sauce for a spin that pays at least
	   BigWinTimesBet times the bet. Walt: "if you win you get 10 sauce" - big wins only,
	   his choice, because nearly every other spin pays something and spins are free. */
	UPROPERTY(EditAnywhere, Category = "Slot Cabinet", meta = (EditCondition = "!bIsTheCathedralMachine", ClampMin = "0"))
	int32 SauceForABigWin = 10;

	UPROPERTY(EditAnywhere, Category = "Slot Cabinet", meta = (EditCondition = "!bIsTheCathedralMachine", ClampMin = "1"))
	int32 BigWinTimesBet = 10;

	// THE CHROMIUM PATH IS GONE (v1.1.3). This cabinet once had a second screen that
	// embedded the real Celestial Fortune web build through UE's WebBrowserWidget.
	// Walt switched it off on 2026-08-05 — the technician's panel edits the C++ par
	// sheet, which drives the NATIVE screen only, so with the web screen on the
	// cathedral machine and the machine the panel edited were two different machines —
	// and it was removed entirely once the televisions took over that job.
	//
	// Removed with it: bUseWebScreen, WebGameURL, ResolveWebGameURL(), the WebScreen
	// member, USlotWebScreenWidget, the WebBrowserWidget plugin, the WebBrowser build
	// dependencies, the staged Content/WebGame/index.html, and 334 MB of Chromium that
	// shipped in every build for a feature nobody could reach.
	//
	// The web game is still in the game — reached the way a person reaches a website,
	// from the TVs in the living room and the bedroom (see InteractorComponent).
	//
	// The native screen is not a stub: it carries the 2026-07-17 facelift (spinning
	// reels, win presentation, procedural sound) and its 9 symbol sprites are asserted
	// by SlotSmokeTest.

private:
	void OpenScreen(APlayerController* PC);

	/** Mrs. Hall, once, the first time he sits at the reels. */
	void SayTheCathedralLine();
	void CloseScreen();   // SC1: the one place input mode is restored

	/** T at the screen — the technician's panel, layered over the machine. */
	void OpenTechPanel();

	// UFUNCTION because the panel's OnClosed is a dynamic multicast delegate.
	UFUNCTION()
	void CloseTechPanel();

	UPROPERTY()
	TObjectPtr<class USlotTechPanelWidget> TechPanel;

	UPROPERTY()
	TObjectPtr<USlotScreenWidget> Screen;

	TWeakObjectPtr<APlayerController> ScreenPC;
	bool bScreenOpen = false;   // SC2 latch
	bool bSeeded = false;       // SC10: seed once per session
};
