// NotHumanWidget.cpp — see the header.

#include "NotHumanWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/IConsoleManager.h"
#include "Components/NativeWidgetHost.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace NotHumanWidgetDetail
{
	const FLinearColor Desktop(0.030f, 0.040f, 0.055f, 1.0f);
	const FLinearColor TitleBar(0.16f, 0.17f, 0.19f, 1.0f);
	const FLinearColor Card(0.97f, 0.97f, 0.97f, 1.0f);
	const FLinearColor Header(0.10f, 0.42f, 0.86f, 1.0f);
	const FLinearColor Ink(0.10f, 0.10f, 0.12f, 1.0f);
	const FLinearColor Soft(0.42f, 0.43f, 0.46f, 1.0f);
	const FLinearColor Tile(0.86f, 0.88f, 0.91f, 1.0f);
	const FLinearColor TileOn(0.18f, 0.52f, 0.95f, 1.0f);
	const FLinearColor Pass(0.08f, 0.55f, 0.24f, 1.0f);
	const FLinearColor Fail(0.78f, 0.14f, 0.10f, 1.0f);
	const FLinearColor VisionGreen(0.25f, 1.0f, 0.45f, 1.0f);
	const FLinearColor VisionStrip(0.02f, 0.06f, 0.03f, 1.0f);
	const FLinearColor Waiting(0.55f, 0.56f, 0.58f, 1.0f);
	const FLinearColor Go(0.10f, 0.80f, 0.25f, 1.0f);

	constexpr float BodyWidth = 760.0f;
	constexpr float BodyHeight = 380.0f;
	constexpr float BoxSize = 46.0f;

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FText Utf8(const std::string& Text)
	{
		return FText::FromString(FString(UTF8_TO_TCHAR(Text.c_str())));
	}

	// Flat white buttons, tinted by ButtonColorAndOpacity. The engine's own button is dark grey,
	// and a tint only darkens it: the first build's checkbox came out charcoal, not white.
	const FButtonStyle& Flat()
	{
		static const FButtonStyle Style = []()
		{
			FButtonStyle S = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 6.0f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.90f, 0.90f, 0.92f, 1.0f), 6.0f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.78f, 0.78f, 0.80f, 1.0f), 6.0f));
			S.SetNormalPadding(FMargin(0.0f));
			S.SetPressedPadding(FMargin(0.0f));
			return S;
		}();
		return Style;
	}

	// The CAPTCHA box itself: white, with a grey edge.
	const FButtonStyle& Box()
	{
		static const FButtonStyle Style = []()
		{
			FButtonStyle S = Flat();
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 3.0f, FLinearColor(0.62f, 0.62f, 0.64f, 1.0f), 2.0f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor::White, 3.0f, FLinearColor(0.35f, 0.35f, 0.38f, 1.0f), 2.0f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f), 3.0f, FLinearColor(0.35f, 0.35f, 0.38f, 1.0f), 2.0f));
			return S;
		}();
		return Style;
	}

	// A light text field on the white card: the engine's own is black, with grey type.
	const FEditableTextBoxStyle& Field()
	{
		static const FEditableTextBoxStyle Style = []()
		{
			FEditableTextBoxStyle S = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			const FSlateRoundedBoxBrush Plain(FLinearColor(0.95f, 0.95f, 0.96f, 1.0f), 4.0f, FLinearColor(0.70f, 0.71f, 0.74f, 1.0f), 1.0f);
			const FSlateRoundedBoxBrush Focused(FLinearColor::White, 4.0f, FLinearColor(0.10f, 0.42f, 0.86f, 1.0f), 2.0f);
			S.SetBackgroundImageNormal(Plain);
			S.SetBackgroundImageHovered(Plain);
			S.SetBackgroundImageFocused(Focused);
			S.SetBackgroundImageReadOnly(Plain);
			S.SetForegroundColor(FSlateColor(Ink));
			S.SetFocusedForegroundColor(FSlateColor(Ink));
			S.SetPadding(FMargin(12.0f, 8.0f));
			return S;
		}();
		return Style;
	}

