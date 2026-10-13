// FreakbookWidget.cpp — see the header.

#include "FreakbookWidget.h"

#include "FreakbookSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/NativeWidgetHost.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace FreakbookWidgetDetail
{
	// Linear colours: Slate converts to sRGB, so these read lighter on screen than they look here.
	const FLinearColor Desktop(0.030f, 0.040f, 0.055f, 1.0f);
	const FLinearColor TitleBar(0.16f, 0.17f, 0.19f, 1.0f);
	const FLinearColor Page(0.55f, 0.52f, 0.60f, 1.0f);       // a tired lavender
	const FLinearColor Purple(0.12f, 0.02f, 0.20f, 1.0f);     // the header: not anybody's blue
	const FLinearColor Accent(0.30f, 0.06f, 0.45f, 1.0f);
	const FLinearColor Logo(1.0f, 0.70f, 0.02f, 1.0f);
	const FLinearColor Ink(0.02f, 0.02f, 0.025f, 1.0f);
	const FLinearColor Soft(0.20f, 0.20f, 0.22f, 1.0f);
	const FLinearColor Faint(0.86f, 0.86f, 0.88f, 1.0f);
	const FLinearColor Mine(0.62f, 0.45f, 0.85f, 1.0f);
	const FLinearColor Liked(0.85f, 0.10f, 0.25f, 1.0f);
	const FLinearColor Friend(0.05f, 0.45f, 0.15f, 1.0f);
	const FLinearColor Warn(0.75f, 0.12f, 0.08f, 1.0f);
	const FLinearColor Bank(0.02f, 0.05f, 0.16f, 1.0f);
	const FLinearColor Covers[FreakPeople] = {
		FLinearColor(0.45f, 0.30f, 0.70f), FLinearColor(0.20f, 0.45f, 0.20f), FLinearColor(0.10f, 0.18f, 0.45f),
		FLinearColor(0.55f, 0.35f, 0.15f), FLinearColor(0.75f, 0.40f, 0.05f) };

	constexpr float WindowWidth = 1220.0f;
	constexpr float BodyHeight = 660.0f;
	constexpr float LeftWidth = 240.0f;
	constexpr float RightWidth = 270.0f;
	constexpr float PhotoWidth = 480.0f;

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	const FSlateBrush* Card()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 8.0f);
		return &Brush;
	}

	const FSlateBrush* Rounded()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 10.0f);
		return &Brush;
	}

	FText From(const std::string& Text)
	{
		return FText::FromString(UFreakbookSubsystem::ToFString(Text));
	}

	FText From(const char* Text)
	{
		return FText::FromString(FString(UTF8_TO_TCHAR(Text)));
	}

	// Flat buttons tinted by ButtonColorAndOpacity: the engine's own is dark grey and a tint only
	// darkens it (UNotHumanWidget learned it first).
	const FButtonStyle& Flat()
	{
		static const FButtonStyle Style = []()
		{
			FButtonStyle S = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 6.0f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.90f, 0.90f, 0.92f, 1.0f), 6.0f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.78f, 0.78f, 0.80f, 1.0f), 6.0f));
			S.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.80f, 0.80f, 0.82f, 1.0f), 6.0f));
			S.SetNormalPadding(FMargin(0.0f));
			S.SetPressedPadding(FMargin(0.0f));
			return S;
		}();
		return Style;
	}

	// Rows and names that act as links: no box until the pointer is over them.
	const FButtonStyle& Link()
	{
		static const FButtonStyle Style = []()
		{
			FButtonStyle S = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
			S.SetNormal(FSlateNoResource());
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.08f), 6.0f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.16f), 6.0f));
			S.SetNormalPadding(FMargin(0.0f));
			S.SetPressedPadding(FMargin(0.0f));
			return S;
		}();
		return Style;
	}

	const FEditableTextBoxStyle& Field()
	{
		static const FEditableTextBoxStyle Style = []()
		{
			FEditableTextBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			const FSlateRoundedBoxBrush Plain(FLinearColor(0.95f, 0.95f, 0.96f, 1.0f), 16.0f, FLinearColor(0.70f, 0.71f, 0.74f, 1.0f), 1.0f);
			const FSlateRoundedBoxBrush Focused(FLinearColor::White, 16.0f, Accent, 2.0f);
			S.SetBackgroundImageNormal(Plain);
			S.SetBackgroundImageHovered(Plain);
			S.SetBackgroundImageFocused(Focused);
			S.SetBackgroundImageReadOnly(Plain);
			S.SetForegroundColor(FSlateColor(Ink));
			S.SetFocusedForegroundColor(FSlateColor(Ink));
			S.SetPadding(FMargin(14.0f, 9.0f));
			return S;
		}();
		return Style;
	}

	FString Picture(const char* Name)
	{
		return FString(UTF8_TO_TCHAR(Name));
	}

	FString PersonLine(EFreakPerson Who, const FFreakbookState& S)
	{
		const FFreakProfile& P = FFreakbook::Profile(Who);
		return FString::Printf(TEXT("%s  -  %s"), UTF8_TO_TCHAR(P.Language),
			S.Done[static_cast<int>(Who)] ? TEXT("Friend") : TEXT("Acquaintance"));
	}

	/** Why the profile's button cannot send it yet: where the thing is, as far as anyone knows. */
	const TCHAR* NotYet(EFreakPerson Who)
	{
		switch (Who)
		{
		case EFreakPerson::Dee:     return TEXT("Find her binder first. She left it in somebody's attic.");
		case EFreakPerson::Gunther: return TEXT("Find a real coconut first. Nobody has seen one around here yet.");
		case EFreakPerson::Brent:   return TEXT("Find a championship belt first. Nobody has seen one around here yet.");
		default:                    return TEXT("");
		}
	}

	const TCHAR* Sent(EFreakPerson Who)
	{
		switch (Who)
		{
		case EFreakPerson::Dee:     return TEXT("You mailed Dee her binder. She will post about it.");
		case EFreakPerson::Gunther: return TEXT("You sent Gunther the coconut. It sloshed all the way to Dayton.");
		case EFreakPerson::Brent:   return TEXT("You gave Brent the belt. He is wearing it to the 2 p.m. sync.");
		default:                    return TEXT("");
		}
	}
}

