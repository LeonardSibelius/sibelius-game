// GameMenuWidget.h
//
// FUN-8 — the player-facing game menu (Walt's ask: "menus like Raymond has").
// A native (C++-built) UUserWidget in the SauceShopWidget mold: Tab opens it,
// Tab/Esc closes it, two tabs —
//   STATUS   : sauce, the six powers (earned vs not), inventory, generate budget
//   CONTROLS : the key reference, with unearned verbs greyed out
// This replaces the dev overlay as the player's window into the game state; the
// dev overlay remains on H for Walt (now default OFF — see SibeliusHUD).
//
// Raymond's convention rule applied: one menu key, Esc backs out, tabs across
// the top — the shape every PC player already knows.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMenuWidget.generated.h"

class SVerticalBox;

UCLASS()
class SIBELIUSGAME_API UGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Close + hand input back to the game. Safe to call twice.
	void CloseMenu();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
	virtual void NativeConstruct() override;

private:
	enum class ETab : uint8 { Status, Controls };

	void SetTab(ETab NewTab);
	void RefreshContent();

	void BuildStatusTab(TSharedRef<SVerticalBox> Box);
	void BuildControlsTab(TSharedRef<SVerticalBox> Box);

	ETab ActiveTab = ETab::Status;
	TSharedPtr<SVerticalBox> ContentBox;
};