#if !UE_BUILD_SHIPPING
	// The open screen, for the dev command below.
	TWeakObjectPtr<UNotHumanWidget> OpenScreen;

	FAutoConsoleCommand ShowCommand(
		TEXT("sib.NotHuman.Show"),
		TEXT("With the laptop open, jump to test N (0-13): to look at each one without playing a whole run."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (UNotHumanWidget* Screen = OpenScreen.Get())
			{
				Screen->DebugShowTest(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
			}
		}));
#endif
}

UNotHumanWidget::UNotHumanWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);   // so its own key handler hears keys when no text field has them
}

TSharedRef<SWidget> UNotHumanWidget::RebuildWidget()
{
	// Hosted, not returned raw: wrapped by the UUserWidget, the Slate tree still gets this
	// widget's NativeTick and key handling.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UNativeWidgetHost* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("NotHumanHost"));
		Host->SetContent(BuildLayout());
		WidgetTree->RootWidget = Host;
	}
	return Super::RebuildWidget();
}

TSharedRef<SWidget> UNotHumanWidget::BuildLayout()
{
	using namespace NotHumanWidgetDetail;
	return SNew(SBorder)
		.BorderImage(White())
		.BorderBackgroundColor(Desktop)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(BodyWidth + 40.0f)
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
							.Text(FText::FromString(TEXT("notHUMAN  -  Prove you're not human")))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock).Font(Font("Regular", 14)).ColorAndOpacity(FLinearColor(0.70f, 0.72f, 0.75f, 1.0f))
							.Text(FText::FromString(TEXT("[ESC] close   hold [V] Code Vision")))
						]
					]
				]
				// The card.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Card).Padding(FMargin(20.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Header).Padding(FMargin(18.0f, 14.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SAssignNew(TitleText, STextBlock).Font(Font("Bold", 26)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
								[
									SAssignNew(DetailText, STextBlock).Font(Font("Regular", 17)).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f)).AutoWrapText(true)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 12.0f))
						[
							SAssignNew(BodyBox, SBox).WidthOverride(BodyWidth).HeightOverride(BodyHeight)
							[
								SAssignNew(BodyHolder, SBorder).BorderImage(White()).BorderBackgroundColor(Card).Padding(0.0f)
							]
						]
						// CODE VISION: the machine's view, while [V] is held.
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBorder).BorderImage(White()).BorderBackgroundColor(VisionStrip).Padding(FMargin(12.0f, 8.0f))
							.Visibility_Lambda([this]() { return bVHeld && (Stage == EStage::Test || Stage == EStage::Judged) ? EVisibility::Visible : EVisibility::Collapsed; })
							[
								SNew(STextBlock).Font(Font("Mono", 17)).ColorAndOpacity(VisionGreen).AutoWrapText(true)
								.Text_Lambda([this]()
								{
									if (!bCanSee)
									{
										return FText::FromString(TEXT("CODE VISION: not yours yet. Kaia gives it to you upstairs."));
									}
									const FNotHumanChallenge& C = Stage == EStage::Judged ? Judging : Test();
									return FText::FromString(TEXT("CODE VISION:  ") + FString(UTF8_TO_TCHAR(C.Vision.c_str())));
								})
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
						[
							SAssignNew(ResultText, STextBlock).Font(Font("Bold", 19)).AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SAssignNew(ProgressText, STextBlock).Font(Font("Regular", 16)).ColorAndOpacity(Soft)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SAssignNew(ActionButton, SButton)
								.ButtonStyle(&Flat())
								.ButtonColorAndOpacity(Header)
								.ContentPadding(FMargin(26.0f, 10.0f))
								.OnClicked_Lambda([this]() { OnAction(); return FReply::Handled(); })
								[
									SAssignNew(ActionLabel, STextBlock).Font(Font("Bold", 18)).ColorAndOpacity(FLinearColor::White)
								]
							]
						]
					]
				]
			]
		];
}

// =====================================================================================
// THE STAGES

void UNotHumanWidget::Open(bool bHasCodeVision)
{
#if !UE_BUILD_SHIPPING
	NotHumanWidgetDetail::OpenScreen = this;
#endif
	bCanSee = bHasCodeVision;
	bVHeld = false;
	TakeWidget();   // make sure the layout exists before it is filled in
	ShowIntro();
}

