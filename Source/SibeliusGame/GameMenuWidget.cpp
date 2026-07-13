// GameMenuWidget.cpp — the Tab game menu (FUN-8). See header.

#include "GameMenuWidget.h"
#include "ProgressionSubsystem.h"
#include "InventoryComponent.h"
#include "CompileTypes.h"
#include "GenerateComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"

namespace GameMenuNS
{
	const FLinearColor Heading(0.55f, 0.85f, 1.0f);
	const FLinearColor SauceGreen(0.4f, 1.0f, 0.5f);
	const FLinearColor Body(0.9f, 0.9f, 0.9f);
	const FLinearColor Dim(0.45f, 0.45f, 0.45f);

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}
}

TSharedRef<SWidget> UGameMenuWidget::RebuildWidget()
{
	using namespace GameMenuNS;

	auto TabButton = [this](const FText& Label, ETab Tab) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ContentPadding(FMargin(22.0f, 8.0f))
			.OnClicked_Lambda([this, Tab]() { SetTab(Tab); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(Font("Bold", 20))
				.ColorAndOpacity_Lambda([this, Tab]() { return ActiveTab == Tab ? SauceGreen : Dim; })
			];
	};

	SAssignNew(ContentBox, SVerticalBox);

	// Walt's readability report: SBorder's default brush is translucent, so the
	// tint barely darkened the scene. WhiteBrush is solid — the tint becomes the
	// actual panel color.
	return SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
		[
			SNew(SBorder)
			.Padding(30.0f)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.05f, 0.94f))
			[
				SNew(SBox)
				.WidthOverride(860.0f)
				.MinDesiredHeight(560.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "MenuTitle", "LEONARD SIBELIUS"))
						.Font(Font("Bold", 30))
						.ColorAndOpacity(Heading)
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[ TabButton(NSLOCTEXT("Sibelius", "MenuTabStatus", "STATUS"), ETab::Status) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0)
						[ TabButton(NSLOCTEXT("Sibelius", "MenuTabControls", "CONTROLS"), ETab::Controls) ]
					]

					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						ContentBox.ToSharedRef()
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "MenuFooter", "[M] or [Esc] to close"))
						.Font(Font("Regular", 15))
						.ColorAndOpacity(Dim)
					]
				]
			]
		];
}

void UGameMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshContent();   // the Slate tree exists now — fill the active tab
}

void UGameMenuWidget::SetTab(ETab NewTab)
{
	ActiveTab = NewTab;
	RefreshContent();
}

void UGameMenuWidget::RefreshContent()
{
	if (!ContentBox.IsValid())
	{
		return;
	}
	ContentBox->ClearChildren();

	switch (ActiveTab)
	{
	case ETab::Status:   BuildStatusTab(ContentBox.ToSharedRef());   break;
	case ETab::Controls: BuildControlsTab(ContentBox.ToSharedRef()); break;
	}
}

void UGameMenuWidget::BuildStatusTab(TSharedRef<SVerticalBox> Box)
{
	using namespace GameMenuNS;

	auto Row = [&Box](const FString& Text, const FLinearColor& Color, int32 Size = 18, const char* Style = "Regular")
	{
		Box->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(Font(Style, Size))
			.ColorAndOpacity(Color)
			.AutoWrapText(true)   // long explainer lines wrap instead of clipping
		];
	};

	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	APawn* Pawn = GetOwningPlayerPawn();

	Row(FString::Printf(TEXT("SAUCE   %d"), Progression ? Progression->GetSauce() : 0), SauceGreen, 26, "Bold");
	Row(TEXT("Earn it: books, curios in the Many Worlds, slapped Refusers, chapters. Spend it: the kitchen cauldron. Risk it: the Carousel of Fates."), Dim, 14);

	Row(TEXT(""), Body);
	Row(TEXT("POWERS"), Heading, 20, "Bold");
	for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
	{
		const EPowerVerb Verb = static_cast<EPowerVerb>(i);
		const bool bOwned = Progression && Progression->IsUnlocked(Verb);
		Row(FString::Printf(TEXT("  %s  %s"), bOwned ? TEXT("◆") : TEXT("◇"), *PowerVerbDisplayName(Verb)),
			bOwned ? Body : Dim, 18, bOwned ? "Bold" : "Regular");
	}

	Row(TEXT(""), Body);
	Row(TEXT("INVENTORY"), Heading, 20, "Bold");
	if (UInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr)
	{
		if (const UEnum* ResEnum = StaticEnum<EResourceType>())
		{
			for (int32 i = 0; i < ResEnum->NumEnums(); ++i)
			{
				const FString Name = ResEnum->GetNameStringByIndex(i);
				if (Name.Contains(TEXT("_MAX")))
				{
					continue;
				}
				Row(FString::Printf(TEXT("  %s: %d"), *Name,
					Inv->GetCount(static_cast<EResourceType>(ResEnum->GetValueByIndex(i)))), Body);
			}
		}
	}

	if (const UGenerateComponent* Gen = Pawn ? Pawn->FindComponentByClass<UGenerateComponent>() : nullptr)
	{
		const UProgressionSubsystem* P = Progression;
		if (P && P->IsUnlocked(EPowerVerb::Generate))
		{
			Row(FString::Printf(TEXT("  Generate budget: %d"), Gen->GetRemainingBudget()), Body);
		}
	}
}

void UGameMenuWidget::BuildControlsTab(TSharedRef<SVerticalBox> Box)
{
	using namespace GameMenuNS;

	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	auto Owned = [Progression](EPowerVerb Verb)
	{
		return !Progression || Progression->IsUnlocked(Verb);
	};

	struct FControlRow { FString Keys; FString Action; bool bShown; };
	const TArray<FControlRow> Rows = {
		{ TEXT("W A S D / mouse"), TEXT("move / look"), true },
		{ TEXT("E"), TEXT("interact — collect, doors, the cauldron"), true },
		{ TEXT("F"), TEXT("slap a Refuser"), true },
		{ TEXT("V (hold)"), TEXT("Code Vision"), Owned(EPowerVerb::CodeVision) },
		{ TEXT("R"), TEXT("Refactor what you're looking at"), Owned(EPowerVerb::Refactor) },
		{ TEXT("B"), TEXT("Build at a build site"), Owned(EPowerVerb::Compile) },
		{ TEXT("6 / 7 / 8"), TEXT("Test-Drive: branch / merge / discard"), Owned(EPowerVerb::TestDrive) },
		{ TEXT("0"), TEXT("Deploy (persist your edits)"), Owned(EPowerVerb::Deploy) },
		{ TEXT("G"), TEXT("Generate — type a request"), Owned(EPowerVerb::Generate) },
		{ TEXT("M"), TEXT("this menu"), true },
		{ TEXT("J"), TEXT("how to play"), true },
		{ TEXT("O"), TEXT("back to the office (from any other world)"), true },
		{ TEXT("Q Q"), TEXT("quit"), true },
	};

	for (const FControlRow& R : Rows)
	{
		Box->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(230.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(R.Keys))
					.Font(Font("Bold", 18))
					.ColorAndOpacity(R.bShown ? Heading : Dim)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(R.bShown ? R.Action : TEXT("(a power you have not earned yet)")))
				.Font(Font("Regular", 18))
				.ColorAndOpacity(R.bShown ? Body : Dim)
			]
		];
	}
}

FReply UGameMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab
		|| InKeyEvent.GetKey() == EKeys::M)
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UGameMenuWidget::CloseMenu()
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