UFreakbookWidget::UFreakbookWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);   // so Esc reaches NativeOnKeyDown when no text field has the keyboard
}

UFreakbookSubsystem* UFreakbookWidget::Book() const
{
	return UFreakbookSubsystem::Get(this);
}

TSharedRef<SWidget> UFreakbookWidget::RebuildWidget()
{
	// Hosted, not returned raw: wrapped by the UUserWidget, the Slate tree still gets this
	// widget's NativeTick and key handling (UNotHumanWidget does the same).
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UNativeWidgetHost* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("FreakbookHost"));
		Host->SetContent(BuildLayout());
		WidgetTree->RootWidget = Host;
	}
	return Super::RebuildWidget();
}

TSharedRef<SWidget> UFreakbookWidget::BuildLayout()
{
	using namespace FreakbookWidgetDetail;
	return SNew(SBorder)
		.BorderImage(White())
		.BorderBackgroundColor(Desktop)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			// A fixed width, no scale box: the engine's DPI curve already scales the UI with the
			// resolution, as for UNotHumanWidget. (Wrapped text lays out one frame late - a page
			// screenshotted the frame it is built shows every long line clipped. It is not a bug.)
			SNew(SBox)
			.WidthOverride(WindowWidth)
			[
				SNew(SVerticalBox)
				// The browser window's title bar.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(White()).BorderBackgroundColor(TitleBar).Padding(FMargin(14.0f, 8.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(STextBlock).Font(Font("Regular", 14)).ColorAndOpacity(FLinearColor(0.85f, 0.86f, 0.88f, 1.0f))
							.Text(FText::FromString(TEXT("freakbook.com  -  Freakbook")))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock).Font(Font("Regular", 14)).ColorAndOpacity(FLinearColor(0.70f, 0.72f, 0.75f, 1.0f))
							.Text(FText::FromString(TEXT("[ESC] log out")))
						]
					]
				]
				// The header.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Purple).Padding(FMargin(20.0f, 10.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Font("Bold", 30)).ColorAndOpacity(Logo).Text(FText::FromString(TEXT("freakbook")))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(40.0f, 0.0f))
						[
							SNew(SBox).WidthOverride(320.0f).HAlign(HAlign_Left)
							.Visibility_Lambda([this]() { return View == EView::Login ? EVisibility::Hidden : EVisibility::Visible; })
							[
								SNew(SBorder).BorderImage(Rounded()).BorderBackgroundColor(FLinearColor(0.30f, 0.20f, 0.38f, 1.0f)).Padding(FMargin(14.0f, 6.0f))
								[
									SNew(STextBlock).Font(Font("Regular", 14)).ColorAndOpacity(FLinearColor(0.80f, 0.76f, 0.86f, 1.0f))
									.Text(FText::FromString(TEXT("Search Freakbook")))
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
						[
							SNew(SBox).Visibility_Lambda([this]() { return View == EView::Login ? EVisibility::Hidden : EVisibility::Visible; })
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									Avatar(TEXT("leonard_avatar"), 34.0f)
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
								[
									SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(FLinearColor::White).Text(FText::FromString(TEXT("Leonard")))
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(Logo).ContentPadding(FMargin(14.0f, 6.0f))
							.OnClicked_Lambda([this]() { Close(); return FReply::Handled(); })
							[
								SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(Purple).Text(FText::FromString(TEXT("Log out")))
							]
						]
					]
				]
				// The page.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(BodyHeight)
					[
						SAssignNew(Body, SBorder).BorderImage(White()).BorderBackgroundColor(Page).Padding(FMargin(14.0f))
					]
				]
			]
		];
}

