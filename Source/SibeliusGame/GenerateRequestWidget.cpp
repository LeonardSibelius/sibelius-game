// GenerateRequestWidget.cpp — SIB-30 Ch6 P1. Native typed-request panel.

#include "GenerateRequestWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Styling/CoreStyle.h"

UGenerateRequestWidget::UGenerateRequestWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UGenerateRequestWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// Parchment box, consistent with the Journal.
		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GenBg"));
		Bg->SetBrushColor(FLinearColor(0.92f, 0.88f, 0.78f, 0.97f));
		Bg->SetPadding(FMargin(24.0f));

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GenBox"));

		UTextBlock* Prompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GenPrompt"));
		Prompt->SetText(FText::FromString(TEXT("Ask the room for...    (Enter to ask  -  Esc to cancel)")));
		Prompt->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 22));
		Prompt->SetColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.10f, 0.05f, 1.0f)));

		InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("GenInput"));
		InputBox->SetHintText(FText::FromString(TEXT("e.g. a lamp")));
		InputBox->OnTextCommitted.AddDynamic(this, &UGenerateRequestWidget::HandleTextCommitted);

		Box->AddChildToVerticalBox(Prompt);
		Box->AddChildToVerticalBox(InputBox);
		Bg->SetContent(Box);

		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Bg))
		{
			// Stretch to a centered band — the same robust anchors+offsets layout the
			// Journal panel uses (a point anchor + SetSize was producing a zero-size box).
			CSlot->SetAnchors(FAnchors(0.22f, 0.40f, 0.78f, 0.58f));
			CSlot->SetOffsets(FMargin(0.0f));
		}
	}

	return Super::RebuildWidget();
}

void UGenerateRequestWidget::FocusInput()
{
	if (InputBox)
	{
		InputBox->SetText(FText::GetEmpty());
		InputBox->SetKeyboardFocus();
	}
}

void UGenerateRequestWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter)
	{
		OnSubmit.ExecuteIfBound(Text.ToString());
	}
	else
	{
		// Esc (OnCleared) or focus-loss — close without submitting.
		OnCancel.ExecuteIfBound();
	}
}
