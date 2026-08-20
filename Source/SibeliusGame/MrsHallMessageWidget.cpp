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

		/* Wrap at an EXPLICIT width, and let the card grow downward to fit.
		   SetAutoWrapText alone wraps to whatever width the parent offers — fine while the
		   slot was a fixed band, useless once the card auto-sizes, because then the parent
		   offers as much width as the text asks for and a long line runs off screen in one
		   strip. WrapTextAt is the control that actually holds. (Same lesson as the SizeBox
		   width override, which does not govern text wrapping either.) */
		BodyText->SetWrapTextAt(CardWidth - 2.0f * BodyPadX);

		UBorder* BodyPad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MemoBodyPad"));
		BodyPad->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // transparent — padding only
		BodyPad->SetPadding(FMargin(BodyPadX, 18.0f));
		BodyPad->SetContent(BodyText);

		Box->AddChildToVerticalBox(HeaderBar);
		Box->AddChildToVerticalBox(BodyPad);
		Card->SetContent(Box);

		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Card))
		{
			/* THE CARD GROWS TO FIT ITS LINE.

			   This was a fixed band — FAnchors(0.26, 0.11, 0.74, 0.30), i.e. 11%..30% of
			   screen height — which was tall enough for Chapter 6's one-line refusals and
			   nothing else. The four-line opening ticket (SPINE moment 1) rendered its last
			   line BELOW the painted card, as dark text straight onto the living room, where
			   it was unreadable. Any future line longer than the ones it was tuned against
			   would have done the same.

			   Auto-size instead: a top-centre point anchor, aligned (0.5, 0) so it hangs
			   centred from that point, and the slot takes the card's desired height. Width
			   is governed by BodyText's WrapTextAt above, NOT by the slot — which is why
			   auto-size is safe here where P1's "point anchor + SetSize" produced a
			   zero-size box. */
			CSlot->SetAnchors(FAnchors(0.5f, 0.11f, 0.5f, 0.11f));
			CSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			CSlot->SetAutoSize(true);
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
