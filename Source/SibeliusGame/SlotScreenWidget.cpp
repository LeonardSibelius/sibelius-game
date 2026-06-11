// SlotScreenWidget.cpp — SIB-34 S2. See header + docs/sib-34-s2-s3-slot-cabinet-notes.md.

#include "SlotScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "SlotGameModel.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlotScreen, Log, All);

namespace
{
	// Art ids on disk (the June 11 vector set). Earth -> scatter art (docs note).
	const TCHAR* SpriteIdFor(ESlotSymbol S)
	{
		switch (S)
		{
		case ESlotSymbol::Star:   return TEXT("star");
		case ESlotSymbol::Moon:   return TEXT("moon");
		case ESlotSymbol::Galaxy: return TEXT("galaxy");
		case ESlotSymbol::Saturn: return TEXT("saturn");
		case ESlotSymbol::Mars:   return TEXT("mars");
		case ESlotSymbol::Crown:  return TEXT("crown");
		case ESlotSymbol::Seven:  return TEXT("seven");
		case ESlotSymbol::Wild:   return TEXT("wild");
		case ESlotSymbol::Earth:  return TEXT("scatter");
		default:                  return TEXT("star");
		}
	}

	const TCHAR* DisplayNameFor(ESlotSymbol S)
	{
		switch (S)
		{
		case ESlotSymbol::Star:   return TEXT("Star");
		case ESlotSymbol::Moon:   return TEXT("Moon");
		case ESlotSymbol::Galaxy: return TEXT("Galaxy");
		case ESlotSymbol::Saturn: return TEXT("Saturn");
		case ESlotSymbol::Mars:   return TEXT("Mars");
		case ESlotSymbol::Crown:  return TEXT("Crown");
		case ESlotSymbol::Seven:  return TEXT("Lucky 7");
		case ESlotSymbol::Wild:   return TEXT("Wild");
		case ESlotSymbol::Earth:  return TEXT("Earth");
		default:                  return TEXT("?");
		}
	}

	constexpr float CELL_SIZE = 150.0f;   // px per symbol cell (4K-friendly)
	constexpr float REVEAL_STEP = 0.14f;  // seconds between reel reveals
}

USlotScreenWidget::USlotScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);   // required to receive Space/Esc in UIOnly
}

void USlotScreenWidget::InitModel(int32 Seed)
{
	if (!Model)
	{
		Model = NewObject<USlotGameModel>(this);
	}
	Model->Init(Seed);
	bModelReady = true;
	UE_LOG(LogSlotScreen, Display, TEXT("[Slot] model seeded (%d), credits %lld"), Seed, Credits);
}

TSharedRef<SWidget> USlotScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SlotRoot"));
		WidgetTree->RootWidget = Canvas;

		// Cabinet panel: dark navy with a gold rim (outer gold border wrapping
		// an inner navy border — the web machine's look).
		UBorder* Gold = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotGoldRim"));
		Gold->SetBrushColor(FLinearColor(0.83f, 0.66f, 0.21f, 1.0f));
		Gold->SetPadding(FMargin(6.0f));

		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotBg"));
		Bg->SetBrushColor(FLinearColor(0.035f, 0.035f, 0.10f, 0.98f));
		Bg->SetPadding(FMargin(28.0f, 20.0f));
		Gold->SetContent(Bg);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotBox"));
		Bg->SetContent(Box);

		// Title
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotTitle"));
		Title->SetText(FText::FromString(TEXT("C E L E S T I A L   F O R T U N E")));
		Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 36));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.30f, 1.0f)));
		Title->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Title)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 14)); }

		// 5x3 reel grid
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SlotGrid"));
		Grid->SetSlotPadding(FMargin(6.0f));
		Cells.SetNum(USlotGameModel::REELS * USlotGameModel::ROWS);
		for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
		{
			for (int32 Row = 0; Row < USlotGameModel::ROWS; ++Row)
			{
				USizeBox* CellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				CellBox->SetWidthOverride(CELL_SIZE);
				CellBox->SetHeightOverride(CELL_SIZE);
				UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Img->SetColorAndOpacity(FLinearColor(1, 1, 1, 1));
				CellBox->AddChild(Img);
				Cells[Reel * USlotGameModel::ROWS + Row] = Img;
				if (UUniformGridSlot* GS = Grid->AddChildToUniformGrid(CellBox, Row, Reel))
				{
					GS->SetHorizontalAlignment(HAlign_Center);
					GS->SetVerticalAlignment(VAlign_Center);
				}
			}
		}
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Grid)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0, 0, 0, 14)); }

		// HUD row: CREDITS | WIN | FREE SPINS
		UHorizontalBox* Hud = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotHud"));
		auto MakeHudText = [&](const TCHAR* Name) -> UTextBlock*
		{
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			T->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 26));
			T->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.92f, 0.62f, 1.0f)));
			if (UHorizontalBoxSlot* HS = Hud->AddChildToHorizontalBox(T))
			{
				HS->SetPadding(FMargin(22, 0));
				HS->SetHorizontalAlignment(HAlign_Center);
				HS->SetSize(ESlateSizeRule::Fill);
			}
			return T;
		};
		CreditsText = MakeHudText(TEXT("SlotCredits"));
		WinText = MakeHudText(TEXT("SlotWin"));
		FreeSpinsText = MakeHudText(TEXT("SlotFree"));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Hud)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 10)); }

		// Win-lines readout (fills as lines hit)
		LinesText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotLines"));
		LinesText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 22));
		LinesText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.93f, 1.0f, 1.0f)));
		LinesText->SetJustification(ETextJustify::Center);
		LinesText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(LinesText)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 8)); }

		// Hint
		HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotHint"));
		HintText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 22));
		HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.75f, 1.0f)));
		HintText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(HintText)) { S->SetHorizontalAlignment(HAlign_Fill); }

		// SC3: the proven stretch-anchor layout — never point-anchor + SetSize.
		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Gold))
		{
			CSlot->SetAnchors(FAnchors(0.20f, 0.06f, 0.80f, 0.94f));
			CSlot->SetOffsets(FMargin(0.0f));
		}
	}

	return Super::RebuildWidget();
}

void USlotScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadSymbolTextures();

	// Idle grid: a calm diagonal of symbols so the screen never opens blank.
	static const ESlotSymbol Idle[5] = { ESlotSymbol::Seven, ESlotSymbol::Star, ESlotSymbol::Crown, ESlotSymbol::Saturn, ESlotSymbol::Galaxy };
	for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
	{
		for (int32 Row = 0; Row < USlotGameModel::ROWS; ++Row)
		{
			SetCell(Reel, Row, Idle[(Reel + Row) % 5], /*bDimmed=*/true);
		}
	}
	UpdateHud();
}

void USlotScreenWidget::NativeDestruct()
{
	// SC7: never leak reveal timers.
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& H : RevealTimers) { World->GetTimerManager().ClearTimer(H); }
	}
	RevealTimers.Reset();
	bRevealing = false;
	Super::NativeDestruct();
}

FReply USlotScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		OnClosed.ExecuteIfBound();   // SC1: the ONE close path (cabinet restores input)
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter)
	{
		TrySpin();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USlotScreenWidget::TrySpin()
{
	if (!bModelReady || bRevealing) { return; }                       // SC7 latch
	if (!Model->IsInFreeSpins() && Credits < TOTAL_BET)               // SC6 guard
	{
		if (HintText) { HintText->SetText(FText::FromString(TEXT("Out of credits — Esc to leave (it resets next visit)"))); }
		return;
	}

	PendingResult = Model->Spin(TOTAL_BET);
	if (!PendingResult.bWasFreeSpin) { Credits -= TOTAL_BET; }        // SC5: model says whether it was free

	bRevealing = true;
	if (WinText) { WinText->SetText(FText::FromString(TEXT("WIN  —"))); }
	if (LinesText) { LinesText->SetText(FText::GetEmpty()); }
	UpdateHud();

	// Dim everything, then reveal reel by reel.
	for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
	{
		for (int32 Row = 0; Row < USlotGameModel::ROWS; ++Row)
		{
			if (UImage* Img = Cells[Reel * USlotGameModel::ROWS + Row]) { Img->SetColorAndOpacity(FLinearColor(1, 1, 1, 0.12f)); }
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		// No world (shouldn't happen in PIE) — reveal instantly.
		for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel) { RevealReel(Reel); }
		FinishReveal();
		return;
	}

	RevealTimers.SetNum(USlotGameModel::REELS + 1);
	for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
	{
		World->GetTimerManager().SetTimer(RevealTimers[Reel],
			FTimerDelegate::CreateUObject(this, &USlotScreenWidget::RevealReel, Reel),
			REVEAL_STEP * (Reel + 1), false);
	}
	World->GetTimerManager().SetTimer(RevealTimers[USlotGameModel::REELS],
		FTimerDelegate::CreateUObject(this, &USlotScreenWidget::FinishReveal),
		REVEAL_STEP * (USlotGameModel::REELS + 1), false);
}

void USlotScreenWidget::RevealReel(int32 ReelIndex)
{
	for (int32 Row = 0; Row < USlotGameModel::ROWS; ++Row)
	{
		SetCell(ReelIndex, Row, PendingResult.At(ReelIndex, Row), /*bDimmed=*/false);
	}
}

void USlotScreenWidget::FinishReveal()
{
	bRevealing = false;
	Credits += static_cast<int64>(PendingResult.TotalWin);            // SC5: credited exactly once

	FString Lines;
	for (const FSlotLineWin& W : PendingResult.LineWins)
	{
		Lines += FString::Printf(TEXT("Line %d — %s ×%d — %.0f      "), W.LineIndex + 1, DisplayNameFor(W.Symbol), W.Count, W.Pay);
	}
	if (PendingResult.bBonusTriggered)
	{
		Lines += FString::Printf(TEXT("\n✦  %d FREE SPINS — ALL WINS ×%d  ✦"), USlotGameModel::FREE_SPINS_AWARD, USlotGameModel::FREE_SPIN_MULTIPLIER);
	}
	if (LinesText) { LinesText->SetText(FText::FromString(Lines)); }
	if (WinText) { WinText->SetText(FText::FromString(FString::Printf(TEXT("WIN  %.0f"), PendingResult.TotalWin))); }
	UpdateHud();
}

void USlotScreenWidget::SetCell(int32 Reel, int32 Row, ESlotSymbol Symbol, bool bDimmed)
{
	UImage* Img = Cells.IsValidIndex(Reel * USlotGameModel::ROWS + Row) ? Cells[Reel * USlotGameModel::ROWS + Row].Get() : nullptr;
	if (!Img) { return; }

	if (TObjectPtr<UTexture2D>* Tex = SymbolTextures.Find(Symbol); Tex && *Tex)
	{
		Img->SetBrushFromTexture(*Tex, /*bMatchSize=*/false);
		Img->SetColorAndOpacity(FLinearColor(1, 1, 1, bDimmed ? 0.35f : 1.0f));
	}
	else
	{
		// SC4 fallback: a colored block beats an invisible cell.
		Img->SetBrushFromTexture(nullptr);
		Img->SetColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.6f, bDimmed ? 0.35f : 1.0f));
	}
}

