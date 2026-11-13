// DanceMenuWidget.cpp — her dance menu. See the header.

#include "DanceMenuWidget.h"
#include "DancerAgentComponent.h"
#include "FilmDirectorSubsystem.h"         // the film rows
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
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

	/** "1x", "1.5x", "0.75x" - no trailing zeros. */
	FString SpeedLabel(float Speed)
	{
		const int32 Hundredths = FMath::RoundToInt(Speed * 100.0f);
		if (Hundredths % 100 == 0)
		{
			return FString::Printf(TEXT("%dx"), Hundredths / 100);
		}
		if (Hundredths % 10 == 0)
		{
			return FString::Printf(TEXT("%.1fx"), Hundredths / 100.0f);
		}
		return FString::Printf(TEXT("%.2fx"), Hundredths / 100.0f);
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
	Part = EPart::Whole;
	const int32 Current = InAgent->GetCurrentDanceIndex();
	Selected = Current == INDEX_NONE ? 0 : Current + 1 + NumFilms();
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
		if (Part == EPart::Whole)
		{
			Dancer->ShuffleDance();   // "Surprise me": the old F
		}
		else if (Dancer->Dances.Num() >= 2)
		{
			// Surprise ARMS or surprise LEGS: a random different dance for that half only.
			const int32 Now = Part == EPart::Arms ? Dancer->GetDanceArmsIndex() : Dancer->GetCurrentDanceIndex();
			int32 Pick = FMath::RandRange(0, Dancer->Dances.Num() - 2);
			if (Now != INDEX_NONE && Pick >= Now)
			{
				++Pick;
			}
			Part == EPart::Arms ? Dancer->SetDanceArms(Pick) : Dancer->SetDanceLegs(Pick);
		}
	}
	else if (!IsFilmRow(Row))
	{
		const int32 Dance = Row - 1 - NumFilms();
		switch (Part)
		{
		case EPart::Arms: Dancer->SetDanceArms(Dance); break;
		case EPart::Legs: Dancer->SetDanceLegs(Dance); break;
		default:          Dancer->PlayDance(Dance);    break;
		}
	}
	// A film row is only highlighted here. PlayFilm starts it.
	Selected = Row;
	RebuildRows();
}

int32 UDanceMenuWidget::RowOfDance(int32 DanceIndex) const
{
	return DanceIndex == INDEX_NONE ? INDEX_NONE : DanceIndex + 1 + NumFilms();
}

void UDanceMenuWidget::SetPart(EPart NewPart)
{
	Part = NewPart;

	// The highlight goes to the dance that tab controls, so the next arrow press moves from it.
	if (const UDancerAgentComponent* Dancer = Agent.Get())
	{
		const int32 Row = RowOfDance(Part == EPart::Arms ? Dancer->GetDanceArmsIndex() : Dancer->GetCurrentDanceIndex());
		if (Row != INDEX_NONE)
		{
			Selected = Row;
		}
	}
	RebuildRows();
}

void UDanceMenuWidget::StepSpeed(float Delta)
{
	if (UDancerAgentComponent* Dancer = Agent.Get())
	{
		Dancer->SetDanceSpeed(Dancer->GetDanceSpeed() + Delta);
	}
}

void UDanceMenuWidget::ToggleMirror()
{
	if (UDancerAgentComponent* Dancer = Agent.Get())
	{
		Dancer->SetDanceMirror(!Dancer->IsDanceMirrored());
	}
}

int32 UDanceMenuWidget::NumFilms()
{
	return 1 + FilmCatalog::All().Num();   // "make a new one", then the written ones
}

