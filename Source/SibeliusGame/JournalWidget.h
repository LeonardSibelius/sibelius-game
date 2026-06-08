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

	// (Re)load docs/NARRATIVE.md and display it. Call before showing.
	void RefreshFromNarrative();

protected:
	// Native widget: build the scroll-box + text-block tree in code (no BP asset).
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UScrollBox> ScrollBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	// Light markdown cleanup for readable display (strip #, >, * markers).
	static FString CleanMarkdown(const FString& Raw);
};
