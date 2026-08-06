// SlotTechPanelWidget.h
//
// THE TECHNICIAN'S PANEL — step 3 of docs/PAR_SHEET_PANEL.md.
//
// Three knobs, and the par sheet report recomputed the instant one moves. Open it with
// T while standing at the machine.
//
// WHY T FROM INSIDE THE SCREEN, RATHER THAN A SIDE DOOR
//
// The design doc pictured a service door on the side of the cabinet, opened with E. That
// needs a second actor with its own mesh, placed by hand in L_Cathedral. This is the
// same fiction with none of that: a real machine's attendant menu lives INSIDE the
// machine, behind a key the floor staff carry. Press T at the screen and the reels stop
// and the numbers come up.
//
// It also solves a discoverability problem. World interaction prompts go through
// GEngine->AddOnScreenDebugMessage, which is SUPPRESSED in Shipping builds — so a
// prompt on a side door would be invisible to every actual player. The hint here is a
// UMG text block inside the widget, which renders in Shipping like any other UI.
//
// THE PANEL IS THE PAR SHEET REPORT, LIVE
//
// It shows what SlotSmokeTest prints, updating as a knob turns — including per-symbol
// contribution to RTP, which is the anatomy that turns a slider into a lesson. HOLD sits
// beside RTP because that is the vocabulary the operator side of the business uses, and
// the one Walt's own warehouse reports were written in.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlotParSheet.h"
#include "SlotParSheetMath.h"
#include "SlotTechPanelWidget.generated.h"

class USlotGameModel;
class UTextBlock;
class UBorder;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTechPanelClosed);

UCLASS()
class SIBELIUSGAME_API USlotTechPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Point the panel at the machine it edits. Call before adding to the viewport. */
	void Setup(USlotGameModel* InModel);

	/** Fires when the player closes the panel (Esc). The cabinet restores input mode. */
	UPROPERTY(BlueprintAssignable)
	FOnTechPanelClosed OnClosed;

	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** Which knob the player is on. Order matches the doc: pure, entangled, feel. */
	enum class EKnob : uint8 { Pays, Wilds, Jackpot, Count };

	void BuildTree();
	void Refresh();

	/** Rebuild the par sheet FROM THE FACTORY SHEET plus the current knob values. */
	FSlotParSheet ComposeParSheet() const;

	void AdjustSelected(int32 Direction);
	void RevertToFactory();

	FString ComposeReportText(const FSlotParSheetReport& Rep, const FSlotParSheet& Sheet) const;

	UPROPERTY()
	TObjectPtr<USlotGameModel> Model;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY()
	TObjectPtr<UTextBlock> FooterText;

	UPROPERTY()
	TObjectPtr<UBorder> Panel;

	/**
	 * The untouched shipped machine. Every edit is composed from THIS plus the knobs,
	 * never applied on top of the previous edit — so repeated adjustments cannot
	 * accumulate rounding drift, and REVERT is exact rather than approximate.
	 */
	FSlotParSheet Factory;

	EKnob Selected = EKnob::Pays;

	// Knob values. Ranges are provisional per the design doc and get re-tuned against
	// real numbers now that the calculator exists.
	double PaysMultiplier = 1.0;    // 0.70 .. 1.30
	int32  WildCount      = 1;      // 0 .. 3
	double JackpotPay     = 1000.0; // 250 .. 4000, the Seven's five-of-a-kind
};
