// ArcadeMenuWidget.cpp — Leonard's Arcade on the TVs. See the header.

#include "ArcadeMenuWidget.h"
#include "ArcadeLibrary.h"
#include "ProgressionSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"   // the clipboard
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IWebBrowserDialog.h"
#include "IWebBrowserWindow.h"             // FWebNavigationRequest
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SWebBrowserView.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "WebBrowserModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcade, Log, All);

namespace ArcadeMenuNS
{
	// The Tab menu's palette (GameMenuWidget.cpp), like the dance menu, so they read as one game.
	const FLinearColor Heading(0.55f, 0.85f, 1.0f);
	const FLinearColor Chosen(0.4f, 1.0f, 0.5f);
	const FLinearColor Body(0.9f, 0.9f, 0.9f);
	const FLinearColor Dim(0.45f, 0.45f, 0.45f);
	const FLinearColor Warn(1.0f, 0.65f, 0.25f);
	const FLinearColor Panel(0.03f, 0.04f, 0.05f, 0.94f);
	const FLinearColor Well(0.07f, 0.08f, 0.10f, 1.0f);

	// Counters in the player's lifetime stats (FProgressionState::LifetimeStats).
	const FName CodeStat(TEXT("Arcade.Code"));          // the code before code word.txt; else unwritable Documents
	const FName MadeStat(TEXT("Arcade.GamesMade"));     // games carrying the code word: a record
	const FName PlayedStat(TEXT("Arcade.GamesPlayed")); // every time a TV plays one

	TAutoConsoleVariable<int32> CVarRenderHeight(
		TEXT("sib.Arcade.RenderHeight"),
		900,
		TEXT("How many pixels tall the arcade lays a game out before stretching it to the screen. ")
		TEXT("0 = the screen's own height (sharpest, and the most work for Chromium)."),
		ECVF_Default);

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	/* THE SCROLL BARS, LIT (Walt, 2026-10-23: "could you put a little contrast on the scroll bars
	   for the list of games and the prompt display?"). Unreal's thumb is grey on grey, easy to miss
	   in a panel this dark: here a dark track and a thumb in the headings' blue, brighter under
	   the mouse and brighter still while dragged. */
	const FScrollBarStyle& Scrolls()
	{
		static const FScrollBarStyle Style = []()
		{
			FScrollBarStyle S = FAppStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
			const FSlateColorBrush Track(FLinearColor(0.015f, 0.02f, 0.03f, 1.0f));
			S.SetVerticalBackgroundImage(Track).SetVerticalTopSlotImage(Track).SetVerticalBottomSlotImage(Track);
			S.SetNormalThumbImage(FSlateRoundedBoxBrush(FLinearColor(0.42f, 0.66f, 0.85f, 0.95f), 4.0f));
			S.SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor(0.6f, 0.85f, 1.0f, 1.0f), 4.0f));
			S.SetDraggedThumbImage(FSlateRoundedBoxBrush(FLinearColor(0.82f, 0.95f, 1.0f, 1.0f), 4.0f));
			return S;
		}();
		return Style;
	}

	TSharedRef<SWidget> Button(const FText& Label, TFunction<void()> OnClick, bool bPrimary = false)
	{
		return SNew(SButton)
			.IsFocusable(false)   // the keyboard stays with the menu, so Esc always works
			.ContentPadding(FMargin(14.0f, 6.0f))
			.ButtonColorAndOpacity(bPrimary ? FLinearColor(0.15f, 0.35f, 0.5f, 1.0f) : FLinearColor(0.16f, 0.18f, 0.22f, 1.0f))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(Font("Bold", 14))
				.ColorAndOpacity(Body)
			];
	}
}

void UArcadeMenuWidget::Open()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	Folder = SibArcade::ArcadeFolder();
	IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);
	Code = PlayerCode();
	Notice.Reset();
	bConfirming = false;

	if (!IsInViewport())
	{
		AddToViewport(60);   // with the other panels, under the chat's 70
	}
	ShowMenu();

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	SetFocus();
}

