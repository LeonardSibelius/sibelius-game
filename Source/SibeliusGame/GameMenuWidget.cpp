// GameMenuWidget.cpp — the Tab game menu (FUN-8). See header.

#include "GameMenuWidget.h"
#include "ProgressionSubsystem.h"
#include "SibeliusControls.h"          // the ONE control list   // IsCityOpen - the [>] row asks the key itself
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
						[ TabButton(NSLOCTEXT("Sibelius", "MenuTabRecords", "RECORDS"), ETab::Records) ]
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
	case ETab::Records:  BuildRecordsTab(ContentBox.ToSharedRef());  break;
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
	Row(TEXT("Earn it: books, Refusers fought, chapters, the temple fountain. Spend it: the kitchen cauldron. Risk it: the Carousel of Fates."), Dim, 14);

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

void UGameMenuWidget::BuildRecordsTab(TSharedRef<SVerticalBox> Box)
{
	using namespace GameMenuNS;

	const UProgressionSubsystem* P = UProgressionSubsystem::Get(this);
	auto Stat = [P](FName Key) { return P ? P->GetStat(Key) : 0; };

	// Label + right-sized number column, same shape as the CONTROLS rows.
	auto Row = [&Box](const FString& Label, const FString& Value, const FLinearColor& Color)
	{
		Box->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(150.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Value))
					.Font(Font("Bold", 20))
					.ColorAndOpacity(Color)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(Font("Regular", 18))
				.ColorAndOpacity(Body)
			]
		];
	};
	auto Section = [&Box](const FText& Title)
	{
		Box->AddSlot().AutoHeight().Padding(0, 12, 0, 4)
		[
			SNew(STextBlock)
			.Text(Title)
			.Font(Font("Bold", 20))
			.ColorAndOpacity(Heading)
		];
	};

	Section(NSLOCTEXT("Sibelius", "RecLife", "A LIFE IN NUMBERS"));
	// The stat KEY stays RefusersSlapped — it is written into every save. Only the label changes.
	Row(TEXT("Refusers fought"),   FString::FromInt(Stat(SibeliusStats::RefusersSlapped)), SauceGreen);
	Row(TEXT("sauce earned, lifetime (the wallet is on STATUS)"),
		FString::FromInt(Stat(SibeliusStats::SauceEarned)), SauceGreen);
	Row(TEXT("books collected"),    FString::FromInt(Stat(SibeliusStats::BooksCollected)), Body);
	// ("curios found" removed with the forest cut — never show a counter the
	// player cannot raise. The stat key survives for a future curio revival.)
	Row(TEXT("chapters completed"), FString::FromInt(Stat(SibeliusStats::ChaptersCompleted)), Body);

	Section(NSLOCTEXT("Sibelius", "RecCarousel", "THE CAROUSEL OF FATES"));
	Row(TEXT("runs staked"), FString::FromInt(Stat(SibeliusStats::CarouselRuns)), Body);
	Row(TEXT("runs won"),    FString::FromInt(Stat(SibeliusStats::CarouselWins)), SauceGreen);
	Row(TEXT("best run — rounds cleared"),
		FString::FromInt(Stat(SibeliusStats::CarouselBestRound)), Body);
	Row(TEXT("biggest single spin (chips)"),
		FString::FromInt(Stat(SibeliusStats::CarouselBestSpin)), Body);

	/* The toll, on the page where every other lifetime number already lives. Reads the
	   slot meters rather than a LifetimeStats key, because the meters are the authority
	   and a mirrored stat would be a second number free to disagree with the first. */
	{
		const FProgressionState& St = P ? P->GetStateForRead() : FProgressionState();
		Section(NSLOCTEXT("Sibelius", "RecArchitects", "THE ARCHITECTS"));
		Row(TEXT("credits the cathedral machine has paid out"),
			FString::Printf(TEXT("%lld"), St.BattleCreditsPaid()), Body);
		Row(St.IsBattleQualified()
				? TEXT("the toll is paid - the field is waiting")
				: TEXT("still owed before Mrs. Hall's army will have you"),
			St.IsBattleQualified()
				? FString(TEXT("PAID"))
				: FString::Printf(TEXT("%lld"),
					FProgressionState::BattleQualifyingCoinOut - St.BattleCreditsPaid()),
			St.IsBattleQualified() ? SauceGreen : Body);
	}

	Box->AddSlot().AutoHeight().Padding(0, 14, 0, 0)
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("Sibelius", "RecFooter", "These survive everything except [N N] New Game."))
		.Font(Font("Regular", 14))
		.ColorAndOpacity(Dim)
	];
}

void UGameMenuWidget::BuildControlsTab(TSharedRef<SVerticalBox> Box)
{
	using namespace GameMenuNS;

	// THE LIST LIVES IN SibeliusControls, not here. There used to be a second copy of
	// these facts in Content/Journal/HOW_TO_PLAY.md and the two had drifted in both
	// directions. This screen is now a VIEW of that list - the view that can grey out
	// what has not been earned, which is the thing prose could never do.
	const TArray<SibeliusControls::FControlRow> Rows = SibeliusControls::BuildRows(this);

	for (const SibeliusControls::FControlRow& R : Rows)
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
				.Text(FText::FromString(R.bShown ? R.Action : R.Locked))
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