// =====================================================================================
// THE PAGES

void UFreakbookWidget::Open()
{
	Clock = 0.0f;
	RevealAt = -1.0f;
	Notice.Reset();
	Draft.Reset();
	TakeWidget();   // make sure the layout exists before it is filled in
	Show(EView::Login);
}

#if !UE_BUILD_SHIPPING
void UFreakbookWidget::DebugShow(int32 InView, int32 InPerson)
{
	Show(static_cast<EView>(FMath::Clamp(InView, 0, static_cast<int32>(EView::Bank))),
		static_cast<EFreakPerson>(FMath::Clamp(InPerson, 0, FreakPeople - 1)));
}
#endif

void UFreakbookWidget::Show(EView InView, EFreakPerson InPerson)
{
	if (InView != View || InPerson != Person)
	{
		Notice.Reset();
		Draft.Reset();
	}
	View = InView;
	Person = InPerson;
	if (UFreakbookSubsystem* B = Book())
	{
		if (View == EView::Dashboard && B->Grant(EFreakPerson::Margo))
		{
			Notice = TEXT("You are the first person ever to open this dashboard. Margo will hear about it.");
		}
		if (View == EView::Bank)
		{
			BankBanner = B->State().Done[static_cast<int>(EFreakPerson::Jaxx)] ? 3 : 0;
		}
	}
	Refresh();
}

void UFreakbookWidget::Refresh(bool bKeepScroll)
{
	if (!Body.IsValid())
	{
		return;
	}
	const float Offset = bKeepScroll && Scroll.IsValid() ? Scroll->GetScrollOffset() : 0.0f;
	Scroll.Reset();
	Field.Reset();
	if (View == EView::Login)
	{
		Body->SetContent(BuildLogin());
	}
	else
	{
		TSharedRef<SWidget> Centre = SNullWidget::NullWidget;
		switch (View)
		{
		case EView::Profile:   Centre = BuildProfile(); break;
		case EView::Thread:    Centre = BuildThread(); break;
		case EView::Dashboard: Centre = BuildDashboard(); break;
		case EView::Bank:      Centre = BuildBank(); break;
		default:               Centre = BuildFeed(); break;
		}
		Body->SetContent(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(FreakbookWidgetDetail::LeftWidth)[ BuildLeft() ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(14.0f, 0.0f))
			[
				Centre
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(FreakbookWidgetDetail::RightWidth)[ BuildRight() ]
			]);
	}
	if (Scroll.IsValid())
	{
		if (View == EView::Thread)
		{
			Scroll->ScrollToEnd();
		}
		else if (bKeepScroll)
		{
			Scroll->SetScrollOffset(Offset);
		}
	}
	if (FSlateApplication::IsInitialized())
	{
		if (View == EView::Thread && Field.IsValid())
		{
			FSlateApplication::Get().SetKeyboardFocus(Field, EFocusCause::SetDirectly);
		}
		else
		{
			SetKeyboardFocus();
		}
	}
}

TSharedRef<SWidget> UFreakbookWidget::BuildNotice()
{
	using namespace FreakbookWidgetDetail;
	if (Notice.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}
	return SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor(1.0f, 0.93f, 0.55f, 1.0f)).Padding(FMargin(14.0f, 10.0f))
		[
			SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(Ink).AutoWrapText(true).Text(FText::FromString(Notice))
		];
}

TSharedRef<SWidget> UFreakbookWidget::BuildLogin()
{
	using namespace FreakbookWidgetDetail;
	const UFreakbookSubsystem* B = Book();
	const int32 Machine = B ? B->State().MachinePercent : -1;
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(560.0f)
			[
				SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(34.0f, 30.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font("Bold", 46)).ColorAndOpacity(Accent).Text(FText::FromString(TEXT("freakbook")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 18.0f))   // fill, centred by justification: a wrapping line in a centred slot clips
					[
						SNew(STextBlock).Font(Font("Regular", 16)).ColorAndOpacity(Soft).AutoWrapText(true).Justification(ETextJustify::Center)
						.Text(FText::FromString(TEXT("Where five programmers say what they can't say at work.")))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 0.0f, 0.0f, 18.0f))
					[
						SNew(STextBlock).Font(Font("Italic", 16)).ColorAndOpacity(Accent)
						.Visibility(Machine >= 0 ? EVisibility::Visible : EVisibility::Collapsed)
						.Text(FText::FromString(FString::Printf(TEXT("notHUMAN says you are %d%% machine. Close enough."), Machine)))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(Accent).ContentPadding(FMargin(36.0f, 12.0f))
						.OnClicked_Lambda([this]()
						{
							if (UFreakbookSubsystem* Sub = Book())
							{
								Sub->Login();
							}
							Show(EView::Feed);
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Font(Font("Bold", 20)).ColorAndOpacity(FLinearColor::White).Text(FText::FromString(TEXT("Log in as Leonard")))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 14.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Soft)
						.Text(FText::FromString(TEXT("No password needed. Nobody here can remember theirs either.")))
					]
				]
			]
		];
}

