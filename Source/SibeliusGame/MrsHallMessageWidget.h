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

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	// Held so a SetMessage that arrives before RebuildWidget still lands on the body.
	FText PendingMessage;
};
