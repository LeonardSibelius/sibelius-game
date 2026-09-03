// GenerateRequestWidget.cpp — SIB-30 Ch6 P1. Native typed-request panel.

#include "GenerateRequestWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"   // FEditableTextBoxStyle
#include "InputCoreTypes.h"       // EKeys::Escape

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

		// Big, dark, readable text on a light field so it's unmistakable on the parchment.
		FEditableTextBoxStyle TBStyle = InputBox->WidgetStyle;
		TBStyle.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 24));
		TBStyle.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.10f, 0.07f, 0.03f, 1.0f)));
		TBStyle.BackgroundColor = FSlateColor(FLinearColor(0.98f, 0.96f, 0.90f, 1.0f)); // pale field
		InputBox->WidgetStyle = TBStyle;
		InputBox->OnTextCommitted.AddDynamic(this, &UGenerateRequestWidget::HandleTextCommitted);

		// Min size via a SizeBox (UEditableTextBox has no SetMinimumDesiredWidth in 5.7).
		USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GenInputSize"));
		InputSize->SetMinDesiredWidth(640.0f);
		InputSize->SetMinDesiredHeight(40.0f);
		InputSize->AddChild(InputBox);

		Box->AddChildToVerticalBox(Prompt);
		Box->AddChildToVerticalBox(InputSize);
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
	/* FOCUS LOSS IS NOT A CANCEL — and treating it as one is why G needed pressing twice
	   for the whole life of this feature (Walt, 2026-09-02).

	   Opening the panel is itself a focus transition: SetVisibility, then
	   SetInputMode(UIOnly) with SetWidgetToFocus on this box. Slate delivers a commit of
	   OnUserMovedFocus during that dance, the old code read anything-but-Enter as "the
	   player gave up", and the panel cancelled itself in the same frame it appeared.

	   The first G therefore opened and instantly closed it; the second opened it once
	   focus had settled. Walt learned to double-tap and reasonably assumed that was the
	   game. It was never a binding problem, which is exactly where anyone would look.

	   So only two things close this panel now, and both are the player saying so:
	   ENTER submits, and ESCAPE cancels — through NativeOnKeyDown, which handles the key
	   directly and does not depend on the text box's commit reason at all. A focus change
	   leaves the panel open, which is also just better behaviour: clicking away from a
	   text field has never meant "throw away what I typed".

	   OnCleared is left alone deliberately: it is Slate's own "the box was cleared and
	   dismissed" and is the one non-Enter commit that really does mean cancel. */
	if (Method == ETextCommit::OnEnter)
	{
		OnSubmit.ExecuteIfBound(Text.ToString());
	}
	else if (Method == ETextCommit::OnCleared)
	{
		OnCancel.ExecuteIfBound();
	}
}

TSharedPtr<SWidget> UGenerateRequestWidget::GetInputSlate() const
{
	return InputBox ? InputBox->GetCachedWidget() : TSharedPtr<SWidget>();
}

FReply UGenerateRequestWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// UIOnly swallows game input, so the panel must close itself on Esc.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCancel.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
