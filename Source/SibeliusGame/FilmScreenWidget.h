// FilmScreenWidget.h
//
// What a film puts on the screen that is not the picture: the letterbox, the intertitle
// in the lower bar, the title cards, and the fades. It also holds the keyboard while the
// film runs, which is what keeps him from walking off mid-shot, and gives Esc to
// UFilmDirectorSubsystem::Skip.
//
// It draws nothing of its own accord: every frame it asks the director what the caption
// is and how black the screen should be. The director owns the film; this is the screen.
//
// THE WORDS GO IN THE BAR, NOT ON THE PICTURE. DancerAgentComponent.h records why: a
// subtitle across the middle of a close-up sits exactly on her mouth.
//
// Native (C++-built), like the dance menu and the Tab menu.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FilmScreenWidget.generated.h"

class UFilmDirectorSubsystem;

UCLASS()
class SIBELIUSGAME_API UFilmScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Go up over everything and take the keyboard. */
	void Open(UFilmDirectorSubsystem* InDirector);

	/** Come down and hand input back to the game. Safe to call twice. */
	void Close();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

private:
	/** A card's caption is "BIG LINE|small line"; these are the two halves. */
	FText CardLine(bool bSmall) const;

	TWeakObjectPtr<UFilmDirectorSubsystem> Director;
};