void UNotHumanWidget::DebugShowTest(int32 Kind)
{
	Run.StartSingle(static_cast<ENotHumanKind>(FMath::Clamp(Kind, 0, static_cast<int32>(ENotHumanKind::Count) - 1)),
		static_cast<uint32>(FMath::Rand()) | 1u);
	History.Reset();
	ShowTest();
}

void UNotHumanWidget::ShowIntro()
{
	Stage = EStage::Intro;
	StageTime = 0.0f;
	History.Reset();
	LastVerdict = FNotHumanVerdict();
	if (BodyHolder)
	{
		BodyHolder->SetContent(BuildIntroBody());
	}
	RefreshChrome();
	FocusForStage();
}

void UNotHumanWidget::ShowTest()
{
	Stage = EStage::Test;
	StageTime = 0.0f;
	const FNotHumanChallenge& C = Test();
	TileSelected.Init(false, static_cast<int32>(C.Options.size()));
	Typed.Reset();
	SliderValue = 50.0f;
	bMoved = false;
	LastCursor = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCursorPos() : FVector2D::ZeroVector;
	BoxPos = FVector2D(NotHumanWidgetDetail::BodyWidth * 0.5 - 23.0, NotHumanWidgetDetail::BodyHeight * 0.4);
	if (BodyHolder)
	{
		BodyHolder->SetContent(BuildTestBody());
	}
	RefreshChrome();
	FocusForStage();
}

void UNotHumanWidget::ShowVerdict()
{
	Stage = EStage::Verdict;
	StageTime = 0.0f;
	if (BodyHolder)
	{
		BodyHolder->SetContent(BuildVerdictBody());
	}
	RefreshChrome();
	FocusForStage();
}

void UNotHumanWidget::RefreshChrome()
{
	using namespace NotHumanWidgetDetail;
	FText Title;
	FText Detail;
	FText Progress;
	FText Action;
	bool bAction = true;
	switch (Stage)
	{
	case EStage::Intro:
		Title = FText::FromString(TEXT("Verify that you are not a human"));
		Detail = FText::FromString(TEXT("This site is protected by notHUMAN. Click the box to begin."));
		bAction = false;
		break;
	case EStage::Verifying:
		Title = FText::FromString(TEXT("Verifying..."));
		Detail = FText::FromString(TEXT("We need to make sure you are not a human."));
		bAction = false;
		break;
	case EStage::Test:
	case EStage::Judged:
	{
		const FNotHumanChallenge& C = Stage == EStage::Judged ? Judging : Test();
		Title = Utf8(C.Title);
		Detail = Utf8(C.Detail);
		const int32 Number = Stage == EStage::Judged ? Run.CurrentIndex() : Run.CurrentIndex() + 1;
		Progress = FText::FromString(FString::Printf(TEXT("Test %d of %d     %d passed"), Number, Run.Count(), Run.Passed()));
		if (Stage == EStage::Judged)
		{
			Action = FText::FromString(Run.IsOver() ? TEXT("SEE THE RESULT") : TEXT("NEXT"));
		}
		else if (C.Input == ENotHumanInput::Tiles || C.Input == ENotHumanInput::Text || C.Input == ENotHumanInput::Slider)
		{
			Action = FText::FromString(TEXT("VERIFY"));
		}
		else if (C.Input == ENotHumanInput::Checkbox)
		{
			Action = FText::FromString(TEXT("I GIVE UP"));
		}
		else
		{
			bAction = false;   // the choice buttons, the stillness and the reflex button answer for themselves
		}
		break;
	}
	case EStage::Verdict:
		Title = FText::FromString(FString::Printf(TEXT("You are %d%% machine."), Run.MachinePercent()));
		Detail = FText::FromString(FString::Printf(TEXT("%d of %d tests passed."), Run.Passed(), Run.Count()));
		Action = FText::FromString(TEXT("TRY AGAIN"));
		break;
	}
	if (TitleText) { TitleText->SetText(Title); }
	if (DetailText) { DetailText->SetText(Detail); }
	if (ProgressText) { ProgressText->SetText(Progress); }
	if (ActionLabel) { ActionLabel->SetText(Action); }
	if (ActionButton) { ActionButton->SetVisibility(bAction ? EVisibility::Visible : EVisibility::Hidden); }
	if (ResultText)
	{
		const bool bShow = Stage == EStage::Judged;
		ResultText->SetText(bShow ? Utf8(LastVerdict.Message) : FText::GetEmpty());
		ResultText->SetColorAndOpacity(LastVerdict.bPass ? Pass : Fail);
	}
}