TSharedRef<SWidget> UFreakbookWidget::BuildLeft()
{
	using namespace FreakbookWidgetDetail;
	const UFreakbookSubsystem* B = Book();
	const FFreakbookState Empty;
	const FFreakbookState& S = B ? B->State() : Empty;
	const int32 Friends = FFreakbook::Friends(S);

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
	[
		SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(12.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ Avatar(TEXT("leonard_avatar"), 52.0f) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("Leonard Sibelius")))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Soft).AutoWrapText(true)
						.Text(FText::FromString(TEXT("71. Programmer, forty years.")))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(Friends > 0 ? Friend : Soft)
				.Text(FText::FromString(FString::Printf(TEXT("Friends: %d of 5"), Friends)))
			]
		]
	];
	List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
	[
		SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(View == EView::Feed ? Logo : FLinearColor::White).ContentPadding(FMargin(12.0f, 8.0f))
		.OnClicked_Lambda([this]() { Show(EView::Feed); return FReply::Handled(); })
		[
			SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(Purple).Text(FText::FromString(TEXT("Feed")))
		]
	];
	List->AddSlot().AutoHeight().Padding(FMargin(4.0f, 8.0f, 0.0f, 4.0f))
	[
		SNew(STextBlock).Font(Font("Bold", 12)).ColorAndOpacity(FLinearColor::White)
		.Text(FText::FromString(Friends == 5 ? TEXT("YOUR FRIENDS") : Friends > 0 ? TEXT("FRIENDS AND ACQUAINTANCES") : TEXT("YOUR ACQUAINTANCES")))
	];
	for (int32 I = 0; I < FreakPeople; ++I)
	{
		const EFreakPerson Who = static_cast<EFreakPerson>(I);
		const FFreakProfile& P = FFreakbook::Profile(Who);
		const bool bHere = (View == EView::Profile || View == EView::Thread) && Person == Who;
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 2.0f))
		[
			SNew(SButton).ButtonStyle(&Link()).ContentPadding(FMargin(6.0f, 4.0f))
			.OnClicked_Lambda([this, Who]() { Show(EView::Profile, Who); return FReply::Handled(); })
			[
				SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(bHere ? FLinearColor(1.0f, 1.0f, 1.0f, 0.9f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.0f)).Padding(FMargin(6.0f, 4.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Avatar(Picture(P.Avatar), 38.0f) ]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(bHere ? Ink : FLinearColor::White).Text(From(P.Short))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(bHere ? Soft : FLinearColor(0.92f, 0.90f, 0.95f, 1.0f))
							.AutoWrapText(true).Text(FText::FromString(PersonLine(Who, S)))
						]
					]
				]
			]
		];
	}
	return List;
}

TSharedRef<SWidget> UFreakbookWidget::BuildRight()
{
	using namespace FreakbookWidgetDetail;
	TSharedRef<SVerticalBox> Known = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
		[
			SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("People you may know")))
		];
	for (const std::pair<std::string, std::string>& Row : FFreakbook::PeopleYouMayKnow())
	{
		Known->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ Avatar(TEXT("leonard_avatar"), 34.0f) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Bold", 13)).ColorAndOpacity(Ink).Text(From(Row.first))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Soft).AutoWrapText(true).Text(From(Row.second))
				]
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(12.0f))[ Known ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
		[
			SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(12.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Regular", 11)).ColorAndOpacity(Soft).Text(FText::FromString(TEXT("Sponsored")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
				[
					Photo(TEXT("dee_freezer"), RightWidth - 24.0f)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Bold", 13)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("CRYO-RETIRE 3000")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Soft).AutoWrapText(true)
					.Text(FText::FromString(TEXT("Stay fresh until the batch fails. Thaw fees apply.")))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.0f, 10.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock).Font(Font("Regular", 11)).ColorAndOpacity(FLinearColor(0.95f, 0.93f, 0.97f, 1.0f)).AutoWrapText(true)
			.Text(FText::FromString(TEXT("Freakbook 2026  -  Privacy  -  Terms  -  Feelings")))
		];
}

TSharedRef<SWidget> UFreakbookWidget::BuildPost(const FFreakPost& Post)
{
	using namespace FreakbookWidgetDetail;
	const UFreakbookSubsystem* B = Book();
	const bool bLiked = B && FFreakbook::IsLiked(B->State(), Post.Id);
	const FFreakProfile& P = FFreakbook::Profile(Post.Who);
	const EFreakPerson Who = Post.Who;
	const FString Id = UFreakbookSubsystem::ToFString(Post.Id);

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(&Link()).ContentPadding(FMargin(0.0f))
				.OnClicked_Lambda([this, Who]() { Show(EView::Profile, Who); return FReply::Handled(); })
				[
					Avatar(Picture(P.Avatar), 44.0f)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Bold", 16)).ColorAndOpacity(Ink).Text(From(P.Name))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Soft)
					.Text(FText::FromString(FString::Printf(TEXT("%s  -  %s"), UTF8_TO_TCHAR(P.Language), *UFreakbookSubsystem::ToFString(Post.When))))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock).Font(Font("Regular", 16)).ColorAndOpacity(Ink).AutoWrapText(true).Text(From(Post.Text))
		];
	if (!Post.Picture.empty())
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
		[
			Photo(UFreakbookSubsystem::ToFString(Post.Picture), PhotoWidth)
		];
	}
	Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(bLiked ? Liked : Faint).ContentPadding(FMargin(14.0f, 5.0f))
			.OnClicked_Lambda([this, Id]()
			{
				if (UFreakbookSubsystem* Sub = Book())
				{
					Sub->ToggleLike(Id);
				}
				Refresh(true);
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(Font("Bold", 13)).ColorAndOpacity(bLiked ? FLinearColor::White : Ink)
				.Text(FText::FromString(FString::Printf(TEXT("%s  %d"), bLiked ? TEXT("Liked") : TEXT("Like"), Post.Likes + (bLiked ? 1 : 0))))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SSpacer)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(Font("Regular", 12)).ColorAndOpacity(Soft)
			.Visibility(Post.Comments.empty() ? EVisibility::Collapsed : EVisibility::Visible)
			.Text(FText::FromString(FString::Printf(TEXT("%d comment%s"), static_cast<int32>(Post.Comments.size()), Post.Comments.size() == 1 ? TEXT("") : TEXT("s"))))
		]
	];
	for (const FFreakComment& C : Post.Comments)
	{
		const FFreakProfile& By = FFreakbook::Profile(C.Who);
		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ Avatar(Picture(By.Avatar), 28.0f) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(Faint).Padding(FMargin(10.0f, 6.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Bold", 13)).ColorAndOpacity(Ink).Text(From(By.Short))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Regular", 13)).ColorAndOpacity(Ink).AutoWrapText(true).Text(From(C.Text))
					]
				]
			]
		];
	}
	return SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(16.0f, 14.0f))[ Box ];
}

