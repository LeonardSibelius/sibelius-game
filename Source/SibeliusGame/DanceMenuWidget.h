// DanceMenuWidget.h
//
// F on an AI agent: choose her dance. Walt, 2026-10-07: "would it be possible for F change her
// dance to pull up a big menu of dances, maybe all the Morro dances plus all the Mixamo dances
// as well and the player can select from them?" - "yes start the menu with the Morro dances".
//
// A panel down the RIGHT of the screen, not a dimmed overlay, so he watches her while he
// chooses. The arrow keys audition: moving the highlight plays that dance at once. A click plays
// one. "Surprise me" keeps the old F - a random different dance. Enter, F or Esc closes, and she
// keeps dancing whatever was last played; it is saved as his choice for her
// (UDancerAgentComponent::PlayDance).
//
// Native (C++-built) like the Tab menu (GameMenuWidget.h). The list is her Dances array, so a
// dance added there - Mixamo's, next - appears here with nothing else to change.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DanceMenuWidget.generated.h"

class UDancerAgentComponent;
class SScrollBox;
class SWidget;

UCLASS()
class SIBELIUSGAME_API UDanceMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Show her dances and take the keyboard and mouse. */
	void Open(UDancerAgentComponent* InAgent);

	/** Hand input back to the game. Safe to call twice. */
	void Close();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

private:
	/* Row 0 is "Surprise me". Then the film rows - row 1 has [G]'s generator write a new
	   one (FilmGenerator.h), rows 2.. are the written films (FilmCatalog, 2026-11-09) -
	   then her dances: row N is her Dances[N - 1 - NumFilms()]. NumFilms counts row 1.

	   A FILM ROW DOES NOT AUDITION. Arrowing onto a dance plays it at once, which is the
	   point of the menu; arrowing onto a film only highlights it, because a film takes
	   the screen for a minute. Enter or a click plays it, with her as its star. */
	void Choose(int32 Row);
	void PlayFilm(int32 Row);
	static int32 NumFilms();
	bool IsFilmRow(int32 Row) const { return Row >= 1 && Row <= NumFilms(); }
	void RebuildRows();

	TWeakObjectPtr<UDancerAgentComponent> Agent;
	int32 Selected = 0;
	TSharedPtr<SScrollBox> List;
	TArray<TSharedPtr<SWidget>> Rows;
};