void UArcadeMenuWidget::Close()
{
	bConfirming = false;
	if (bPlaying)
	{
		bPlaying = false;
		Browser.Reset();
		PlayBox.Reset();
	}
	if (IsInViewport())
	{
		RemoveFromParent();
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	// The keyboard too, not only the input mode (the dance menu's lesson): focus would
	// otherwise stay in a widget that is no longer on screen.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

FString UArcadeMenuWidget::PlayerCode() const
{
	using namespace ArcadeMenuNS;

	/* His number is drawn once, so every assignment he is ever given carries the same code
	   word and every game made from one is recognised as his. The FILE beside the games is
	   the authority (ArcadeLibrary.h, CodeFilePath): one code per PC, through a New Game, a
	   reinstall, the editor and the packaged game alike. The save's copy is read only when
	   there is no file yet, so a code handed out before the file existed is the one kept. */
	const FString File = SibArcade::CodeFilePath(Folder);
	FString Text;
	const int32 InFile = FFileHelper::LoadFileToString(Text, *File) ? SibArcade::ParseCodeWord(Text) : 0;
	int32 Number = InFile;
	UProgressionSubsystem* Prog = UProgressionSubsystem::Get(GetOwningPlayer());
	if (Number <= 0 && Prog)
	{
		Number = Prog->GetStat(CodeStat);
	}
	if (Number <= 0)
	{
		Number = FMath::RandRange(1000, 9999);
	}
	if (InFile != Number && !FFileHelper::SaveStringToFile(SibArcade::CodeFileText(Number), *File))
	{
		// Documents cannot be written to: keep the code in the save, or it would change at
		// every visit. (Nothing could be saved in the folder either, so this is a rare PC.)
		UE_LOG(LogArcade, Warning, TEXT("[Arcade] could not write %s - the code word lives in the save only"), *File);
		if (Prog && Prog->GetStat(CodeStat) <= 0)
		{
			Prog->RaiseStat(CodeStat, Number);
		}
	}
	return SibArcade::CodeWord(Number);
}

TSharedRef<SWidget> UArcadeMenuWidget::RebuildWidget()
{
	SAssignNew(Root, SBox);
	return Root.ToSharedRef();
}

void UArcadeMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Browser.Reset();
	PlayBox.Reset();
	GameList.Reset();
	Root.Reset();
}

void UArcadeMenuWidget::ShowMenu()
{
	if (!Root.IsValid())
	{
		TakeWidget();   // builds Root
	}
	if (Root.IsValid())
	{
		Root->SetContent(BuildMenu());
	}
	Rescan();
}

TSharedRef<SWidget> UArcadeMenuWidget::BuildMenu()
{
	using namespace ArcadeMenuNS;
	using SibArcade::EPick;

	TSharedRef<SVerticalBox> PickRows = SNew(SVerticalBox);
	for (int32 RowIndex = 0; RowIndex < (int32)EPick::Count; ++RowIndex)
	{
		const EPick Row = (EPick)RowIndex;
		PickRows->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(70.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SibArcade::RowLabel(Row)))
					.Font(Font("Regular", 14))
					.ColorAndOpacity(Dim)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Button(FText::FromString(TEXT("<")), [this, Row]() { Step(Row, -1); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this, Row]() { return FText::FromString(SibArcade::GetPick(Row, Assignment[Row]).Label); })
					.Font(Font("Bold", 17))
					.ColorAndOpacity(Body)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					// what this row tells the model: the picks shape the game a model makes, not Sibelius
					SNew(STextBlock)
					.Text(FText::FromString(SibArcade::RowHint(Row)))
					.Font(Font("Regular", 11))
					.ColorAndOpacity(Dim)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Button(FText::FromString(TEXT(">")), [this, Row]() { Step(Row, +1); })
			]
		];
	}

	const FString Steps = FString::Printf(
		TEXT("1.  Copy the prompt and paste it into ChatGPT, Claude or Gemini.\n")
		TEXT("2.  Save the game it makes in:\n      %s\n")
		TEXT("3.  Come back to a TV and press E. Your game will be on the right."),
		*SibArcade::FolderForDisplay(Folder));

	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(STextBlock).Text(NSLOCTEXT("Sibelius", "ArcadeNew", "NEW ASSIGNMENT")).Font(Font("Bold", 18)).ColorAndOpacity(Heading)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			PickRows
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
			[
				Button(NSLOCTEXT("Sibelius", "ArcadeSurprise", "Surprise me"), [this]() { SurpriseMe(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(NSLOCTEXT("Sibelius", "ArcadeCopy", "Copy the prompt"), [this]() { CopyPrompt(); }, /*bPrimary=*/true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Well)
			.Padding(12.0f)
			[
				SNew(SBox).HeightOverride(220.0f)
				[
					SNew(SScrollBox)
					.ScrollBarStyle(&Scrolls())
					.ScrollBarThickness(FVector2D(9.0f, 9.0f))
					.ScrollBarAlwaysVisible(true)   // the prompt is always longer than its box
					+ SScrollBox::Slot()
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FText::FromString(SibArcade::ComposePrompt(Assignment, Code)); })
						.Font(Font("Regular", 12))
						.ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.82f))
						.AutoWrapText(true)
					]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 8)
		[
			SNew(STextBlock).Text(FText::FromString(Steps)).Font(Font("Regular", 14)).ColorAndOpacity(Body).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(NSLOCTEXT("Sibelius", "ArcadeFolder", "Open the folder"), [this]() { OpenFolder(); })
			]
		];

	SAssignNew(GameList, SVerticalBox);
	TSharedRef<SVerticalBox> Right = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				const int32 Mine = Games.FilterByPredicate([](const SibArcade::FGame& G) { return !G.bBuiltIn; }).Num();
				return FText::FromString(Mine > 0 ? FString::Printf(TEXT("YOUR GAMES  (%d)"), Mine) : FString(TEXT("YOUR GAMES")));
			})
			.Font(Font("Bold", 18))
			.ColorAndOpacity(Heading)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Well)
			.Padding(8.0f)
			[
				SNew(SScrollBox)
				.ScrollBarStyle(&Scrolls())
				.ScrollBarThickness(FVector2D(9.0f, 9.0f))
				+ SScrollBox::Slot()[ GameList.ToSharedRef() ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(NSLOCTEXT("Sibelius", "ArcadeRescan", "Look again"), [this]() { Rescan(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return FText::FromString(Notice); })
			.ColorAndOpacity_Lambda([this]() { return FSlateColor(NoticeColor); })
			.Font(Font("Regular", 14))
			.AutoWrapText(true)
		];

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(40.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(28.0f)
			[
				// At least this tall, and taller when the left column needs it: a fixed height let
				// "Open the folder" run into the footer (Walt's first screenshot).
				SNew(SBox).WidthOverride(1180.0f).MinDesiredHeight(760.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(NSLOCTEXT("Sibelius", "ArcadeTitle", "LEONARD'S ARCADE")).Font(Font("Bold", 30)).ColorAndOpacity(Heading)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 18)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Sibelius", "ArcadeSub", "Make a game with one prompt. Any frontier model can do it: ChatGPT, Claude or Gemini."))
						.Font(Font("Regular", 15))
						.ColorAndOpacity(Body)
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(0.56f).Padding(0, 0, 24, 0)[ Left ]
						+ SHorizontalBox::Slot().FillWidth(0.44f)[ Right ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("[Esc] back to the house        Your code word: %s"), *Code)))
						.Font(Font("Regular", 13))
						.ColorAndOpacity(Dim)
					]
				]
			]
		];
}

