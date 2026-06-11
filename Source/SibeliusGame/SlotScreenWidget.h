// SlotScreenWidget.h
//
// SIB-34 / S2 — the slot machine's face. A native (C++-built) UUserWidget that
// PRESENTS USlotGameModel (S1): 5x3 sprite grid, credits/bet/win/free-spins row,
// win-line readout. Space = spin, Esc = leave. The model decides everything;
// this class only draws it (the "reels are theater, model is law" rule).
//
// Economy (Walt, June 11): free play, session-only — START_CREDITS each open
// of a fresh session, no save integration. The coda is a gift.
//
// Sprite art: /Game/SlotFactory/SymbolSprites/T_sym_<id> (the June 11 vector
// set, ours, committed). ESlotSymbol::Earth uses T_sym_scatter (mapping note
// in docs/sib-34-s2-s3-slot-cabinet-notes.md).
//
// Ledger: SC1 (input mode handled by the CABINET, one close path), SC3 (stretch
// anchors), SC4 (loud texture fallback), SC5/SC6 (credits math), SC7 (reveal
// latch + timer cleanup), SC10 (caller seeds).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlotTypes.h"
#include "SlotScreenWidget.generated.h"

class UTextBlock;
class UImage;
class USlotGameModel;

DECLARE_DELEGATE(FOnSlotScreenClosed);

UCLASS()
class SIBELIUSGAME_API USlotScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlotScreenWidget(const FObjectInitializer& ObjectInitializer);

	// Cabinet calls once after CreateWidget. Seeds the model (SC10).
	void InitModel(int32 Seed);

	// Fired on Esc — the cabinet restores input mode (SC1's one close path).
	FOnSlotScreenClosed OnClosed;

	static constexpr int64 START_CREDITS = 25000;
	static constexpr int32 TOTAL_BET = 150;   // multiple of 15 lines

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void TrySpin();                       // SC5/SC6/SC7 guards live here
	void RevealReel(int32 ReelIndex);     // staggered reveal step
	void FinishReveal();                  // credits the win, updates readouts
	void SetCell(int32 Reel, int32 Row, ESlotSymbol Symbol, bool bDimmed);
	void UpdateHud();
	void LoadSymbolTextures();            // SC4: loud fallback per symbol

	UPROPERTY()
	TObjectPtr<USlotGameModel> Model;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> Cells;     // [reel*3 + row], 15 entries

	UPROPERTY()
	TObjectPtr<UTextBlock> CreditsText;
	UPROPERTY()
	TObjectPtr<UTextBlock> WinText;
	UPROPERTY()
	TObjectPtr<UTextBlock> FreeSpinsText;
	UPROPERTY()
	TObjectPtr<UTextBlock> LinesText;
	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY()
	TMap<ESlotSymbol, TObjectPtr<UTexture2D>> SymbolTextures;

	FSlotSpinResult PendingResult;        // result being revealed
	TArray<FTimerHandle> RevealTimers;    // SC7: cleared on destruct/close

	int64 Credits = START_CREDITS;
	bool bRevealing = false;              // SC7 latch
	bool bModelReady = false;
};
