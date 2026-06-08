// JournalWidget.cpp — SIB-41 Journal panel (native UMG, reads docs/NARRATIVE.md).

#include "JournalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UJournalWidget::UJournalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UJournalWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// Dark, slightly-transparent reading panel.
		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("JournalBg"));
		Bg->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.93f));
		Bg->SetPadding(FMargin(28.0f));

		ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("JournalScroll"));

		BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("JournalBody"));
		BodyText->SetAutoWrapText(true);
		BodyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 22)); // readable on 4K
		BodyText->SetText(FText::FromString(TEXT("(Journal)")));

		ScrollBox->AddChild(BodyText);
		Bg->SetContent(ScrollBox);

		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Bg))
		{
			// Centered reading panel covering most of the screen.
			CSlot->SetAnchors(FAnchors(0.12f, 0.10f, 0.88f, 0.90f));
			CSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	return Super::RebuildWidget();
}

void UJournalWidget::RefreshFromNarrative()
{
	const FString Path = FPaths::ProjectDir() / TEXT("docs/NARRATIVE.md");

	FString Raw;
	FString Display;
	if (FFileHelper::LoadFileToString(Raw, *Path))
	{
		Display = CleanMarkdown(Raw);
	}
	else
	{
		Display = FString::Printf(TEXT("Journal unavailable.\n\nCould not read:\n%s\n\n(docs/ is editor-only — it isn't staged into a packaged build.)"), *Path);
	}

	if (BodyText)
	{
		BodyText->SetText(FText::FromString(Display));
	}
}

FString UJournalWidget::CleanMarkdown(const FString& Raw)
{
	TArray<FString> Lines;
	Raw.ParseIntoArrayLines(Lines, /*bCullEmpty*/ false);

	for (FString& Line : Lines)
	{
		// Strip leading ATX header (#) / blockquote (>) markers.
		Line.TrimStartInline();
		while (Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(">")))
		{
			Line.RemoveAt(0);
			Line.TrimStartInline();
		}
		// Drop emphasis asterisks (light cleanup).
		Line.ReplaceInline(TEXT("**"), TEXT(""), ESearchCase::CaseSensitive);
		Line.ReplaceInline(TEXT("*"), TEXT(""), ESearchCase::CaseSensitive);
	}

	return FString::Join(Lines, TEXT("\n"));
}
