// FilmScreenWidget.cpp — the film's screen. See the header.

#include "FilmScreenWidget.h"
#include "FilmDirectorSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace FilmScreenNS
{
	const float BarHeight = 110.0f;
	const FLinearColor Words(0.95f, 0.93f, 0.86f);      // warm white: an intertitle, not a HUD
	const FLinearColor Dim(0.4f, 0.4f, 0.4f);

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}
}

void UFilmScreenWidget::Open(UFilmDirectorSubsystem* InDirector)
{
	Director = InDirector;
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	if (!IsInViewport())
	{
		AddToViewport(80);   // over the panels (60) and the chat (70): a film owns the screen
	}

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TakeWidget());
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(false);
	SetFocus();
}

void UFilmScreenWidget::Close()
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
	// The keyboard itself, not just the input mode (the dance menu's lesson).
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

FText UFilmScreenWidget::CardLine(bool bSmall) const
{
	const UFilmDirectorSubsystem* Film = Director.Get();
	if (!Film || !Film->IsCard())
	{
		return FText::GetEmpty();
	}
	FString Big = Film->GetCaption();
	FString Small;
	Film->GetCaption().Split(TEXT("|"), &Big, &Small);
	return FText::FromString(bSmall ? Small : Big);
}

TSharedRef<SWidget> UFilmScreenWidget::RebuildWidget()
{
	using namespace FilmScreenNS;

	return SNew(SOverlay)

		// The fade, and the black a card sits on. Under the bars, so they never flicker.
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(White())
			.ColorAndOpacity_Lambda([this]()
			{
				const UFilmDirectorSubsystem* Film = Director.Get();
				return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, Film ? Film->GetBlackAlpha() : 0.0f));
			})
		]

		// The top bar.
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SNew(SBox).HeightOverride(BarHeight)
			[
				SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor::Black)
			]
		]

		// The bottom bar, with the intertitle in it.
		+ SOverlay::Slot().VAlign(VAlign_Bottom)
		[
			SNew(SBox).HeightOverride(BarHeight)
			[
				SNew(SBorder)
				.BorderImage(White())
				.BorderBackgroundColor(FLinearColor::Black)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const UFilmDirectorSubsystem* Film = Director.Get();
						return Film && !Film->IsCard() ? FText::FromString(Film->GetCaption()) : FText::GetEmpty();
					})
					.Font(Font("Italic", 26))
					.ColorAndOpacity(Words)
					.Justification(ETextJustify::Center)
				]
			]
		]

		// A title card: the big line, and a small one under it.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return CardLine(/*bSmall=*/false); })
				.Font(Font("Bold", 40))
				.ColorAndOpacity(Words)
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 18.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return CardLine(/*bSmall=*/true); })
				.Font(Font("Italic", 20))
				.ColorAndOpacity(Words)
				.Justification(ETextJustify::Center)
			]
		]

		// How to leave.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.0f, 40.0f, 32.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("Sibelius", "FilmSkip", "[Esc] skip"))
			.Font(Font("Regular", 12))
			.ColorAndOpacity(Dim)
		];
}

FReply UFilmScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Everything is swallowed: no key reaches the game while a film runs. Only Esc acts,
	// and not on a repeat - the Enter that chose the film is still held as this opens.
	if (InKeyEvent.GetKey() == EKeys::Escape && !InKeyEvent.IsRepeat())
	{
		if (UFilmDirectorSubsystem* Film = Director.Get())
		{
			Film->Skip();
		}
	}
	return FReply::Handled();
}

FReply UFilmScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// A click must not carry the keyboard off to the viewport, or Esc stops arriving.
	return FReply::Handled();
}
