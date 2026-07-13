// SauceShopWidget.h
//
// FUN-3 — the cauldron's shop screen. A native (C++-built) UUserWidget like
// UJournalWidget: no Blueprint asset, the Slate tree is built in RebuildWidget.
// Lists FSauceShop offers as clickable rows (greyed when unaffordable), shows
// the live Sauce balance, and closes on Esc/E. Opened by ASauceCauldron's
// Interact; UIOnly input while open.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SauceShop.h"
#include "SauceShopWidget.generated.h"

class SVerticalBox;

UCLASS()
class SIBELIUSGAME_API USauceShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Close + hand input back to the game. Safe to call twice.
	void CloseShop();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

private:
	// Re-list the offers (a bought power vanishes; stock lines update).
	void RefreshOffers();

	FReply HandleBuyClicked(FSauceOffer Offer);

	TSharedPtr<SVerticalBox> OffersBox;
};