void UArcadeMenuWidget::RebuildGameList()
{
	using namespace ArcadeMenuNS;

	if (!GameList.IsValid())
	{
		return;
	}
	GameList->ClearChildren();
	if (Games.Num() == 0 || Games[0].bBuiltIn)
	{
		GameList->AddSlot().AutoHeight().Padding(8)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("Sibelius", "ArcadeEmpty", "No games of yours yet. Make one with the prompt on the left, save it in the folder, and it appears here."))
			.Font(Font("Regular", 14))
			.ColorAndOpacity(Dim)
			.AutoWrapText(true)
		];
	}
	for (int32 Index = 0; Index < Games.Num(); ++Index)
	{
		const SibArcade::FGame& G = Games[Index];
		if (G.bBuiltIn && (Index == 0 || !Games[Index - 1].bBuiltIn))
		{
			// Rescan put the arcade's own games after his: one heading where they start.
			GameList->AddSlot().AutoHeight().Padding(4, 16, 4, 6)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Sibelius", "ArcadeSamples", "FROM THE ARCADE   one prompt each"))
				.Font(Font("Bold", 15))
				.ColorAndOpacity(Heading)
			];
		}
		// A sample shows the picks it was made from, so the four rows on the left have something to point at.
		const FString Sub = G.bBuiltIn ? (G.Picks.IsEmpty() ? FString(TEXT("comes with the arcade")) : G.Picks)
			: G.bHasCode ? FString::Printf(TEXT("%s   ✓ from your assignment"), *G.FileName)
			: G.FileName;
		TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(12.0f, 8.0f))
				.ButtonColorAndOpacity(FLinearColor(0.10f, 0.12f, 0.15f, 1.0f))
				.OnClicked_Lambda([this, Index]() { Play(Index); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(G.Title)).Font(Font("Bold", 17)).ColorAndOpacity(Body)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Sub)).Font(Font("Regular", 12)).AutoWrapText(true)
						.ColorAndOpacity(G.bBuiltIn ? FLinearColor(0.62f, 0.78f, 0.9f) : G.bHasCode ? Chosen : Dim)
					]
				]
			];
		if (!G.Picks.IsEmpty())
		{
			// SEE THE PROMPT: the game's picks go to the left, and the prompt box shows what it was made from
			Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
			[
				Button(NSLOCTEXT("Sibelius", "ArcadeSeePrompt", "See the\nprompt"), [this, Index]()
				{
					if (!Games.IsValidIndex(Index))
					{
						return;
					}
					SibArcade::FAssignment From;
					if (SibArcade::AssignmentFromPicks(Games[Index].Picks, From))
					{
						Assignment = From;
						Notice = FString::Printf(TEXT("On the left: the prompt %s was made from. Copy it, or change a pick ")
							TEXT("and see what your model makes instead."), *Games[Index].Title);
						NoticeColor = Chosen;
					}
					else
					{
						Notice = FString::Printf(TEXT("%s was made from picks this menu does not have."), *Games[Index].Title);
						NoticeColor = Warn;
					}
				})
			];
		}
		GameList->AddSlot().AutoHeight().Padding(0, 2)[ Line ];
	}
}