void UNotHumanWidget::FocusForStage()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	if (Stage == EStage::Test && Test().Input == ENotHumanInput::Text && TextField.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(TextField, EFocusCause::SetDirectly);
		return;
	}
	SetKeyboardFocus();
}

// =====================================================================================
// THE BODIES

TSharedRef<SWidget> UNotHumanWidget::BuildIntroBody()
{
	using namespace NotHumanWidgetDetail;
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.93f, 0.93f, 0.93f, 1.0f)).Padding(FMargin(22.0f, 18.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(BoxSize).HeightOverride(BoxSize)
					[
						SNew(SButton)
						.ButtonStyle(&Box())
						.OnClicked_Lambda([this]()
						{
							Seed = static_cast<uint32>(FMath::Rand()) | 1u;
							Run.Start(Seed);
							Stage = EStage::Verifying;
							StageTime = 0.0f;
							if (BodyHolder)
							{
								BodyHolder->SetContent(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
									[
										SNew(STextBlock).Font(Font("Regular", 28)).ColorAndOpacity(Soft)
										.Text_Lambda([this]()
										{
											const int32 Dots = 1 + static_cast<int32>(StageTime * 4.0f) % 3;
											return FText::FromString(TEXT("Checking for a pulse") + FString::ChrN(Dots, TEXT('.')));
										})
									]);
							}
							RefreshChrome();
							return FReply::Handled();
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.0f, 0.0f, 60.0f, 0.0f))
				[
					SNew(STextBlock).Font(Font("Regular", 26)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("I'm not a human")))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font("Bold", 18)).ColorAndOpacity(Header).Text(FText::FromString(TEXT("notHUMAN")))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font("Regular", 11)).ColorAndOpacity(Soft).Text(FText::FromString(TEXT("Privacy - Terms - Feelings")))
					]
				]
			]
		];
}