TSharedRef<SWidget> UFreakbookWidget::BuildFeed()
{
	TSharedRef<SScrollBox> List = SAssignNew(Scroll, SScrollBox);
	List->AddSlot().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))[ BuildNotice() ];
	if (const UFreakbookSubsystem* B = Book())
	{
		for (const FFreakPost& Post : FFreakbook::Feed(B->State()))
		{
			List->AddSlot().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))[ BuildPost(Post) ];
		}
	}
	return List;
}

TSharedRef<SWidget> UFreakbookWidget::BuildProfile()
{
	using namespace FreakbookWidgetDetail;
	const UFreakbookSubsystem* B = Book();
	const FFreakbookState Empty;
	const FFreakbookState& S = B ? B->State() : Empty;
	const FFreakProfile& P = FFreakbook::Profile(Person);
	const int32 I = static_cast<int32>(Person);
	const bool bDone = S.Done[I];
	const bool bNeedsItem = P.Item[0] != '\0';
	const bool bCan = FFreakbook::CanGrant(S, Person);
	const EFreakPerson Who = Person;

	auto Fact = [](const TCHAR* Label, const FText& Text, const FLinearColor& Colour)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(96.0f)
				[
					SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(Soft).Text(FText::FromString(Label))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(STextBlock).Font(Font("Regular", 15)).ColorAndOpacity(Colour).AutoWrapText(true).Text(Text)
			];
	};

	TSharedRef<SScrollBox> List = SAssignNew(Scroll, SScrollBox);
	List->AddSlot().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))[ BuildNotice() ];
	List->AddSlot()
	[
		SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(0.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(70.0f)
				[
					SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(Covers[I])
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 12.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ Avatar(Picture(P.Avatar), 110.0f) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Bold", 24)).ColorAndOpacity(Ink).Text(From(P.Name))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Regular", 15)).ColorAndOpacity(Soft)
						.Text(FText::FromString(FString::Printf(TEXT("%d  -  writes %s"), P.Age, UTF8_TO_TCHAR(P.Language))))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(bDone ? Friend : Soft)
						.Text(FText::FromString(bDone ? TEXT("Friend") : TEXT("Acquaintance")))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 0.0f, 18.0f, 6.0f))[ Fact(TEXT("Works"), From(P.Job), Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 6.0f))[ Fact(TEXT("Hates"), From(P.Hates), Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 6.0f))[ Fact(TEXT("Dreams of"), From(P.Dreams), Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 6.0f))[ Fact(bDone ? TEXT("Has") : TEXT("Wants"), From(bDone ? P.Thanks : P.Wants), bDone ? Friend : Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 10.0f, 18.0f, 16.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(Accent).ContentPadding(FMargin(18.0f, 8.0f))
					.OnClicked_Lambda([this, Who]() { Show(EView::Thread, Who); return FReply::Handled(); })
					[
						SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(FLinearColor::White)
						.Text(FText::FromString(FString::Printf(TEXT("Message %s"), UTF8_TO_TCHAR(P.Short))))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(Logo).ContentPadding(FMargin(18.0f, 8.0f))
					.Visibility(bDone && bNeedsItem ? EVisibility::Collapsed : EVisibility::Visible)
					.IsEnabled(!bNeedsItem || bCan)
					.OnClicked_Lambda([this, Who]()
					{
						if (Who == EFreakPerson::Margo)
						{
							Show(EView::Dashboard, Who);
						}
						else if (Who == EFreakPerson::Jaxx)
						{
							Show(EView::Bank, Who);
						}
						else if (UFreakbookSubsystem* Sub = Book())
						{
							if (Sub->Grant(Who))
							{
								Notice = Sent(Who);
								Refresh();
							}
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(Purple).Text(From(P.Action))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock).Font(Font("Italic", 13)).ColorAndOpacity(Soft).AutoWrapText(true)
					.Visibility(bNeedsItem && !bDone && !bCan ? EVisibility::Visible : EVisibility::Collapsed)
					.Text(FText::FromString(NotYet(Who)))
				]
			]
		]
	];
	for (const FFreakPost& Post : FFreakbook::PostsBy(S, Person))
	{
		List->AddSlot().Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))[ BuildPost(Post) ];
	}
	return List;
}