void UArcadeMenuWidget::Step(SibArcade::EPick Row, int32 Delta)
{
	const int32 N = SibArcade::NumPicks(Row);
	Assignment[Row] = ((Assignment[Row] + Delta) % N + N) % N;
}

void UArcadeMenuWidget::SurpriseMe()
{
	FRandomStream Rng(FPlatformTime::Cycles());
	Assignment = SibArcade::Surprise(Rng);
}

void UArcadeMenuWidget::CopyPrompt()
{
	FPlatformApplicationMisc::ClipboardCopy(*SibArcade::ComposePrompt(Assignment, Code));
	Notice = TEXT("The prompt is copied. Paste it into ChatGPT, Claude or Gemini.");
	NoticeColor = ArcadeMenuNS::Chosen;
}

void UArcadeMenuWidget::OpenFolder()
{
	FPlatformProcess::ExploreFolder(*SibArcade::FolderForDisplay(Folder));
	Notice = TEXT("The folder is open in Windows. Save the game there, then press Look again.");
	NoticeColor = ArcadeMenuNS::Body;
}

void UArcadeMenuWidget::Rescan()
{
	Games = SibArcade::FindGames(Folder, Code);
	int32 Made = 0;
	for (const SibArcade::FGame& G : Games)
	{
		Made += G.bHasCode ? 1 : 0;
	}

	// His games first, newest at the top; then the arcade's own, in title order, every visit.
	TArray<SibArcade::FGame> Samples = SibArcade::FindGames(SibArcade::SampleFolder(), FString());
	for (SibArcade::FGame& S : Samples)
	{
		S.bBuiltIn = true;
	}
	Samples.Sort([](const SibArcade::FGame& L, const SibArcade::FGame& R) { return L.Title < R.Title; });
	Games.Append(MoveTemp(Samples));
	if (UProgressionSubsystem* Prog = UProgressionSubsystem::Get(GetOwningPlayer()))
	{
		Prog->RaiseStat(ArcadeMenuNS::MadeStat, Made);
	}
	RebuildGameList();
}

