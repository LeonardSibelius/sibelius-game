// NpcChatWidget.cpp — the typed conversation panel (step 4).

#include "NpcChatWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
	// One palette, defined once. Dark card, warm pale type — the agents' register,
	// not Mrs. Hall's parchment memo.
	const FLinearColor CardColour(0.05f, 0.06f, 0.08f, 0.88f);
	const FLinearColor TitleColour(0.85f, 0.88f, 0.94f, 1.0f);
	const FLinearColor BodyColour(0.92f, 0.92f, 0.90f, 1.0f);
	const FLinearColor QuietColour(0.62f, 0.66f, 0.72f, 1.0f);
	const FLinearColor FieldColour(0.12f, 0.13f, 0.16f, 1.0f);
}

UNpcChatWidget::UNpcChatWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UNpcChatWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ChatRoot"));
		WidgetTree->RootWidget = Canvas;

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatCard"));
		Card->SetBrushColor(CardColour);
		Card->SetPadding(FMargin(20.0f, 16.0f));

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChatColumn"));

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChatTitle"));
		TitleText->SetText(FText::FromString(PendingAgentName.IsEmpty() ? TEXT("Agent") : PendingAgentName));
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
		TitleText->SetColorAndOpacity(FSlateColor(TitleColour));

		// The transcript grows; the card does not. A conversation that pushed the input
		// box off the bottom of the screen after six turns would be unusable exactly
		// when it started working.
		TranscriptBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChatLines"));

		Transcript = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ChatScroll"));
		Transcript->AddChild(TranscriptBox);

		USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChatScrollSize"));
		ScrollSize->SetHeightOverride(210.0f);
		ScrollSize->AddChild(Transcript);

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChatStatus"));
		StatusText->SetText(PendingStatus);
		StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Italic", 16));
		StatusText->SetColorAndOpacity(FSlateColor(QuietColour));

		InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ChatInput"));
		InputBox->SetHintText(FText::FromString(TEXT("Say something  -  Enter sends, Esc closes")));

		FEditableTextBoxStyle FieldStyle = InputBox->WidgetStyle;
		FieldStyle.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
		FieldStyle.TextStyle.SetColorAndOpacity(FSlateColor(BodyColour));
		FieldStyle.BackgroundColor = FSlateColor(FieldColour);
		InputBox->WidgetStyle = FieldStyle;
		InputBox->OnTextCommitted.AddDynamic(this, &UNpcChatWidget::HandleTextCommitted);

		USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChatInputSize"));
		InputSize->SetMinDesiredHeight(40.0f);
		InputSize->AddChild(InputBox);

		Column->AddChildToVerticalBox(TitleText);
		if (UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(ScrollSize))
		{
			ScrollSlot->SetPadding(FMargin(0.0f, 8.0f));
		}
		Column->AddChildToVerticalBox(StatusText);
		if (UVerticalBoxSlot* InputSlot = Column->AddChildToVerticalBox(InputSize))
		{
			InputSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		Card->SetContent(Column);

		if (UCanvasPanelSlot* CardSlot = Canvas->AddChildToCanvas(Card))
		{
			// Anchors + zero offsets, not a point anchor and SetSize: the Journal and
			// Generate panels both learned that a point anchor can resolve to a
			// zero-size box. Lower third, so her face keeps the frame.
			CardSlot->SetAnchors(FAnchors(0.20f, 0.56f, 0.80f, 0.95f));
			CardSlot->SetOffsets(FMargin(0.0f));
		}
	}

	return Super::RebuildWidget();
}

void UNpcChatWidget::SetAgentName(const FString& InAgentName)
{
	PendingAgentName = InAgentName;
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(InAgentName));
	}
}

void UNpcChatWidget::AppendTurn(const FString& Speaker, const FString& Line)
{
	if (!TranscriptBox)
	{
		return;
	}

	UTextBlock* Entry = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Entry->SetText(FText::FromString(Speaker.IsEmpty() ? Line : FString::Printf(TEXT("%s: %s"), *Speaker, *Line)));
	Entry->SetFont(FCoreStyle::GetDefaultFontStyle(Speaker.IsEmpty() ? "Italic" : "Regular", 19));
	Entry->SetColorAndOpacity(FSlateColor(Speaker.IsEmpty() ? QuietColour : BodyColour));
	Entry->SetAutoWrapText(true);

	if (UVerticalBoxSlot* EntrySlot = TranscriptBox->AddChildToVerticalBox(Entry))
	{
		EntrySlot->SetPadding(FMargin(0.0f, 3.0f));
	}

	if (Transcript)
	{
		// The newest line is the one being read. Without this the panel silently stops
		// updating from the player's point of view after the box fills.
		Transcript->ScrollToEnd();
	}
}

void UNpcChatWidget::SetStatus(const FText& Status)
{
	PendingStatus = Status;
	if (StatusText)
	{
		StatusText->SetText(Status);
	}
}

void UNpcChatWidget::SetInputEnabled(bool bEnabled)
{
	if (InputBox)
	{
		InputBox->SetIsEnabled(bEnabled);
	}
}

void UNpcChatWidget::FocusInput()
{
	if (InputBox)
	{
		InputBox->SetText(FText::GetEmpty());
		InputBox->SetKeyboardFocus();
	}
}

TSharedPtr<SWidget> UNpcChatWidget::GetInputSlate() const
{
	return InputBox ? InputBox->GetCachedWidget() : TSharedPtr<SWidget>();
}

void UNpcChatWidget::ResetTranscript()
{
	if (TranscriptBox)
	{
		TranscriptBox->ClearChildren();
	}
	SetStatus(FText::GetEmpty());
	SetInputEnabled(true);
	if (InputBox)
	{
		InputBox->SetText(FText::GetEmpty());
	}
}

void UNpcChatWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type Method)
{
	/* FOCUS LOSS IS NOT A CANCEL, and here it is not a send either. The Generate panel
	   paid for this lesson (SibeliusGameCharacter.cpp, CloseGenerate): opening a panel
	   is itself a focus transition, and Slate delivers a commit during it. Only Enter
	   sends; only Esc closes. Everything else leaves the panel exactly as it was. */
	if (Method != ETextCommit::OnEnter)
	{
		return;
	}

	const FString Line = Text.ToString().TrimStartAndEnd();
	if (Line.IsEmpty())
	{
		return;
	}

	OnSend.ExecuteIfBound(Line);
}

FReply UNpcChatWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
