// SlotWebScreenWidget.h
//
// SIB-34 / Path A — the REAL Celestial Fortune, embedded. Hosts UE's Chromium
// (UWebBrowser) pointed at the celestial-fortune web build (dist/index.html,
// built with vite base './'). Reel motion, payline glow, WebAudio sound,
// free-spins autoplay — all the web game's presentation, zero re-implementation.
// The native USlotScreenWidget (S2) remains as the dependency-free fallback;
// ASlotCabinet picks via bUseWebScreen.
//
// Thematic note for the Journal someday: the machine Leonard builds in the
// cathedral IS the game Walt shipped to the world.
//
// Ledger SW1–SW6 in docs/sib-34-s2-s3-slot-cabinet-notes.md (Path A addendum):
// SW1 file:// needs relative base (vite.config.ts). SW3 Esc must be caught in
// PREVIEW key-down — Chromium consumes plain KeyDown. SW4 WebAudio unlocks on
// the first in-page click (CEF gesture policy). SW6 dev-machine URL is an
// EditAnywhere property on the cabinet — packaging will need the dist staged
// into Content (parked with SIB-42).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlotWebScreenWidget.generated.h"

class UWebBrowser;

DECLARE_DELEGATE(FOnSlotWebScreenClosed);

UCLASS()
class SIBELIUSGAME_API USlotWebScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlotWebScreenWidget(const FObjectInitializer& ObjectInitializer);

	// Cabinet calls after CreateWidget (and on re-open to reuse the instance).
	void LoadGame(const FString& URL);

	// Slate widget the cabinet should give keyboard focus (the browser itself,
	// so Space reaches the page's spin handler).
	TSharedPtr<SWidget> GetFocusTarget() const;

	FOnSlotWebScreenClosed OnClosed;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// SW3: preview phase — fires before Chromium can eat the key.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY()
	TObjectPtr<UWebBrowser> Browser;

	FString PendingURL;
};