void USlotScreenWidget::UpdateHud()
{
	if (CreditsText) { CreditsText->SetText(FText::FromString(FString::Printf(TEXT("CREDITS  %lld"), Credits))); }
	if (FreeSpinsText)
	{
		const int32 FS = Model ? Model->GetFreeSpinsRemaining() : 0;
		FreeSpinsText->SetText(FS > 0
			? FText::FromString(FString::Printf(TEXT("FREE SPINS  %d  (×%d)"), FS, USlotGameModel::FREE_SPIN_MULTIPLIER))
			: FText::FromString(FString::Printf(TEXT("BET  %d"), TOTAL_BET)));
	}
	if (HintText) { HintText->SetText(FText::FromString(TEXT("SPACE — spin        Esc — leave"))); }
}

void USlotScreenWidget::LoadSymbolTextures()
{
	static const ESlotSymbol All[] = {
		ESlotSymbol::Star, ESlotSymbol::Moon, ESlotSymbol::Galaxy, ESlotSymbol::Saturn,
		ESlotSymbol::Mars, ESlotSymbol::Crown, ESlotSymbol::Seven, ESlotSymbol::Wild, ESlotSymbol::Earth
	};
	for (ESlotSymbol S : All)
	{
		const FString Path = FString::Printf(TEXT("/Game/SlotFactory/SymbolSprites/T_sym_%s.T_sym_%s"), SpriteIdFor(S), SpriteIdFor(S));
		UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path);
		if (Tex)
		{
			SymbolTextures.Add(S, Tex);
		}
		else
		{
			UE_LOG(LogSlotScreen, Error, TEXT("[Slot] SPRITE MISSING: %s — run Tools/Scripts/import_symbol_sprites.py (SC4)"), *Path);
		}
	}
	UE_LOG(LogSlotScreen, Display, TEXT("[Slot] %d/9 symbol sprites loaded"), SymbolTextures.Num());
}
