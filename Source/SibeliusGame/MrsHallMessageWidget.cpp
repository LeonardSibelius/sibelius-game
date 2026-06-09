// MrsHallMessageWidget.cpp — SIB-30 Ch6 P2. The styled Mrs. Hall "memo" notice.

#include "MrsHallMessageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

UMrsHallMessageWidget::UMrsHallMessageWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UMrsHallMessageWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// Manila memo card — the remote-manager "note from the boss" aesthetic.
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MemoCard"));
		Card->SetBrushColor(FLinearColor(0.86f, 0.82f, 0.70f, 0.98f));
		Card->SetPadding(FMargin(0.0f));

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MemoBox"));

		// Dark header bar — attributes the note to Mrs. Hall (she's named; he is not).
		UBorder* HeaderBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MemoHeaderBar"));
		HeaderBar->SetBrushColor(FLinearColor(0.20f, 0.17f, 0.12f, 1.0f));
		HeaderBar->SetPadding(FMargin(20.0f, 10.0f));

		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MemoHeader"));
		Header->SetText(FText::FromString(TEXT("MEMO  -  from the desk of Mrs. Hall")));
		Header->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
		Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.88f, 0.78f, 1.0f)));
		HeaderBar->SetContent(Header);

		// Body — the line she says. Big, dark, wrapped; readable at 4K (HELP/Journal scale).
		BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MemoBody"));
		BodyText->SetText(PendingMessage);
		BodyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 26));
		BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.09f, 0.05f, 1.0f)));
		BodyText->SetAutoWrapText(true);

		UBorder* BodyPad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MemoBodyPad"));
		BodyPad->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // transparent — padding only
		BodyPad->SetPadding(FMargin(22.0f, 18.0f));
		BodyPad->SetContent(BodyText);

		Box->AddChildToVerticalBox(HeaderBar);
		Box->AddChildToVerticalBox(BodyPad);
		Card->SetContent(Box);

		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Card))
		{
			// Upper-center band, clear of the reticle. Stretch anchors + zero offsets — the
			// robust layout from P1 (a point anchor + SetSize produced a zero-size box).
			CSlot->SetAnchors(FAnchors(0.26f, 0.11f, 0.74f, 0.30f));
			CSlot->SetOffsets(FMargin(0.0f));
		}
	}

	return Super::RebuildWidget();
}

void UMrsHallMessageWidget::SetMessage(const FText& Line)
{
	PendingMessage = Line;
	if (BodyText)
	{
		BodyText->SetText(Line);
	}
}