void UDanceMenuWidget::PlayFilm(int32 Row)
{
	UDancerAgentComponent* Dancer = Agent.Get();
	if (!Dancer || !IsFilmRow(Row))
	{
		return;
	}
	const bool bNewOne = Row == 1;
	const FName FilmId = bNewOne ? NAME_None : FilmCatalog::All()[Row - 2].Id;

	// The menu comes down first: it hands the keyboard back to the game, and the film's
	// own screen then takes it. The other order would leave the film without Esc.
	Close();
	if (UWorld* World = Dancer->GetWorld())
	{
		if (UFilmDirectorSubsystem* Director = World->GetSubsystem<UFilmDirectorSubsystem>())
		{
			if (bNewOne)
			{
				Director->PlayGenerated(FString(), Dancer);
			}
			else
			{
				Director->Play(FilmId, Dancer);
			}
		}
	}
}

TSharedRef<SWidget> UDanceMenuWidget::RebuildWidget()
{
	using namespace DanceMenuNS;

	SAssignNew(List, SScrollBox);

	// A tab: lit when it is the one the dance rows answer to.
	auto PartTab = [this](const TCHAR* Label, EPart Which) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ContentPadding(FMargin(10.0f, 5.0f))
			.OnClicked_Lambda([this, Which]() { SetPart(Which); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(Font("Bold", 15))
				.ColorAndOpacity_Lambda([this, Which]() { return Part == Which ? Chosen : Dim; })
			];
	};
	// A small button with a fixed label.
	auto SmallButton = [](const TCHAR* Label, TFunction<void()> OnClick) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ContentPadding(FMargin(10.0f, 3.0f))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(Font("Bold", 15))
				.ColorAndOpacity(Body)
			];
	};

	// The remix controls: which half the rows change, then speed and mirror.
	TSharedRef<SWidget> Controls =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ PartTab(TEXT("WHOLE DANCE"), EPart::Whole) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[ PartTab(TEXT("ARMS"), EPart::Arms) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[ PartTab(TEXT("LEGS"), EPart::Legs) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("SPEED"))).Font(Font("Bold", 15)).ColorAndOpacity(Heading)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)[ SmallButton(TEXT("-"), [this]() { StepSpeed(-0.25f); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					const UDancerAgentComponent* Dancer = Agent.Get();
					return FText::FromString(DanceMenuNS::SpeedLabel(Dancer ? Dancer->GetDanceSpeed() : 1.0f));
				})
				.Font(Font("Bold", 16))
				.ColorAndOpacity(Body)
			]
			+ SHorizontalBox::Slot().AutoWidth()[ SmallButton(TEXT("+"), [this]() { StepSpeed(0.25f); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(22, 0, 0, 0)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("MIRROR"))).Font(Font("Bold", 15)).ColorAndOpacity(Heading)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(10.0f, 3.0f))
				.OnClicked_Lambda([this]() { ToggleMirror(); return FReply::Handled(); })
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const UDancerAgentComponent* Dancer = Agent.Get();
						return FText::FromString(Dancer && Dancer->IsDanceMirrored() ? TEXT("ON") : TEXT("OFF"));
					})
					.Font(Font("Bold", 15))
					.ColorAndOpacity_Lambda([this]()
					{
						const UDancerAgentComponent* Dancer = Agent.Get();
						return Dancer && Dancer->IsDanceMirrored() ? Chosen : Body;
					})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
		[
			// What she is doing now, in words, once it is more than one dance.
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const UDancerAgentComponent* Dancer = Agent.Get();
				if (!Dancer || !Dancer->IsRemix())
				{
					return FText::GetEmpty();
				}
				const int32 Arms = Dancer->GetDanceArmsIndex();
				const int32 Legs = Dancer->GetCurrentDanceIndex();
				return FText::FromString(FString::Printf(TEXT("Arms: %s     Legs: %s"),
					Dancer->Dances.IsValidIndex(Arms) ? *UDancerAgentComponent::DanceDisplayName(Dancer->Dances[Arms].Get()) : TEXT("?"),
					Dancer->Dances.IsValidIndex(Legs) ? *UDancerAgentComponent::DanceDisplayName(Dancer->Dances[Legs].Get()) : TEXT("?")));
			})
			.Font(Font("Regular", 14))
			.ColorAndOpacity(Chosen)
			.AutoWrapText(true)
		];

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
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						Controls
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						List.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "DanceMenuFooter", "[Up]/[Down] try a dance   [Left]/[Right] whole, arms or legs   [-]/[+] speed   [M] mirror   [Enter] on a film plays it   [Enter], [F] or [Esc] done"))
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
	const int32 Films = Dancer ? NumFilms() : 0;
	const int32 Count = Dancer ? Films + Dancer->Dances.Num() : 0;
	const int32 Playing = Dancer && Dancer->GetCurrentDanceIndex() != INDEX_NONE
		? Dancer->GetCurrentDanceIndex() + 1 + Films : INDEX_NONE;
	// In a remix the legs row says "(legs)" and the arms dance gets its own "(arms)".
	const bool bRemix = Dancer && Dancer->IsRemix();
	const int32 ArmsRow = bRemix ? RowOfDance(Dancer->GetDanceArmsIndex()) : INDEX_NONE;
	Selected = FMath::Clamp(Selected, 0, Count);

	for (int32 Row = 0; Row <= Count; ++Row)
	{
		const bool bFilm = Row >= 1 && Row <= Films;
		const FString Label = Row == 0
			? FString(TEXT("Surprise me"))
			: bFilm
				? (Row == 1 ? FString(TEXT("FILM:  make a new one"))
					: FString::Printf(TEXT("FILM:  %s"), *FilmCatalog::All()[Row - 2].Title))
				: UDancerAgentComponent::DanceDisplayName(Dancer->Dances[Row - 1 - Films].Get());
		const bool bPlaying = Row == Playing;
		const bool bArmsRow = Row == ArmsRow;
		const bool bSelected = Row == Selected;
		FString Shown = Label;
		if (bPlaying)
		{
			Shown += bRemix ? TEXT("   (legs)") : TEXT("   (dancing)");
		}
		if (bArmsRow)
		{
			Shown += TEXT("   (arms)");
		}

		TSharedRef<SWidget> RowWidget =
			SNew(SButton)
			.IsFocusable(false)   // the keyboard stays with the menu, so the arrows always work
			.ContentPadding(FMargin(12.0f, 6.0f))
			.ButtonColorAndOpacity(bSelected ? FLinearColor(0.15f, 0.3f, 0.4f, 1.0f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
			.OnClicked_Lambda([this, Row, bFilm]()
			{
				if (bFilm)
				{
					PlayFilm(Row);
				}
				else
				{
					Choose(Row);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(Shown))
				.Font(Font(bSelected ? "Bold" : "Regular", 18))
				.ColorAndOpacity((bPlaying || bArmsRow) ? Chosen : (Row == 0 || bFilm) ? Heading : Body)
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
	if ((Key == EKeys::Enter || Key == EKeys::SpaceBar) && IsFilmRow(Selected))
	{
		if (!InKeyEvent.IsRepeat())
		{
			PlayFilm(Selected);
		}
		return FReply::Handled();
	}
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

	// The remix: which half the rows change, the speed, the mirror.
	if (Key == EKeys::Left || Key == EKeys::A)
	{
		SetPart(Part == EPart::Whole ? EPart::Legs : Part == EPart::Arms ? EPart::Whole : EPart::Arms);
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::D)
	{
		SetPart(Part == EPart::Whole ? EPart::Arms : Part == EPart::Arms ? EPart::Legs : EPart::Whole);
		return FReply::Handled();
	}
	if (Key == EKeys::Hyphen || Key == EKeys::Subtract || Key == EKeys::LeftBracket)
	{
		StepSpeed(-0.25f);
		return FReply::Handled();
	}
	if (Key == EKeys::Equals || Key == EKeys::Add || Key == EKeys::RightBracket)
	{
		StepSpeed(0.25f);
		return FReply::Handled();
	}
	if (Key == EKeys::M)
	{
		if (!InKeyEvent.IsRepeat())
		{
			ToggleMirror();
		}
		return FReply::Handled();
	}

	const UDancerAgentComponent* Dancer = Agent.Get();
	const int32 Count = Dancer ? NumFilms() + Dancer->Dances.Num() : 0;
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