TSharedRef<SWidget> UFreakbookWidget::BuildThread()
{
	using namespace FreakbookWidgetDetail;
	const UFreakbookSubsystem* B = Book();
	const FFreakbookState Empty;
	const FFreakbookState& S = B ? B->State() : Empty;
	const FFreakProfile& P = FFreakbook::Profile(Person);
	const std::vector<FFreakMessage>& Thread = S.Threads[static_cast<int32>(Person)];
	const bool bTyping = RevealAt > 0.0f && !Thread.empty() && !Thread.back().bFromLeonard;
	const EFreakPerson Who = Person;

	TSharedRef<SScrollBox> Messages = SAssignNew(Scroll, SScrollBox);
	if (Thread.empty())
	{
		Messages->AddSlot().Padding(FMargin(8.0f, 20.0f))
		[
			SNew(STextBlock).Font(Font("Italic", 15)).ColorAndOpacity(Soft).AutoWrapText(true)
			.Text(FText::FromString(FString::Printf(TEXT("Say something. %s doesn't get many messages."), UTF8_TO_TCHAR(P.Short))))
		];
	}
	const size_t Shown = bTyping ? Thread.size() - 1 : Thread.size();
	for (size_t K = 0; K < Shown; ++K)
	{
		const FFreakMessage& M = Thread[K];
		if (M.bFromLeonard)
		{
			Messages->AddSlot().HAlign(HAlign_Right).Padding(FMargin(80.0f, 4.0f, 4.0f, 4.0f))
			[
				SNew(SBorder).BorderImage(Rounded()).BorderBackgroundColor(Mine).Padding(FMargin(14.0f, 9.0f))
				[
					SNew(STextBlock).Font(Font("Regular", 15)).ColorAndOpacity(Ink).AutoWrapText(true).Text(From(M.Text))
				]
			];
			continue;
		}
		TSharedRef<SVerticalBox> Bubble = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font("Regular", 15)).ColorAndOpacity(Ink).AutoWrapText(true).Text(From(M.Text))
			];
		if (!M.Picture.empty())
		{
			Bubble->AddSlot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
			[
				Photo(UFreakbookSubsystem::ToFString(M.Picture), 300.0f)
			];
		}
		Messages->AddSlot().HAlign(HAlign_Left).Padding(FMargin(4.0f, 4.0f, 80.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ Avatar(Picture(P.Avatar), 32.0f) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBorder).BorderImage(Rounded()).BorderBackgroundColor(Faint).Padding(FMargin(14.0f, 9.0f))[ Bubble ]
			]
		];
	}
	if (bTyping)
	{
		Messages->AddSlot().Padding(FMargin(46.0f, 6.0f))
		[
			SNew(STextBlock).Font(Font("Italic", 14)).ColorAndOpacity(Soft)
			.Text_Lambda([this, Short = FString(UTF8_TO_TCHAR(P.Short))]()
			{
				const int32 Dots = 1 + static_cast<int32>(Clock * 3.0f) % 3;
				return FText::FromString(Short + TEXT(" is typing") + FString::ChrN(Dots, TEXT('.')));
			})
		];
	}

	return SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(14.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Avatar(Picture(P.Avatar), 40.0f) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f))
				[
					SNew(STextBlock).Font(Font("Bold", 18)).ColorAndOpacity(Ink)
					.Text(FText::FromString(FString::Printf(TEXT("Messages with %s"), UTF8_TO_TCHAR(P.Short))))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton).ButtonStyle(&Link()).ContentPadding(FMargin(10.0f, 4.0f))
					.OnClicked_Lambda([this, Who]() { Show(EView::Profile, Who); return FReply::Handled(); })
					[
						SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(Accent)
						.Text(FText::FromString(FString::Printf(TEXT("%s's profile"), UTF8_TO_TCHAR(P.Short))))
					]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				Messages
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(Field, SEditableTextBox)
					.Style(&FreakbookWidgetDetail::Field())
					.Font(Font("Regular", 15))
					.Text(FText::FromString(Draft))
					.HintText(FText::FromString(FString::Printf(TEXT("Write to %s..."), UTF8_TO_TCHAR(P.Short))))
					.OnTextChanged_Lambda([this](const FText& Text) { Draft = Text.ToString(); })
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How)
					{
						if (How == ETextCommit::OnEnter)
						{
							Draft = Text.ToString();
							SendDraft();
						}
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(Accent).ContentPadding(FMargin(20.0f, 8.0f))
					.IsEnabled_Lambda([this]() { return RevealAt < 0.0f; })
					.OnClicked_Lambda([this]() { SendDraft(); return FReply::Handled(); })
					[
						SNew(STextBlock).Font(Font("Bold", 15)).ColorAndOpacity(FLinearColor::White).Text(FText::FromString(TEXT("Send")))
					]
				]
			]
		];
}

