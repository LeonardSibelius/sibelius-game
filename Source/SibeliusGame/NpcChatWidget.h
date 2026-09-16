// NpcChatWidget.h
//
// The typed conversation panel (docs/NPC_CHAT_REVIEW.md, step 4). Native UUserWidget,
// no .uasset — the Journal / Generate-panel pattern.
//
// IT SITS IN THE LOWER THIRD AND IT IS DARK. Talk-E frames her face at 38 degrees and
// the HUD blanks itself for the length of the shot (docs/DANCER_VOICE.md); a bright
// parchment card in the middle of that would undo the portrait the same way the
// crosshair used to. So: her face keeps the frame, the panel keeps the floor, and the
// panel is the only lit thing on screen.
//
// Unlike the Generate panel this one STAYS OPEN across many turns, which is the whole
// difference: it has a waiting state, a failure line, and an input box that disables
// itself while a request is out.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NpcChatWidget.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE_OneParam(FOnNpcChatSend, const FString&);
DECLARE_DELEGATE(FOnNpcChatClose);

UCLASS()
class SIBELIUSGAME_API UNpcChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNpcChatWidget(const FObjectInitializer& ObjectInitializer);

	FOnNpcChatSend OnSend;
	FOnNpcChatClose OnClose;

	/** Whose conversation this is. Drawn at the top so two agents never look alike. */
	void SetAgentName(const FString& InAgentName);

	/** Add a line to the transcript. Speaker empty means a system note. */
	void AppendTurn(const FString& Speaker, const FString& Line);

	/** The one-line state under the transcript: waiting, failed, or the mock notice. */
	void SetStatus(const FText& Status);

	/** Grey the field while a request is out, so Enter cannot spend a second one. */
	void SetInputEnabled(bool bEnabled);

	/** Clear the field and take keyboard focus (call on open, and after each reply). */
	void FocusInput();

	/** The live Slate box, for FInputModeUIOnly::SetWidgetToFocus. */
	TSharedPtr<SWidget> GetInputSlate() const;

	/** Everything the next conversation must not inherit. */
	void ResetTranscript();

	/**
	 * Bottom band, or a column beside her face.
	 *
	 * Applied on open rather than at construction so the framing can be tuned in a
	 * live session: change the console variable, close, reopen. A composition call
	 * like this is made by eye, and an eye needs more than one look.
	 */
	void SetSidePanel(bool bSide);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// UIOnly swallows game input, so Esc has to be handled here or there is no way out.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type Method);

	/**
	 * The Close button, which exists because Esc cannot be tested in PIE.
	 *
	 * The editor binds StopPlaySession to Escape (DebuggerCommands.cpp), so in Play In
	 * Editor the key stops the session before this widget sees it. Esc still works in a
	 * packaged build and is still handled below — but a way out that only exists in a
	 * build nobody plays during development is a way out nobody has tested.
	 */
	UFUNCTION()
	void HandleCloseClicked();

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UScrollBox> Transcript;

	UPROPERTY()
	TObjectPtr<UVerticalBox> TranscriptBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> InputBox;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	/** Held so SetSidePanel can reach the canvas slot that positions the whole card. */
	UPROPERTY()
	TObjectPtr<UBorder> Card;

	// Held so a setter that arrives before RebuildWidget still lands.
	FString PendingAgentName;
	FText PendingStatus;
};
