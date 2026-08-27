// JournalWidget.h
//
// SIB-41 — the Journal panel. A native (C++-built) UUserWidget: a scrollable text panel.
// Toggled with J (see SibeliusGameCharacter). Default OFF, dismissable.
//
// It shows TWO things:
//   1. Content/Journal/HOW_TO_PLAY.md — the player's how-to guide, staged as a loose
//      NonUFS file so it ships. (docs/HOW_TO_PLAY.md is the editor-time fallback.)
//   2. The player's own collected memoir messages — see ComposeMemoirRecord.
//
// This comment used to say the panel showed docs/NARRATIVE.md, which stopped being true
// when Walt made J the how-to-play guide. The stale version cost real time: a reader of
// the comment concludes the game hands the player its own lore bible, and writes that
// down as a design problem (docs/SPINE.md, since corrected). Read the function.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JournalWidget.generated.h"

class UScrollBox;
class UTextBlock;

UCLASS()
class SIBELIUSGAME_API UJournalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UJournalWidget(const FObjectInitializer& ObjectInitializer);

	// (Re)load docs/NARRATIVE.md into the cached text and push it to the panel.
	void RefreshFromNarrative();

protected:
	// Native widget: build the scroll-box + text-block tree in code (no BP asset).
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Runs once the Slate tree exists (on AddToViewport) — safe place to load text.
	virtual void NativeConstruct() override;

private:
	// Push the cached text (or a placeholder) into BodyText, if it's been built yet.
	void ApplyText();

	/** The player's collected memoir messages, appended under the how-to-play text.
	 *  Derived from saved progression — see the comment on the implementation for why it
	 *  deliberately adds no new save field. */
	FString ComposeMemoirRecord() const;

	/** The standing invitation to the battle, and the toll still owed for it. Reads the
	 *  Cathedral machine's lifetime CoinOut - see FProgressionState::IsBattleQualified. */
	FString ComposeArchitects() const;

	UPROPERTY()
	TObjectPtr<UScrollBox> ScrollBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	// Cached narrative text — survives independent of when BodyText is constructed,
	// so the load result is never lost to RebuildWidget timing.
	FString JournalText;

	// Light markdown cleanup for readable display (strip #, >, * markers).
	static FString CleanMarkdown(const FString& Raw);
};
