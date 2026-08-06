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
class USlotWebScreenWidget;

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

	// Path A: open the REAL Celestial Fortune (Chromium) instead of the native
	// S2 screen. The native screen stays as the dependency-free fallback.
	//
	// DEFAULT FLIPPED TO FALSE (Walt, 2026-08-05). The technician's panel edits the C++
	// par sheet, which drives the NATIVE screen only — the web build's maths lives
	// inside a minified JS bundle that cannot be edited from C++, gate-tested, or shown
	// in a live report. With the web screen on, the cathedral machine and the machine
	// the panel edits were two different machines.
	//
	// The placed cabinet in L_Cathedral does not override this, so it follows the class
	// default. The native screen is not a stub: it carries the 2026-07-17 facelift
	// (spinning reels, win presentation, procedural sound) and its 9 symbol sprites are
	// asserted by SlotSmokeTest.
	UPROPERTY(EditAnywhere, Category = "Slot Cabinet")
	bool bUseWebScreen = false;

	// SW6/SIB-42: dev-machine FALLBACK only. At runtime ResolveWebGameURL()
	// prefers the staged copy at Content/WebGame/index.html (packaged as a
	// loose NonUFS file so Chromium can read it from disk on any machine);
	// this property is the dev override when the staged copy is absent.
	UPROPERTY(EditAnywhere, Category = "Slot Cabinet")
	FString WebGameURL = TEXT("file:///C:/Users/wpark/projects/celestial-fortune/dist/index.html");

	// Staged-first, dev-fallback URL resolution (PK1 in docs/sib-42-packaging-notes.md).
	FString ResolveWebGameURL() const;

private:
	void OpenScreen(APlayerController* PC);
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

	UPROPERTY()
	TObjectPtr<USlotWebScreenWidget> WebScreen;

	TWeakObjectPtr<APlayerController> ScreenPC;
	bool bScreenOpen = false;   // SC2 latch
	bool bSeeded = false;       // SC10: seed once per session
};
