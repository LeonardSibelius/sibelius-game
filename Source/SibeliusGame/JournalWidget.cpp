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

		// Aged-paper / parchment reading panel (Leonard's notebook).
		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("JournalBg"));
		Bg->SetBrushColor(FLinearColor(0.92f, 0.88f, 0.78f, 0.97f)); // cream/tan
		Bg->SetPadding(FMargin(28.0f));

		ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("JournalScroll"));

		BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("JournalBody"));
		BodyText->SetAutoWrapText(true);
		BodyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 22));               // readable on 4K
		BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.10f, 0.05f, 1.0f))); // dark-brown ink

		ScrollBox->AddChild(BodyText);
		Bg->SetContent(ScrollBox);

		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Bg))
		{
			// Centered reading panel covering most of the screen.
			CSlot->SetAnchors(FAnchors(0.12f, 0.10f, 0.88f, 0.90f));
			CSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		}

		ApplyText(); // show whatever's cached (or a placeholder) the instant BodyText exists
	}

	return Super::RebuildWidget();
}

void UJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The tree exists here (AddToViewport just built it) — load the narrative now.
	RefreshFromNarrative();
}

void UJournalWidget::ApplyText()
{
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(JournalText.IsEmpty() ? TEXT("(Journal — no text loaded)") : JournalText));
	}
}

void UJournalWidget::RefreshFromNarrative()
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("docs/NARRATIVE.md"));

	FString Raw;
	if (FFileHelper::LoadFileToString(Raw, *FullPath))
	{
		JournalText = CleanMarkdown(Raw);
		UE_LOG(LogTemp, Display, TEXT("[Journal] loaded %d chars from %s"), JournalText.Len(), *FullPath);
	}
	else
	{
		// Print the attempted path + reason right in the panel so failures are visible.
		const bool bExists = FPaths::FileExists(FullPath);
		JournalText = FString::Printf(
			TEXT("Journal unavailable — could not load the narrative.\n\nPath attempted:\n%s\n\nReason: %s\n\n(docs/ is editor-only; it is not staged into a packaged build — for shipping, bake the text into Content.)"),
			*FullPath, bExists ? TEXT("file exists but the read failed") : TEXT("file not found at that path"));
		UE_LOG(LogTemp, Warning, TEXT("[Journal] FAILED to load '%s' (exists=%d)"), *FullPath, bExists ? 1 : 0);
	}

	ApplyText();
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