void UArcadeMenuWidget::Play(int32 GameIndex, bool bConfirmed)
{
	if (!Games.IsValidIndex(GameIndex) || !Root.IsValid())
	{
		return;
	}
	const SibArcade::FGame G = Games[GameIndex];   // a copy: ShowMenu below rescans Games
	if (!IFileManager::Get().FileExists(*G.Path))
	{
		bConfirming = false;
		Notice = FString::Printf(TEXT("%s is not in the folder any more."), *G.FileName);
		NoticeColor = ArcadeMenuNS::Warn;
		ShowMenu();
		return;
	}
	if (!G.bHasCode && !G.bBuiltIn && !TrustedPaths.Contains(G.Path))
	{
		if (!bConfirmed)
		{
			bConfirming = true;
			Root->SetContent(BuildConfirm(GameIndex));
			return;
		}
		TrustedPaths.Add(G.Path);
	}
	bConfirming = false;
	if (!IWebBrowserModule::Get().IsWebModuleAvailable())
	{
		Notice = TEXT("The TV's browser could not start, so the game cannot play.");
		NoticeColor = ArcadeMenuNS::Warn;
		UE_LOG(LogArcade, Error, TEXT("[Arcade] the WebBrowser module is not available"));
		return;
	}

	BlockedHosts.Reset();
	PlayingTitle = G.Title;
	bPlaying = true;
	PlaySeconds = 0.0f;
	FocusTicks = 10;
	const FString Url = SibArcade::FileUrl(G.Path);
	UE_LOG(LogArcade, Display, TEXT("[Arcade] playing \"%s\" from %s"), *G.Title, *Url);
	Root->SetContent(BuildPlay(Url));

	if (UProgressionSubsystem* Prog = UProgressionSubsystem::Get(GetOwningPlayer()))
	{
		Prog->BumpStat(ArcadeMenuNS::PlayedStat);
	}
}

TSharedRef<SWidget> UArcadeMenuWidget::BuildConfirm(int32 GameIndex)
{
	using namespace ArcadeMenuNS;

	const FString Title = Games.IsValidIndex(GameIndex) ? Games[GameIndex].Title : FString();
	const FString Why = FString::Printf(
		TEXT("This game was not made from your assignment: it does not carry your code word, %s.\n\n")
		TEXT("The TV runs a game's code on this computer. A game you made with a model is safe. ")
		TEXT("A file from a stranger might not be. Only play games from people you trust."),
		*Code);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(40.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(28.0f)
			[
				SNew(SBox).WidthOverride(720.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(NSLOCTEXT("Sibelius", "ArcadeConfirm", "BEFORE IT PLAYS")).Font(Font("Bold", 24)).ColorAndOpacity(Warn)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 14)
					[
						SNew(STextBlock).Text(FText::FromString(Title)).Font(Font("Bold", 20)).ColorAndOpacity(Body)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(Why)).Font(Font("Regular", 15)).ColorAndOpacity(Body).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 22, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
						[
							Button(NSLOCTEXT("Sibelius", "ArcadePlayAnyway", "Play anyway"), [this, GameIndex]() { Play(GameIndex, /*bConfirmed=*/true); }, /*bPrimary=*/true)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							Button(NSLOCTEXT("Sibelius", "ArcadeBack", "Back"), [this]() { bConfirming = false; ShowMenu(); })
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
					[
						SNew(STextBlock).Text(NSLOCTEXT("Sibelius", "ArcadeConfirmEsc", "[Esc] back")).Font(Font("Regular", 13)).ColorAndOpacity(Dim)
					]
				]
			]
		];
}

TSharedRef<SWidget> UArcadeMenuWidget::BuildPlay(const FString& Url)
{
	using namespace ArcadeMenuNS;

	TWeakObjectPtr<UArcadeMenuWidget> Weak(this);
	SAssignNew(Browser, SWebBrowserView)
		.InitialURL(Url)
		.ShowErrorMessage(true)
		.SupportsTransparency(false)
		.InterceptLoadRequests(true)
		.SupportsThumbMouseButtonNavigation(false)
		.BackgroundColor(FColor::Black)
		.BrowserFrameRate(60)
		.OnBeforeNavigation_Lambda([Weak](const FString& NavUrl, const FWebNavigationRequest& Request)
		{
			return Weak.IsValid() ? Weak->HandleBeforeNavigation(NavUrl, Request) : true;
		})
		.OnLoadUrl_Lambda([Weak](const FString& Method, const FString& LoadUrl, FString& Response)
		{
			if (Weak.IsValid())
			{
				return Weak->HandleLoadUrl(Method, LoadUrl, Response);
			}
			Response.Reset();
			return !SibArcade::IsOffline(LoadUrl);
		})
		.OnBeforePopup_Lambda([Weak](FString PopupUrl, FString Frame)
		{
			if (Weak.IsValid() && !SibArcade::IsOffline(PopupUrl))
			{
				Weak->BlockedHosts.Add(SibArcade::HostOf(PopupUrl));
			}
			return true;   // no popups, ever: a new window is a way off the page
		})
		.OnSuppressContextMenu_Lambda([]() { return true; })
		// alert() and confirm() would stop the game waiting for a dialog nobody draws.
		.OnShowDialog_Lambda([](const TWeakPtr<IWebBrowserDialog>&) { return EWebBrowserDialogEventResponse::Continue; })
		.OnConsoleMessage_Lambda([](const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity)
		{
			// A game that breaks says why here, in the log, where its maker can read it.
			if (Severity >= EWebBrowserConsoleLogSeverity::Warning)
			{
				UE_LOG(LogArcade, Display, TEXT("[Arcade] page: %s (%s:%d)"), *Message, *FPaths::GetCleanFilename(Source), Line);
			}
		});

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor::Black)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SAssignNew(PlayBox, SBox)
			.WidthOverride(1280.0f)
			.HeightOverride(720.0f)
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			[
				Browser.ToSharedRef()
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16.0f)
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Text(FText::FromString(FString::Printf(TEXT("%s      [Esc] back to the arcade"), *PlayingTitle)))
			.Font(Font("Regular", 13))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.ColorAndOpacity_Lambda([this]()
			{
				// On screen for four seconds, gone by six: the game is the point.
				const float A = FMath::Clamp(1.0f - (PlaySeconds - 4.0f) / 2.0f, 0.0f, 1.0f);
				return FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 0.8f * A));
			})
		];
}

