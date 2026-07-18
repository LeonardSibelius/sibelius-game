// SlotScreenWidget.h
//
// SIB-34 / S2 — the slot machine's face. A native (C++-built) UUserWidget that
// PRESENTS USlotGameModel (S1): 5x3 sprite grid, credits/bet/win/free-spins row,
// win-line readout. Space = spin, Esc = leave. The model decides everything;
// this class only draws it (the "reels are theater, model is law" rule).
//
// THE FACELIFT (Walt-approved 2026-07-17, BOLD_PLAN "NEXT SESSION"): the flat
// dim-then-reveal became real theater —
//   • Each reel is a clipped vertical strip scrolling the model's ACTUAL par
//     sheet strip (Strip(reel)), staggered eased stops left-to-right with a
//     bounce on landing. Space mid-spin slam-stops all reels (casino standard).
//   • Wins glow their payline cells (Line(index) patterns; Earths glow on a
//     bonus), credits COUNT UP, the win text pulses, free spins get a banner.
//   • Sound is synthesized in C++ (USoundWaveProcedural PCM: spin whir, reel
//     stop ticks, win sting, bonus fanfare) — no sound assets exist for the
//     slot, and procedural PCM can never be missed by the cooker.
//
// Economy (Walt, June 11): free play, session-only — START_CREDITS each open
// of a fresh session, no save integration. The coda is a gift.
//
// Sprite art: /Game/SlotFactory/SymbolSprites/T_sym_<id> (the June 11 vector
// set, ours, committed). ESlotSymbol::Earth uses T_sym_scatter (mapping note
// in docs/sib-34-s2-s3-slot-cabinet-notes.md).
//
// Ledger: SC1 (input mode handled by the CABINET, one close path), SC3 (stretch
// anchors), SC4 (loud texture fallback), SC5/SC6 (credits math), SC7 (the
// reveal latch survives; the reveal timers are gone — the reels are driven by
// NativeTick, so there is nothing to leak), SC10 (caller seeds).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlotTypes.h"
#include "SlotScreenWidget.generated.h"

class UTextBlock;
class UImage;
class UBorder;
class UVerticalBox;
class UAudioComponent;
class USlotGameModel;

DECLARE_DELEGATE(FOnSlotScreenClosed);
DECLARE_DELEGATE(FOnSlotTrialWon);

// Per-reel spin animation state (pure presentation — not reflected).
enum class EReelSpinState : uint8 { Idle, Spinning, Stopping, Bounce, Stopped };

struct FSlotReelAnim
{
	EReelSpinState State = EReelSpinState::Idle;
	float Pos = 0.0f;          // scroll offset within one cell step [0, STEP)
	float Velocity = 0.0f;     // px/s downward
	float StopAt = 0.0f;       // spin-elapsed time when this reel begins stopping
	float BounceTime = 0.0f;
	int32 ShiftsToStop = -1;   // landing countdown; the last 3 feeds are the result column
	int32 StripCursor = 0;     // where on the model's real strip the feed reads from
	ESlotSymbol Symbols[4] = { ESlotSymbol::Star, ESlotSymbol::Star, ESlotSymbol::Star, ESlotSymbol::Star }; // [0] hidden above, [1..3] visible rows
};

UCLASS()
class SIBELIUSGAME_API USlotScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USlotScreenWidget(const FObjectInitializer& ObjectInitializer);

	// Cabinet calls once after CreateWidget. Seeds the model (SC10).
	void InitModel(int32 Seed);

	// SHRINE TRIAL (Walt): a stake and a target instead of the leisure bankroll.
	// Call after InitModel, before AddToViewport. Reaching Target fires
	// OnTrialWon exactly once. 0 target = normal free play.
	void SetTrial(int64 StartCredits, int64 TargetCredits);

	// Fired on Esc — the cabinet restores input mode (SC1's one close path).
	FOnSlotScreenClosed OnClosed;

	// Fired once when a trial target is reached (the shrine claims through this).
	FOnSlotTrialWon OnTrialWon;

	static constexpr int64 START_CREDITS = 25000;
	static constexpr int32 TOTAL_BET = 150;   // multiple of 15 lines

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void TrySpin();                       // SC5/SC6/SC7 guards live here
	void TickReel(int32 Reel, FSlotReelAnim& R, float Dt);
	void ShiftFeed(int32 Reel, ESlotSymbol NewTop);   // strip scrolls one cell
	ESlotSymbol NextStripSymbol(int32 Reel, FSlotReelAnim& R);
	void SlamStop();                      // Space mid-spin: land everything now
	void FinishReveal();                  // credits the win, updates readouts
	void SetStripCell(int32 Reel, int32 StripIdx, ESlotSymbol Symbol);
	void RefreshReelImages(int32 Reel);
	void ClearWinDressing();              // glows off, cells full-bright
	void ShowBanner(const FString& Msg);
	void StopWhir();
	void UpdateHud();
	void LoadSymbolTextures();            // SC4: loud fallback per symbol

	// Procedural sound (PCM 16-bit mono, built once per widget).
	void BuildSoundBank();
	void PlayPcm(const TArray<uint8>& Pcm, float Volume);

	UPROPERTY()
	TObjectPtr<USlotGameModel> Model;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> Cells;     // [reel*4 + stripIdx], 20 entries

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> CellGlows; // parallel glow backing per cell

	UPROPERTY()
	TArray<TObjectPtr<UVerticalBox>> StripBoxes; // 5 translated strips

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
	TObjectPtr<UBorder> BannerBox;        // free-spins / machine-yields moment
	UPROPERTY()
	TObjectPtr<UTextBlock> BannerText;

	UPROPERTY()
	TObjectPtr<UAudioComponent> WhirComp; // the spin whir, stopped on last land

	UPROPERTY()
	TMap<ESlotSymbol, TObjectPtr<UTexture2D>> SymbolTextures;

	FSlotSpinResult PendingResult;        // result being revealed
	FSlotReelAnim Reels[5];
	float SpinElapsed = 0.0f;

	TArray<int32> GlowCellIdx;            // flat Cells indices currently glowing
	float GlowPhase = 0.0f;
	double CreditsShown = static_cast<double>(START_CREDITS); // count-up display value
	double CreditRate = 0.0;              // credits/s toward Credits
	float WinPulse = -1.0f;               // <0 = idle
	float BannerTime = -1.0f;             // <0 = hidden

	// Procedural sound bank (built lazily in BuildSoundBank).
	TArray<uint8> WhirPcm;
	TArray<uint8> TickPcm;
	TArray<uint8> WinPcm;
	TArray<uint8> FanfarePcm;

	int64 Credits = START_CREDITS;
	int64 TrialTarget = 0;                // 0 = leisure mode (the cathedral coda)
	bool bTrialWon = false;               // fire OnTrialWon exactly once
	bool bRevealing = false;              // SC7 latch
	bool bModelReady = false;
};
