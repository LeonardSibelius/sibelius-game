// DanceMenuWidget.cpp — her dance menu. See the header.

#include "DanceMenuWidget.h"
#include "DancerAgentComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"

namespace DanceMenuNS
{
	// The Tab menu's palette (GameMenuWidget.cpp), so the two read as one game.
	const FLinearColor Heading(0.55f, 0.85f, 1.0f);
	const FLinearColor Chosen(0.4f, 1.0f, 0.5f);
	const FLinearColor Body(0.9f, 0.9f, 0.9f);
	const FLinearColor Dim(0.45f, 0.45f, 0.45f);

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}
}

void UDanceMenuWidget::Open(UDancerAgentComponent* InAgent)
{
	Agent = InAgent;
	APlayerController* PC = GetOwningPlayer();
	if (!InAgent || !PC)
	{
		return;
	}

	// Start on the dance she is doing, so the first arrow press moves away from it.
	Selected = InAgent->GetCurrentDanceIndex() + 1;
	if (!IsInViewport())
	{
		AddToViewport(60);   // with the panels, under the chat's 70
	}
	RebuildRows();

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TakeWidget());
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	SetFocus();
}

void UDanceMenuWidget::Close()
{
	if (IsInViewport())
	{
		RemoveFromParent();
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	// Give the keyboard back to the game, not just the input mode (DancerAgentComponent's
	// chat close learned this): the focus would otherwise stay in a removed widget.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UDanceMenuWidget::Choose(int32 Row)
{
	UDancerAgentComponent* Dancer = Agent.Get();
	if (!Dancer)
	{
		Close();
		return;
	}
	if (Row == 0)
	{
		Dancer->ShuffleDance();   // "Surprise me": the old F
	}
	else
	{
		Dancer->PlayDance(Row - 1);
	}
	Selected = Row;
	RebuildRows();
}

TSharedRef<SWidget> UDanceMenuWidget::RebuildWidget()
{
	using namespace DanceMenuNS;

	SAssignNew(List, SScrollBox);

	// The right-hand third of the screen; the rest stays clear so he can watch her dance.
	return SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 40.0f, 40.0f, 40.0f))
		[
			SNew(SBorder)
			.Padding(24.0f)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.05f, 0.90f))
			[
				SNew(SBox)
				.WidthOverride(420.0f)
				.MaxDesiredHeight(640.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							const UDancerAgentComponent* Dancer = Agent.Get();
							return FText::FromString(FString::Printf(TEXT("%s'S DANCE"),
								Dancer ? *Dancer->AgentName.ToUpper() : TEXT("HER")));
						})
						.Font(Font("Bold", 24))
						.ColorAndOpacity(Heading)
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						List.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "DanceMenuFooter", "[Up]/[Down] try a dance   [Enter], [F] or [Esc] done"))
						.Font(Font("Regular", 13))
						.ColorAndOpacity(Dim)
						.AutoWrapText(true)
					]
				]
			]
		];
}

void UDanceMenuWidget::RebuildRows()
{
	using namespace DanceMenuNS;

	if (!List.IsValid())
	{
		return;
	}
	List->ClearChildren();
	Rows.Reset();

	const UDancerAgentComponent* Dancer = Agent.Get();
	const int32 Count = Dancer ? Dancer->Dances.Num() : 0;
	const int32 Playing = Dancer ? Dancer->GetCurrentDanceIndex() + 1 : INDEX_NONE;
	Selected = FMath::Clamp(Selected, 0, Count);

	for (int32 Row = 0; Row <= Count; ++Row)
	{
		const FString Label = Row == 0
			? FString(TEXT("Surprise me"))
			: UDancerAgentComponent::DanceDisplayName(Dancer->Dances[Row - 1].Get());
		const bool bPlaying = Row == Playing;
		const bool bSelected = Row == Selected;

		TSharedRef<SWidget> RowWidget =
			SNew(SButton)
			.IsFocusable(false)   // the keyboard stays with the menu, so the arrows always work
			.ContentPadding(FMargin(12.0f, 6.0f))
			.ButtonColorAndOpacity(bSelected ? FLinearColor(0.15f, 0.3f, 0.4f, 1.0f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
			.OnClicked_Lambda([this, Row]() { Choose(Row); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(bPlaying ? FString::Printf(TEXT("%s   (dancing)"), *Label) : Label))
				.Font(Font(bSelected ? "Bold" : "Regular", 18))
				.ColorAndOpacity(bPlaying ? Chosen : Row == 0 ? Heading : Body)
			];
		Rows.Add(RowWidget);
		List->AddSlot().Padding(0, 1)[ RowWidget ];
	}

	if (Rows.IsValidIndex(Selected) && Rows[Selected].IsValid())
	{
		List->ScrollDescendantIntoView(Rows[Selected], /*InAnimateScroll=*/false);
	}
}

FReply UDanceMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::F || Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		/* NOT ON A REPEAT. The F that opened the menu is still held for a moment, and its
		   repeats arrive here - the menu would close the instant it opened. */
		if (!InKeyEvent.IsRepeat())
		{
			Close();
		}
		return FReply::Handled();
	}

	const UDancerAgentComponent* Dancer = Agent.Get();
	const int32 Count = Dancer ? Dancer->Dances.Num() : 0;
	if (Key == EKeys::Down || Key == EKeys::S)
	{
		// Down from "Surprise me" lands on the first dance; past the last wraps to the top.
		Choose(Selected >= Count ? 0 : Selected + 1);
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::W)
	{
		Choose(Selected <= 0 ? Count : Selected - 1);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