TSharedRef<SWidget> UFreakbookWidget::BuildDashboard()
{
	using namespace FreakbookWidgetDetail;
	const UFreakbookSubsystem* B = Book();
	const int32 Views = B && B->State().Done[static_cast<int32>(EFreakPerson::Margo)] ? 1 : 0;
	TSharedRef<SScrollBox> List = SAssignNew(Scroll, SScrollBox);
	List->AddSlot().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))[ BuildNotice() ];
	List->AddSlot()
	[
		SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(18.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font("Bold", 22)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("Kibble Sales - Weekly")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 10.0f))
			[
				SNew(STextBlock).Font(Font("Regular", 13)).ColorAndOpacity(Soft)
				.Text(FText::FromString(TEXT("Kibblesworth Pet Nutrition  -  internal  -  built by M. Tuttle")))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ Photo(TEXT("margo_dashboard_page"), PhotoWidth) ]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 24.0f, 26.0f))
				[
					SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor(1.0f, 0.90f, 0.90f, 1.0f)).Padding(FMargin(18.0f, 10.0f))
					[
						SNew(STextBlock).Font(Font("Bold", 30)).ColorAndOpacity(Warn)
						.Text(FText::FromString(FString::Printf(TEXT("VIEWS: %d"), Views)))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
			[
				SNew(SButton).ButtonStyle(&Link()).ContentPadding(FMargin(6.0f, 4.0f))
				.OnClicked_Lambda([this]() { Show(EView::Profile, EFreakPerson::Margo); return FReply::Handled(); })
				[
					SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(Accent).Text(FText::FromString(TEXT("Back to Margo's profile")))
				]
			]
		]
	];
	return List;
}

