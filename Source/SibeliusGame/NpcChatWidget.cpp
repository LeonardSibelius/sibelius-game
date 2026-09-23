// NpcChatWidget.cpp — the typed conversation panel (step 4).

#include "NpcChatWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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

		Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatCard"));
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

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChatStatus"));
		StatusText->SetText(PendingStatus);
		StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Italic", 16));
		StatusText->SetColorAndOpacity(FSlateColor(QuietColour));

		InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ChatInput"));
		InputBox->SetHintText(FText::FromString(TEXT("Say something  -  Enter sends, Close leaves")));

		FEditableTextBoxStyle FieldStyle = InputBox->WidgetStyle;
		FieldStyle.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
		FieldStyle.TextStyle.SetColorAndOpacity(FSlateColor(BodyColour));
		FieldStyle.BackgroundColor = FSlateColor(FieldColour);
		InputBox->WidgetStyle = FieldStyle;
		InputBox->OnTextCommitted.AddDynamic(this, &UNpcChatWidget::HandleTextCommitted);

		USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChatInputSize"));
		InputSize->SetMinDesiredHeight(40.0f);
		InputSize->AddChild(InputBox);

		/* HER NAME AND THE WAY OUT, ON ONE LINE. The button is the only exit that can
		   be exercised in PIE, so it is not a decoration — see HandleCloseClicked. */
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ChatHeader"));

		if (UHorizontalBoxSlot* TitleSlot = HeaderRow->AddChildToHorizontalBox(TitleText))
		{
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TitleSlot->SetVerticalAlignment(VAlign_Center);
		}

		CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ChatClose"));
		CloseButton->SetBackgroundColor(FLinearColor(0.18f, 0.20f, 0.24f, 0.95f));
		CloseButton->OnClicked.AddDynamic(this, &UNpcChatWidget::HandleCloseClicked);

		UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChatCloseLabel"));
		CloseLabel->SetText(FText::FromString(TEXT("Close")));
		CloseLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 16));
		CloseLabel->SetColorAndOpacity(FSlateColor(BodyColour));
		CloseButton->AddChild(CloseLabel);

		if (UHorizontalBoxSlot* CloseSlot = HeaderRow->AddChildToHorizontalBox(CloseButton))
		{
			CloseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CloseSlot->SetVerticalAlignment(VAlign_Center);
			CloseSlot->SetHorizontalAlignment(HAlign_Right);
		}

		Column->AddChildToVerticalBox(HeaderRow);
		if (UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(Transcript))
		{
			ScrollSlot->SetPadding(FMargin(0.0f, 8.0f));

			/* FILL, NOT A FIXED HEIGHT. The card is a fraction of screen height, so a
			   transcript pinned at 210px fits a 1080p window and overflows a short
			   one — and the thing it overflows into is her face. Filling makes the
			   title, status and input take their natural height and gives the
			   transcript whatever the band has left, at any resolution. */
			ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Column->AddChildToVerticalBox(StatusText);
		if (UVerticalBoxSlot* InputSlot = Column->AddChildToVerticalBox(InputSize))
		{
			InputSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		/* WHERE THE WORDS GO, EVERY TIME THE PANEL IS OPEN (docs/STEAM_AI_CHAT_DISCLOSURE.md).
		   The Steam live-generated disclosure says players are told their messages go to
		   Google, and the characters are written to be confided in (NPC_CHAT_REVIEW.md,
		   Data handling). A one-time notice is the one a player forgets or never sees after
		   a reinstall; a quiet line under the field is always true and costs one row. In
		   the mock service nothing leaves the machine, but the mock is a development
		   tool and the line describes the game players get. */
		UTextBlock* ProviderNote = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChatProviderNote"));
		ProviderNote->SetText(NSLOCTEXT("Sibelius", "NpcChatProviderNote",
			"Replies are written live by an AI (Google Gemini). What you type is sent to Google to generate them."));
		ProviderNote->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
		ProviderNote->SetColorAndOpacity(FSlateColor(QuietColour));
		ProviderNote->SetAutoWrapText(true);
		if (UVerticalBoxSlot* NoteSlot = Column->AddChildToVerticalBox(ProviderNote))
		{
			NoteSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		Card->SetContent(Column);

		if (UCanvasPanelSlot* CardSlot = Canvas->AddChildToCanvas(Card))
		{
			/* Anchors + zero offsets, not a point anchor and SetSize: the Journal and
			   Generate panels both learned that a point anchor can resolve to a
			   zero-size box. The real position is chosen in SetSidePanel, on open. */
			CardSlot->SetAnchors(FAnchors(0.20f, 0.66f, 0.80f, 0.96f));
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
	/* Kept for the packaged build, where nothing else owns Escape. In PIE the editor
	   binds it to StopPlaySession and takes it first, which is why the Close button
	   exists rather than this being the only way out. */
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNpcChatWidget::HandleCloseClicked()
{
	OnClose.ExecuteIfBound();
}

void UNpcChatWidget::SetSidePanel(bool bSide)
{
	if (!Card)
	{
		return;
	}

	UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(Card->Slot);
	if (!CardSlot)
	{
		return;
	}

	/* TWO COMPOSITIONS, AND THE SIDE ONE IS WHY THE CAMERA MOVES TOO.

	   Bottom band: the card owns a strip under her chin. It reads well and it costs
	   the bottom of her face — at 38 degrees of FOV her head fills the middle of the
	   frame, so there is nowhere else for a full-width card to go.

	   Side column: the shot yaws her left of centre (UDancerAgentComponent's chat
	   framing) and the text takes the cleared right third. Her whole face stays
	   visible, and the column is TALLER than the band was, so the transcript gains
	   lines rather than losing them. This only works because both halves move; a side
	   panel over a centred face would sit on her hair. */
	if (bSide)
	{
		CardSlot->SetAnchors(FAnchors(0.54f, 0.16f, 0.975f, 0.90f));
	}
	else
	{
		CardSlot->SetAnchors(FAnchors(0.20f, 0.66f, 0.80f, 0.96f));
	}
	CardSlot->SetOffsets(FMargin(0.0f));
}
