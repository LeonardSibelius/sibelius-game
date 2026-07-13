// SauceShopWidget.cpp — the cauldron's shop screen (FUN-3). See header.

#include "SauceShopWidget.h"
#include "ProgressionSubsystem.h"
#include "SibeliusHUD.h"   // purchase announcements (Walt's 120-sauce lesson)
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> USauceShopWidget::RebuildWidget()
{
	// Live balance line — a TAttribute so every purchase updates it for free.
	TAttribute<FText> BalanceText = TAttribute<FText>::CreateLambda([WeakThis = TWeakObjectPtr<USauceShopWidget>(this)]()
	{
		const UProgressionSubsystem* Progression =
			WeakThis.IsValid() ? UProgressionSubsystem::Get(WeakThis.Get()) : nullptr;
		return FText::FromString(FString::Printf(TEXT("SAUCE: %d"),
			Progression ? Progression->GetSauce() : 0));
	});

	SAssignNew(OffersBox, SVerticalBox);
	RefreshOffers();

	// Solid WhiteBrush + tint = an actually-opaque panel (Walt's menu-readability
	// report; the default border brush is translucent).
	return SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
		[
			SNew(SBorder)
			.Padding(28.0f)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.04f, 0.03f, 0.94f))
			[
				SNew(SBox)
				.WidthOverride(760.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "SauceShopTitle", "THE SAUCE OF ALL KNOWLEDGE"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 30))
						.ColorAndOpacity(FLinearColor(0.5f, 1.0f, 0.6f))
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
					[
						SNew(STextBlock)
						.Text(BalanceText)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
						.ColorAndOpacity(FLinearColor(0.4f, 1.0f, 0.5f))
					]

					+ SVerticalBox::Slot().AutoHeight()
					[
						OffersBox.ToSharedRef()
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "SauceShopFooter", "click to blend    ·    [Esc] or [E] to step back"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
				]
			]
		];
}

void USauceShopWidget::RefreshOffers()
{
	if (!OffersBox.IsValid())
	{
		return;
	}
	OffersBox->ClearChildren();

	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);

	TArray<FSauceOffer> Offers;
	FSauceShop::BuildOffers(Progression ? Progression->GetStateForRead() : FProgressionState(), Offers);

	if (Offers.IsEmpty())
	{
		OffersBox->AddSlot().AutoHeight().Padding(0, 8)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("Sibelius", "SauceShopEmpty", "The Sauce has nothing left to teach you."))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 18))
			.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
		];
		return;
	}

	for (const FSauceOffer& Offer : Offers)
	{
		// Affordability as a live attribute so earning/spending re-greys rows.
		TAttribute<bool> CanAfford = TAttribute<bool>::CreateLambda(
			[WeakThis = TWeakObjectPtr<USauceShopWidget>(this), Cost = Offer.Cost]()
			{
				const UProgressionSubsystem* P =
					WeakThis.IsValid() ? UProgressionSubsystem::Get(WeakThis.Get()) : nullptr;
				return P && P->GetSauce() >= Cost;
			});

		OffersBox->AddSlot().AutoHeight().Padding(0, 4)
		[
			SNew(SButton)
			.IsEnabled(CanAfford)
			.OnClicked(FOnClicked::CreateUObject(this, &USauceShopWidget::HandleBuyClicked, Offer))
			.ContentPadding(FMargin(14.0f, 10.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s    —    %d sauce"), *Offer.Title, Offer.Cost)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Offer.Desc))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
					.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
				]
			]
		];
	}
}

FReply USauceShopWidget::HandleBuyClicked(FSauceOffer Offer)
{
	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (FSauceShop::TryPurchase(Progression, GetOwningPlayerPawn(), Offer))
	{
		// Walt's 120-sauce lesson: a purchase must ANNOUNCE itself — the
		// ceremony banner says what was blended and what it cost. (Power
		// unlocks already banner via OnPowerUnlocked; this covers upgrades.)
		if (!Offer.bIsPowerUnlock)
		{
			APlayerController* PC = GetOwningPlayer();
			if (ASibeliusHUD* HUD = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
			{
				HUD->ShowBanner(FString::Printf(TEXT("BLENDED:  %s   (−%d sauce)"),
					*Offer.Title.ToUpper(), Offer.Cost), 4.0f);
			}
		}
	}
	RefreshOffers(); // bought powers vanish; stock counters update
	return FReply::Handled();
}

FReply USauceShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::E)
	{
		CloseShop();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USauceShopWidget::CloseShop()
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
}