bool UArcadeMenuWidget::HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request)
{
	if (SibArcade::IsOffline(Url))
	{
		return false;   // the game's own file, or another file beside it
	}
	BlockedHosts.Add(SibArcade::HostOf(Url));
	UE_LOG(LogArcade, Display, TEXT("[Arcade] kept the page from going to %s"), *Url);
	return true;   // handled: the page stays where it is
}

bool UArcadeMenuWidget::HandleLoadUrl(const FString& Method, const FString& Url, FString& Response)
{
	if (SibArcade::IsOffline(Url))
	{
		return false;   // load it as usual
	}
	BlockedHosts.Add(SibArcade::HostOf(Url));
	UE_LOG(LogArcade, Display, TEXT("[Arcade] answered %s %s with nothing (offline)"), *Method, *Url);
	Response.Reset();
	return true;   // answered, with nothing
}

void UArcadeMenuWidget::StopPlaying()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	Browser.Reset();
	PlayBox.Reset();
	if (BlockedHosts.Num() > 0)
	{
		Notice = FString::Printf(TEXT("%s asked the internet for %s. The arcade plays offline: ask the model to put ")
			TEXT("everything into the one file, with no libraries or links."),
			*PlayingTitle, *FString::Join(BlockedHosts.Array(), TEXT(", ")));
		NoticeColor = ArcadeMenuNS::Warn;
	}
	else
	{
		Notice.Reset();
	}
	ShowMenu();
	SetFocus();
}

void UArcadeMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bPlaying || !PlayBox.IsValid())
	{
		return;
	}
	PlaySeconds += InDeltaTime;

	// Lay the page out RenderHeight pixels tall, in the screen's shape, then draw it scaled up.
	const FVector2D Local = MyGeometry.GetLocalSize();
	const float ScreenPixelsTall = FMath::Max(1.0f, Local.Y * MyGeometry.Scale);
	const int32 Wanted = ArcadeMenuNS::CVarRenderHeight.GetValueOnGameThread();
	const float Shrink = Wanted > 0 ? FMath::Clamp((float)Wanted / ScreenPixelsTall, 0.1f, 1.0f) : 1.0f;
	PlayBox->SetWidthOverride(FMath::Max(1.0f, Local.X * Shrink));
	PlayBox->SetHeightOverride(FMath::Max(1.0f, Local.Y * Shrink));
	PlayBox->SetRenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(1.0f / Shrink))));

	// The page has the keyboard, or the arrows would go nowhere. A few ticks, because the view
	// has to be on screen before Slate will hand it focus.
	if (FocusTicks > 0 && Browser.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocus(Browser, EFocusCause::SetDirectly);
		--FocusTicks;
	}
}

FReply UArcadeMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Esc belongs to the arcade, before Chromium sees it - the prompt tells every game never
	// to use it, and this is why.
	if (bPlaying && InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!InKeyEvent.IsRepeat())
		{
			StopPlaying();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UArcadeMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bPlaying && InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Not on a repeat: an Esc held from leaving a game would close the menu too.
		if (!InKeyEvent.IsRepeat())
		{
			if (bConfirming)
			{
				bConfirming = false;
				ShowMenu();   // from "before it plays", Esc is Back
			}
			else
			{
				Close();
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
