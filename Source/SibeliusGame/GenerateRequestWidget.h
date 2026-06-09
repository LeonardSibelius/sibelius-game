// GenerateRequestWidget.h
//
// SIB-30 — Ch6 P1. The typed-request panel (DD = typed text). A native UUserWidget
// (same pattern as the Journal): a parchment box with an editable text field. Enter
// submits (OnSubmit), Esc / focus-loss cancels (OnCancel). The character owns open/close.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GenerateRequestWidget.generated.h"

class UEditableTextBox;

DECLARE_DELEGATE_OneParam(FOnGenerateSubmit, const FString&);
DECLARE_DELEGATE(FOnGenerateCancel);

UCLASS()
class SIBELIUSGAME_API UGenerateRequestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGenerateRequestWidget(const FObjectInitializer& ObjectInitializer);

	FOnGenerateSubmit OnSubmit;
	FOnGenerateCancel OnCancel;

	// Clear the field and give it keyboard focus (call on open).
	void FocusInput();

	// The text box's live Slate widget — for FInputModeUIOnly::SetWidgetToFocus so
	// keystrokes go to the field (not the pawn).
	TSharedPtr<SWidget> GetInputSlate() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// In UIOnly the game can't receive Esc, so the panel closes itself from here.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type Method);

private:
	UPROPERTY()
	TObjectPtr<UEditableTextBox> InputBox;
};
