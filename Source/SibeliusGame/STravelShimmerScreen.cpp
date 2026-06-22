// STravelShimmerScreen.cpp — see header. Pure C++ Slate, no UMG asset.

#include "STravelShimmerScreen.h"

#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"

void STravelShimmerScreen::Construct(const FArguments& InArgs)
{
	bFadeIn = InArgs._FadeIn;
	bFadeOut = InArgs._FadeOut;
	FadeDuration = FMath::Max(0.01f, InArgs._FadeDuration);
	OnFadeComplete = InArgs._OnFadeComplete;

	// Cosmetic only — never trap gameplay input behind the cover.
	SetVisibility(EVisibility::HitTestInvisible);

	// A self-contained OPAQUE backdrop. FCoreStyle's brush can render as a transparency
	// checkerboard on the MoviePlayer loading thread; a baked color brush is solid in both
	// the viewport and the loading-screen contexts. Function-local static -> stable address
	// for the SBorder's BorderImage pointer.
	static const FSlateColorBrush BackdropBrush(FLinearColor(0.015f, 0.015f, 0.02f, 1.0f));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&BackdropBrush)                                      // solid near-black, fully opaque
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SThrobber)
					.NumPieces(7)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 28.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(InArgs._ContextText)
					.Justification(ETextJustify::Center)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.87f, 1.0f, 1.0f)))
				]
			]
		]
	];

	SetRenderOpacity(bFadeIn ? 0.0f : 1.0f);

	if (bFadeIn || bFadeOut)
	{
		RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &STravelShimmerScreen::DriveFade));
	}
}

EActiveTimerReturnType STravelShimmerScreen::DriveFade(double /*InCurrentTime*/, float InDeltaTime)
{
	Elapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / FadeDuration, 0.0f, 1.0f);
	SetRenderOpacity(bFadeIn ? Alpha : (1.0f - Alpha));

	if (Alpha >= 1.0f)
	{
		if (bFadeOut)
		{
			OnFadeComplete.ExecuteIfBound();
		}
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}