TSharedRef<SWidget> UFreakbookWidget::BuildBank()
{
	using namespace FreakbookWidgetDetail;
	auto Choice = [this](const TCHAR* Label, const FLinearColor& Colour, const FLinearColor& TextColour, int32 Next)
	{
		return SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(Colour).ContentPadding(FMargin(16.0f, 7.0f))
			.OnClicked_Lambda([this, Next]()
			{
				if (Next == 3)
				{
					if (UFreakbookSubsystem* Sub = Book())
					{
						Sub->Grant(EFreakPerson::Jaxx);
					}
				}
				BankBanner = Next;
				Refresh();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(TextColour).Text(FText::FromString(Label))
			];
	};

	TSharedRef<SWidget> Banner = SNullWidget::NullWidget;
	if (BankBanner == 0 || BankBanner == 1)
	{
		const bool bSettings = BankBanner == 1;
		Banner = SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(22.0f, 18.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font("Bold", bSettings ? 22 : 30)).ColorAndOpacity(Ink)
					.Text(FText::FromString(bSettings ? TEXT("Cookie settings") : TEXT("WE USE COOKIES")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 14.0f))
				[
					SNew(STextBlock).Font(Font("Regular", 15)).ColorAndOpacity(Soft).AutoWrapText(true)
					.Text(FText::FromString(bSettings
						? TEXT("Strictly necessary cookies: 1,400. Optional cookies: also 1,400. None of them can be turned off. This is the second banner.")
						: TEXT("and 1,400 packages. By continuing you agree to all of them, and to whatever they agree to.")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[ Choice(bSettings ? TEXT("Accept all") : TEXT("Accept"), FLinearColor(0.05f, 0.45f, 0.15f, 1.0f), FLinearColor::White, 3) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
					[
						bSettings ? SNullWidget::NullWidget : StaticCastSharedRef<SWidget>(Choice(TEXT("Settings"), Faint, Ink, 1))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SSpacer) ]
					+ SHorizontalBox::Slot().AutoWidth()[ Choice(TEXT("X"), Faint, Ink, 2) ]
				]
			];
	}
	else if (BankBanner == 2)
	{
		Banner = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font("Italic", 15)).ColorAndOpacity(Soft).AutoWrapText(true)
				.Text(FText::FromString(TEXT("You clicked the X. Everyone clicks the X.")))
			]
			+ SHorizontalBox::Slot().AutoWidth()[ Choice(TEXT("Show the banner again"), Faint, Ink, 0) ];
	}
	else
	{
		Banner = SNew(STextBlock).Font(Font("Bold", 16)).ColorAndOpacity(Friend).AutoWrapText(true)
			.Text(FText::FromString(TEXT("Cookies accepted. Somewhere in this building, a young man is weeping with joy.")));
	}

	TSharedRef<SScrollBox> List = SAssignNew(Scroll, SScrollBox);
	List->AddSlot()
	[
		SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(FLinearColor::White).Padding(FMargin(0.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(Bank).Padding(FMargin(18.0f, 14.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Bold", 22)).ColorAndOpacity(FLinearColor::White).Text(FText::FromString(TEXT("First Second National Bank")))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font("Regular", 13)).ColorAndOpacity(FLinearColor(0.75f, 0.80f, 0.90f, 1.0f))
						.Text(FText::FromString(TEXT("Banking the old way since 1983. Bannering the new way since last week.")))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 16.0f))
			[
				SNew(STextBlock).Font(Font("Regular", 15)).ColorAndOpacity(Ink).AutoWrapText(true)
				.Text(FText::FromString(TEXT("Welcome to online banking. Your statement is ready. It was always ready: a woman named Dee sees to that.")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(18.0f, 0.0f, 18.0f, 18.0f))
			[
				SNew(SBorder).BorderImage(Card()).BorderBackgroundColor(Faint).Padding(FMargin(6.0f))[ Banner ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(12.0f, 0.0f, 12.0f, 12.0f))
			[
				SNew(SButton).ButtonStyle(&Link()).ContentPadding(FMargin(6.0f, 4.0f))
				.OnClicked_Lambda([this]() { Show(EView::Profile, EFreakPerson::Jaxx); return FReply::Handled(); })
				[
					SNew(STextBlock).Font(Font("Bold", 14)).ColorAndOpacity(Accent).Text(FText::FromString(TEXT("Back to Jaxx's profile")))
				]
			]
		]
	];
	return List;
}

// =====================================================================================
// PICTURES

const FSlateBrush* UFreakbookWidget::PictureBrush(const FString& Picture)
{
	if (const TSharedPtr<FSlateBrush>* Found = Brushes.Find(Picture))
	{
		return Found->Get();
	}
	TSharedPtr<FSlateBrush> Made = MakeShared<FSlateBrush>();
	const FString Path = FString::Printf(TEXT("/Game/Freakbook/T_FB_%s.T_FB_%s"), *Picture, *Picture);
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path))
	{
		Textures.Add(Texture);
		Made->SetResourceObject(Texture);
		Made->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Made->DrawAs = ESlateBrushDrawType::Image;
	}
	else
	{
		// Not imported yet (make_freakbook_assets.py): a plain grey card, never a crash.
		*Made = *FreakbookWidgetDetail::White();
		Made->TintColor = FSlateColor(FLinearColor(0.75f, 0.75f, 0.78f, 1.0f));
	}
	Brushes.Add(Picture, Made);
	return Made.Get();
}

TSharedRef<SWidget> UFreakbookWidget::Avatar(const FString& Picture, float Size)
{
	return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
		[
			SNew(SImage).Image(PictureBrush(Picture))
		];
}

TSharedRef<SWidget> UFreakbookWidget::Photo(const FString& Picture, float Width)
{
	return SNew(SBox).WidthOverride(Width).HeightOverride(Width * 0.75f)
		[
			SNew(SImage).Image(PictureBrush(Picture))
		];
}

// =====================================================================================
// SENDING, THE CLOCK AND THE KEYS

void UFreakbookWidget::SendDraft()
{
	UFreakbookSubsystem* B = Book();
	if (!B || View != EView::Thread || RevealAt > 0.0f)
	{
		return;   // one at a time: they are still typing
	}
	const FString Message = Draft.TrimStartAndEnd();
	if (Message.IsEmpty())
	{
		return;
	}
	const FFreakMessage Reply = B->Send(Person, Message);
	Draft.Reset();
	if (!Reply.Text.empty())
	{
		// Long enough to feel like someone read it; short enough not to be a wait.
		RevealAt = Clock + FMath::Clamp(0.9f + 0.012f * static_cast<float>(Reply.Text.size()), 1.0f, 2.6f);
	}
	Refresh();
}

void UFreakbookWidget::Close()
{
	RevealAt = -1.0f;
	OnClosed.ExecuteIfBound();
}

void UFreakbookWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	if (RevealAt > 0.0f && Clock >= RevealAt)
	{
		RevealAt = -1.0f;
		Refresh();
	}
}

FReply UFreakbookWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
