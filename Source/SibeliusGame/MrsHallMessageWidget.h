// MrsHallMessageWidget.h
//
// SIB-30 — Ch6 P2. A clearly-visible, styled "office memo" that delivers Mrs. Hall's
// refusal — distinct from the small success toast, readable at 4K. Native UUserWidget
// built in C++ (the Journal/Generate-panel pattern; no .uasset). Skinnable: Walt refines
// the visual in playtest. It does NOT take focus — the player keeps playing and it
// auto-dismisses (the GenerateComponent owns the timer).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MrsHallMessageWidget.generated.h"

class UTextBlock;

UCLASS()
class SIBELIUSGAME_API UMrsHallMessageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMrsHallMessageWidget(const FObjectInitializer& ObjectInitializer);

	// Set the memo body (the line she says). Safe before or after construction.
	void SetMessage(const FText& Line);

	/**
	 * Memo width in pixels, and the horizontal padding inside it.
	 *
	 * ABSOLUTE, not a fraction of the screen, on purpose: the fonts here are absolute sizes
	 * too (22 and 26), so a relative width changed the characters-per-line with resolution
	 * while the glyphs stayed put. A fixed width keeps the line breaks identical everywhere.
	 *
	 * The card's HEIGHT is deliberately NOT fixed — it grows to fit however many lines the
	 * body wraps to. It used to be pinned to an 11%..30% band, which fit Chapter 6's
	 * one-line refusals and dropped the last line of anything longer onto the floor of the
	 * living room, unreadable and unremarked.
	 */
	static constexpr float CardWidth = 900.0f;
	static constexpr float BodyPadX  = 22.0f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	// Held so a SetMessage that arrives before RebuildWidget still lands on the body.
	FText PendingMessage;
};