TSharedRef<SWidget> UNotHumanWidget::BuildTestBody()
{
	using namespace NotHumanWidgetDetail;
	const FNotHumanChallenge& C = Test();
	TextField.Reset();

	switch (C.Input)
	{
	case ENotHumanInput::Tiles:
	{
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(5.0f));
		for (int32 I = 0; I < static_cast<int32>(C.Options.size()); ++I)
		{
			Grid->AddSlot(I % 3, I / 3)
			[
				SNew(SBox).HeightOverride(110.0f)
				[
					SNew(SButton)
					.ButtonStyle(&Flat())
					.ButtonColorAndOpacity_Lambda([this, I]() { return FSlateColor(TileSelected.IsValidIndex(I) && TileSelected[I] ? TileOn : Tile); })
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					.OnClicked_Lambda([this, I]()
					{
						if (Stage == EStage::Test && TileSelected.IsValidIndex(I))
						{
							TileSelected[I] = !TileSelected[I];
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(Font("Regular", 19)).Justification(ETextJustify::Center).AutoWrapText(true)
						.ColorAndOpacity_Lambda([this, I]() { return FSlateColor(TileSelected.IsValidIndex(I) && TileSelected[I] ? FLinearColor::White : Ink); })
						.Text(Utf8(C.Options[I]))
					]
				]
			];
		}
		return SNew(SBox).VAlign(VAlign_Center)[ Grid ];
	}
	case ENotHumanInput::Text:
	{
		const bool bDots = C.Kind == ENotHumanKind::Dots;
		const bool bMemory = C.Kind == ENotHumanKind::Memory;
		const int32 ShownSize = bDots ? 17 : bMemory ? 40 : 46;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(bDots || bMemory ? "Mono" : "Bold", ShownSize)).ColorAndOpacity(Ink).Justification(ETextJustify::Center)
				.Text_Lambda([this, bMemory]()
				{
					const FNotHumanChallenge& Now = Stage == EStage::Judged ? Judging : Test();
					// The string to remember goes after a second - unless he is looking with Code Vision.
					const bool bGone = bMemory && Stage == EStage::Test && StageTime > Now.ShowSeconds && !IsVisionOn();
					return bGone ? FText::FromString(TEXT("? ? ? ? ? ? ?")) : Utf8(Now.Shown);
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				SNew(STextBlock).Font(Font("Bold", 18)).ColorAndOpacity(Fail)
				.Text_Lambda([this]()
				{
					const FNotHumanChallenge& Now = Test();
					if (Stage != EStage::Test || Now.TimeLimit <= 0.0f)
					{
						return FText::GetEmpty();
					}
					return FText::FromString(FString::Printf(TEXT("%.1f s"), FMath::Max(0.0f, Now.TimeLimit - StageTime)));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
			[
				SAssignNew(TextField, SEditableTextBox)
				.Style(&Field())
				.Font(Font("Regular", 26))
				.HintText(FText::FromString(C.Kind == ENotHumanKind::Dreams ? TEXT("") : TEXT("Type here, then Enter")))
				.ClearKeyboardFocusOnCommit(false)
				.OnTextChanged_Lambda([this](const FText& Text) { Typed = Text.ToString(); })
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How)
				{
					if (How != ETextCommit::OnEnter)
					{
						return;
					}
					Typed = Text.ToString();
					OnAction();
				})
			];
	}
	case ENotHumanInput::Choice:
	{
		TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
		for (int32 I = 0; I < static_cast<int32>(C.Options.size()); ++I)
		{
			List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 6.0f))
			[
				SNew(SBox).HeightOverride(64.0f)
				[
					SNew(SButton).ButtonStyle(&Flat()).ButtonColorAndOpacity(Tile).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.OnClicked_Lambda([this, I]()
					{
						if (Stage == EStage::Test)
						{
							FNotHumanAnswer A;
							A.Choice = I;
							A.Seconds = StageTime;
							Submit(A);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(Font("Regular", 24)).ColorAndOpacity(Ink).Text(Utf8(C.Options[I]))
					]
				]
			];
		}
		return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(120.0f, 0.0f))[ List ];
	}
	case ENotHumanInput::Wait:
		return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font("Bold", 120)).ColorAndOpacity(Ink)
				.Text_Lambda([this]()
				{
					const float Left = FMath::Max(0.0f, Test().WaitSeconds - StageTime);
					return FText::AsNumber(Stage == EStage::Test ? FMath::CeilToInt(Left) : 0);
				})
			];
	case ENotHumanInput::Reflex:
		return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(380.0f).HeightOverride(170.0f)
				[
					SNew(SButton)
					.ButtonStyle(&Flat())
					.ButtonColorAndOpacity_Lambda([this]() { return FSlateColor(Stage == EStage::Test && StageTime >= Test().WaitSeconds ? Go : Waiting); })
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					.OnClicked_Lambda([this]()
					{
						if (Stage == EStage::Test)
						{
							FNotHumanAnswer A;
							A.Seconds = StageTime;
							Submit(A);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(Font("Bold", 34)).ColorAndOpacity(FLinearColor::White)
						.Text_Lambda([this]()
						{
							const float Green = Test().WaitSeconds;
							if (Stage == EStage::Test && StageTime >= Green)
							{
								return FText::FromString(TEXT("NOW"));
							}
							// Code Vision reads the timer the button is waiting on.
							return IsVisionOn() && Stage == EStage::Test
								? FText::FromString(FString::Printf(TEXT("green in %.2f"), Green - StageTime))
								: FText::FromString(TEXT("wait..."));
						})
					]
				]
			];
	case ENotHumanInput::Slider:
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font("Bold", 54)).ColorAndOpacity(Ink)
				.Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%.4f%%"), SliderValue)); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(30.0f, 0.0f))
			[
				SNew(SSlider).MinValue(0.0f).MaxValue(100.0f)
				.Value_Lambda([this]() { return SliderValue; })
				.OnValueChanged_Lambda([this](float Value) { if (Stage == EStage::Test) { SliderValue = Value; } })
			]
			// Code Vision: the slider is a variable, and a machine just sets it.
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 18.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return IsVisionOn() ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Font("Mono", 20)).ColorAndOpacity(Pass).Text(FText::FromString(TEXT("slider.value = ")))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(180.0f)
					[
						SNew(SEditableTextBox).Style(&Field()).Font(Font("Mono", 20))
						.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
						{
							if (Stage == EStage::Test && Text.ToString().IsNumeric())
							{
								SliderValue = FMath::Clamp(FCString::Atof(*Text.ToString()), 0.0f, 100.0f);
							}
						})
					]
				]
			];
	case ENotHumanInput::Checkbox:
		return SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(10.0f))
			[
				SNew(STextBlock).Font(Font("Regular", 16)).ColorAndOpacity(Soft)
				.Text(FText::FromString(TEXT("The box is somewhere around here.")))
			]
			+ SOverlay::Slot()
			[
				SNew(SCanvas)
				+ SCanvas::Slot()
				.Position(TAttribute<FVector2D>::CreateLambda([this]() { return BoxPos; }))
				.Size(FVector2D(BoxSize + 190.0f, BoxSize))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(BoxSize).HeightOverride(BoxSize)
						[
							SNew(SButton).ButtonStyle(&Box())
							.OnClicked_Lambda([this]()
							{
								// Caught with the mouse after all: not how a machine would do it.
								if (Stage == EStage::Test)
								{
									FNotHumanAnswer A;
									A.Seconds = StageTime;
									Submit(A);
								}
								return FReply::Handled();
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock).Font(Font("Regular", 20)).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("I'm not a human")))
					]
				]
			];
	}
	return SNew(SSpacer);
}

