// JournalWidget.h
//
// SIB-41 — the Journal/story panel. A native (C++-built) UUserWidget: a scrollable
// text panel that shows the narrative from docs/NARRATIVE.md, loaded at runtime.
// Toggled with J (see SibeliusGameCharacter). Default OFF, dismissable.
//
// NOTE: docs/ is NOT staged into a packaged build — reading it at runtime is fine for
// editor/PIE only. For shipping, bake the text into Content or a data asset.

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
