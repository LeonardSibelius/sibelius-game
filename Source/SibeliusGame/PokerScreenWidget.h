// PokerScreenWidget.h
//
// SIDE_GAMES G5 — video poker's face. A native (C++-built) UUserWidget that
// PRESENTS UPokerGameModel: five text-rendered cards (no art assets — rank
// and suit glyphs on white card panels; nothing for the cooker to miss),
// hold toggles, the 9/6 paytable, and the sauce wallet riding every hand.
//
// Controls: Space = deal / draw · 1-5 = hold a card · Esc = leave.
// The wallet is REAL (UProgressionSubsystem): each deal spends the stake via
// TrySpendSauce, each win pays via GrantSauce — the first machine where the
// reels' lesson (model is law) meets the player's actual sauce per hand.
//
// Esc mid-hand settles first (auto-draw with current holds) so leaving can
// never eat a bet — the slot facelift's rule, kept.
//
// The cabinet (APokerMachine) owns input-mode transitions (SC1 pattern):
// Esc fires OnClosed; only the cabinet restores GameOnly.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PokerGameModel.h"
#include "PokerScreenWidget.generated.h"

class UTextBlock;
class UBorder;

DECLARE_DELEGATE(FOnPokerScreenClosed);

UCLASS()
class SIBELIUSGAME_API UPokerScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPokerScreenWidget(const FObjectInitializer& ObjectInitializer);

	// Cabinet calls once after CreateWidget (seed) and before AddToViewport.
	void InitModel(int32 Seed);
	void SetStake(int32 SaucePerHand);

	FOnPokerScreenClosed OnClosed;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	enum class EPokerPhase : uint8 { Ready, Holding };

	void TryDeal();                 // spends the stake; hints when short
	void DoDraw();                  // settles the hand, pays the wallet
	void ToggleHold(int32 Index);
	void ShowCard(int32 Index, int32 Card);
	void ShowCardBack(int32 Index);
	void RefreshHolds();
	void UpdateHud();

	// Procedural sound (the slot's cook-proof PCM trick, small edition).
	void BuildSoundBank();
	void PlayPcm(const TArray<uint8>& Pcm, float Volume);

	UPROPERTY()
	TObjectPtr<UPokerGameModel> Model;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> CardFaces;    // 5 white panels
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CardTexts; // rank+suit glyphs
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> HeldTexts; // "HELD" tags above cards

	UPROPERTY()
	TObjectPtr<UTextBlock> SauceText;
	UPROPERTY()
	TObjectPtr<UTextBlock> BetText;
	UPROPERTY()
	TObjectPtr<UTextBlock> WinText;
	UPROPERTY()
	TObjectPtr<UTextBlock> ResultText;
	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;

	TArray<int32> Hand;
	int32 HoldMask = 0;
	int32 Stake = 10;
	EPokerPhase Phase = EPokerPhase::Ready;
	bool bModelReady = false;

	TArray<uint8> DealPcm;
	TArray<uint8> WinPcm;
};