TSharedRef<SWidget> UNotHumanWidget::BuildVerdictBody()
{
	using namespace NotHumanWidgetDetail;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const TPair<FString, bool>& Row : History)
	{
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
			[
				SNew(SBox).WidthOverride(70.0f)
				[
					SNew(STextBlock).Font(Font("Bold", 16)).ColorAndOpacity(Row.Value ? Pass : Fail)
					.Text(FText::FromString(Row.Value ? TEXT("PASS") : TEXT("FAIL")))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(STextBlock).Font(Font("Regular", 16)).ColorAndOpacity(Ink).Text(FText::FromString(Row.Key))
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 6.0f))
		[
			SNew(STextBlock).Font(Font("Bold", 28)).ColorAndOpacity(Ink).AutoWrapText(true).Text(Utf8(Run.Rank()))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 14.0f))
		[
			SNew(STextBlock).Font(Font("Italic", 20)).ColorAndOpacity(Header).AutoWrapText(true).Text(Utf8(Run.AgentLine()))
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			List
		];
}

// =====================================================================================
// ANSWERS

void UNotHumanWidget::Submit(const FNotHumanAnswer& Answer)
{
	if (Stage != EStage::Test || Run.IsOver())
	{
		return;
	}
	Judging = Test();
	LastVerdict = Run.Submit(Answer);
	History.Add(TPair<FString, bool>(FString(UTF8_TO_TCHAR(Judging.Title.c_str())), LastVerdict.bPass));
	Stage = EStage::Judged;
	RefreshChrome();
	FocusForStage();
}

void UNotHumanWidget::SubmitCurrent()
{
	const FNotHumanChallenge& C = Test();
	FNotHumanAnswer A;
	A.Seconds = StageTime;
	switch (C.Input)
	{
	case ENotHumanInput::Tiles:
		for (int32 I = 0; I < TileSelected.Num(); ++I)
		{
			if (TileSelected[I])
			{
				A.Selected.push_back(I);
			}
		}
		break;
	case ENotHumanInput::Text:
		A.Text = TCHAR_TO_UTF8(*Typed);
		break;
	case ENotHumanInput::Slider:
		A.Value = SliderValue;
		break;
	default:
		break;   // the checkbox: I GIVE UP is a mouse-and-patience answer
	}
	Submit(A);
}

void UNotHumanWidget::OnAction()
{
	switch (Stage)
	{
	case EStage::Test:    SubmitCurrent(); break;
	case EStage::Judged:  Advance(); break;
	case EStage::Verdict: ShowIntro(); break;
	default: break;
	}
}

void UNotHumanWidget::Advance()
{
	if (Run.IsOver())
	{
		ShowVerdict();
	}
	else
	{
		ShowTest();
	}
}

void UNotHumanWidget::Close()
{
	bVHeld = false;
	OnClosed.ExecuteIfBound();
}

// =====================================================================================
// THE CLOCK, THE MOUSE AND THE KEYS

void UNotHumanWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace NotHumanWidgetDetail;
	StageTime += InDeltaTime;

	if (Stage == EStage::Verifying && StageTime > 1.4f)
	{
		ShowTest();
		return;
	}
	if (Stage != EStage::Test)
	{
		return;
	}
	const FNotHumanChallenge& C = Test();
	const FVector2D Pointer = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCursorPos() : FVector2D::ZeroVector;
	const bool bCursorMoved = FVector2D::Distance(Pointer, LastCursor) > 1.5;
	LastCursor = Pointer;

	switch (C.Input)
	{
	case ENotHumanInput::Text:
		if (C.TimeLimit > 0.0f && StageTime > C.TimeLimit + 0.05f)
		{
			SubmitCurrent();   // out of time: judged as it stands
		}
		break;
	case ENotHumanInput::Wait:
		// A moment's grace for the hand still on the mouse from clicking NEXT.
		if ((bCursorMoved || bMoved) && StageTime > 0.4f)
		{
			FNotHumanAnswer A;
			A.bMoved = true;
			A.Seconds = StageTime;
			Submit(A);
		}
		else if (StageTime >= C.WaitSeconds)
		{
			Submit(FNotHumanAnswer());
		}
		bMoved = false;
		break;
	case ENotHumanInput::Reflex:
		if (StageTime > C.WaitSeconds + 1.5f)
		{
			FNotHumanAnswer A;
			A.Seconds = StageTime;
			Submit(A);   // never clicked: far too slow
		}
		break;
	case ENotHumanInput::Checkbox:
		if (BodyBox.IsValid())
		{
			// The box runs from the pointer: whenever it comes within reach, the box is
			// somewhere else, as far from it as the panel allows.
			const FGeometry& Body = BodyBox->GetTickSpaceGeometry();
			const FVector2D Local = Body.AbsoluteToLocal(Pointer);
			const FVector2D Centre = BoxPos + FVector2D(BoxSize * 0.5, BoxSize * 0.5);
			if (FVector2D::Distance(Local, Centre) < 130.0)
			{
				FVector2D Best = BoxPos;
				double BestDistance = -1.0;
				for (int32 Try = 0; Try < 16; ++Try)
				{
					const FVector2D Candidate(FMath::FRandRange(10.0f, BodyWidth - BoxSize - 200.0f), FMath::FRandRange(10.0f, BodyHeight - BoxSize - 40.0f));
					const double Distance = FVector2D::Distance(Local, Candidate + FVector2D(BoxSize * 0.5, BoxSize * 0.5));
					if (Distance > BestDistance)
					{
						Best = Candidate;
						BestDistance = Distance;
					}
				}
				BoxPos = Best;
			}
		}
		break;
	default:
		break;
	}
}

FReply UNotHumanWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}
	// In the stillness test any key is moving, V included.
	if (Stage == EStage::Test && Test().Input == ENotHumanInput::Wait)
	{
		bMoved = true;
	}
	if (Key == EKeys::V)
	{
		bVHeld = true;
		return FReply::Handled();
	}
	const bool bGo = Key == EKeys::Enter || Key == EKeys::SpaceBar;
	if (bGo && Stage == EStage::Test && Test().Input == ENotHumanInput::Checkbox)
	{
		// The machine's way: set the box without touching the mouse.
		FNotHumanAnswer A;
		A.bByKeyboard = true;
		A.Seconds = StageTime;
		Submit(A);
		return FReply::Handled();
	}
	if (bGo && (Stage == EStage::Judged || Stage == EStage::Verdict
		|| (Stage == EStage::Test && (Test().Input == ENotHumanInput::Tiles || Test().Input == ENotHumanInput::Slider))))
	{
		OnAction();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UNotHumanWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::V)
	{
		bVHeld = false;
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}
