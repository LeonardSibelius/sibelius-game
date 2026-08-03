// SlotScreenWidget.cpp — SIB-34 S2 + the 2026-07-17 facelift (spinning reels,
// win presentation, procedural sound). See header + docs/BOLD_PLAN.md.

#include "SlotScreenWidget.h"

#include "ProceduralPcm.h"          // SND_SR + AppendSample, shared with the poker machine
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/AudioComponent.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "SlotGameModel.h"

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

	constexpr float CELL_SIZE = 150.0f;   // px per symbol sprite (4K-friendly)
	constexpr float CELL_PAD = 6.0f;      // gap inside each cell
	constexpr float STEP = CELL_SIZE + 2.0f * CELL_PAD;  // 162: one strip step
	constexpr int32 STRIP_CELLS = 4;      // 1 hidden feed cell above 3 visible rows

	// Spin feel (Walt's trial grinds many spins — total ~2.3 s, slam-able).
	constexpr float SPIN_VMAX = 2400.0f;  // px/s at full speed
	constexpr float SPIN_ACCEL = 14000.0f;
	constexpr float STOP_VEND = 620.0f;   // px/s at the final landing step
	constexpr float FIRST_STOP = 0.85f;   // s until reel 0 begins landing
	constexpr float STOP_STAGGER = 0.24f; // s between reel landings, L->R
	constexpr int32 LAND_SHIFTS = 4;      // decelerating steps; last 3 feeds = result
	constexpr float BOUNCE_PX = 14.0f;    // landing overshoot
	constexpr float BOUNCE_SECS = 0.16f;

	// SND_SR / AppendSample now live in ProceduralPcm.h — see that header for why.
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

		// 5 reel windows, each clipping a 4-cell scrolling strip (the extra
		// cell rides hidden above the window and feeds symbols in).
		UHorizontalBox* ReelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotReels"));
		Cells.SetNum(USlotGameModel::REELS * STRIP_CELLS);
		CellGlows.SetNum(USlotGameModel::REELS * STRIP_CELLS);
		StripBoxes.SetNum(USlotGameModel::REELS);
		for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
		{
			USizeBox* Window = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Window->SetWidthOverride(STEP);
			Window->SetHeightOverride(STEP * USlotGameModel::ROWS);
			Window->SetClipping(EWidgetClipping::ClipToBounds);

			UCanvasPanel* ClipCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
			Window->AddChild(ClipCanvas);

			UVerticalBox* Strip = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			StripBoxes[Reel] = Strip;
			if (UCanvasPanelSlot* CS = ClipCanvas->AddChildToCanvas(Strip))
			{
				CS->SetAnchors(FAnchors(0.0f, 0.0f));
				CS->SetPosition(FVector2D(0.0f, -STEP));   // hidden cell above the window
				CS->SetSize(FVector2D(STEP, STEP * STRIP_CELLS));
				CS->SetAutoSize(false);
			}

			for (int32 StripIdx = 0; StripIdx < STRIP_CELLS; ++StripIdx)
			{
				USizeBox* CellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				CellBox->SetWidthOverride(STEP);
				CellBox->SetHeightOverride(STEP);

				UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
				CellBox->AddChild(Ov);

				UBorder* GlowB = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				GlowB->SetBrushColor(FLinearColor(0, 0, 0, 0));   // dark until a win lights it
				if (UOverlaySlot* OS = Ov->AddChildToOverlay(GlowB))
				{
					OS->SetHorizontalAlignment(HAlign_Fill);
					OS->SetVerticalAlignment(VAlign_Fill);
					OS->SetPadding(FMargin(2.0f));
				}

				UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Img->SetColorAndOpacity(FLinearColor(1, 1, 1, 1));
				if (UOverlaySlot* OS = Ov->AddChildToOverlay(Img))
				{
					OS->SetHorizontalAlignment(HAlign_Fill);
					OS->SetVerticalAlignment(VAlign_Fill);
					OS->SetPadding(FMargin(CELL_PAD));
				}

				Cells[Reel * STRIP_CELLS + StripIdx] = Img;
				CellGlows[Reel * STRIP_CELLS + StripIdx] = GlowB;
				Strip->AddChildToVerticalBox(CellBox);
			}

			ReelRow->AddChildToHorizontalBox(Window);
		}
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(ReelRow)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0, 0, 0, 14)); }

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
		if (WinText) { WinText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); }
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

		// Celebration banner (free spins, THE MACHINE YIELDS) — above the panel.
		BannerBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotBanner"));
		BannerBox->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.09f, 0.94f));
		BannerBox->SetPadding(FMargin(30.0f, 16.0f));
		BannerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotBannerText"));
		BannerText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 34));
		BannerText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.32f, 1.0f)));
		BannerText->SetJustification(ETextJustify::Center);
		BannerBox->SetContent(BannerText);
		BannerBox->SetVisibility(ESlateVisibility::Collapsed);
		BannerBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (UCanvasPanelSlot* BSlot = Canvas->AddChildToCanvas(BannerBox))
		{
			BSlot->SetAnchors(FAnchors(0.5f, 0.40f));
			BSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			BSlot->SetAutoSize(true);
			BSlot->SetZOrder(10);
		}
	}

	return Super::RebuildWidget();
}

void USlotScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadSymbolTextures();
	BuildSoundBank();

	// Idle grid: a calm diagonal of symbols so the screen never opens blank.
	static const ESlotSymbol Idle[5] = { ESlotSymbol::Seven, ESlotSymbol::Star, ESlotSymbol::Crown, ESlotSymbol::Saturn, ESlotSymbol::Galaxy };
	for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
	{
		FSlotReelAnim& R = Reels[Reel];
		R.State = EReelSpinState::Idle;
		R.Pos = 0.0f;
		R.StripCursor = Reel * 3;
		for (int32 StripIdx = 0; StripIdx < STRIP_CELLS; ++StripIdx)
		{
			R.Symbols[StripIdx] = Idle[(Reel + StripIdx) % 5];
		}
		RefreshReelImages(Reel);
		if (StripBoxes.IsValidIndex(Reel) && StripBoxes[Reel]) { StripBoxes[Reel]->SetRenderTranslation(FVector2D(0.0f, 0.0f)); }
	}
	CreditsShown = static_cast<double>(Credits);
	UpdateHud();
}

void USlotScreenWidget::NativeDestruct()
{
	StopWhir();
	Super::NativeDestruct();
}

void USlotScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Dt = FMath::Min(InDeltaTime, 0.05f);   // hitch guard: never leap a whole cell

	// Credits count-up toward the true balance.
	const double Target = static_cast<double>(Credits);
	if (CreditsShown != Target)
	{
		if (CreditsShown < Target)
		{
			CreditsShown = FMath::Min(Target, CreditsShown + CreditRate * Dt);
		}
		else
		{
			CreditsShown = Target;   // deductions land instantly (casino standard)
		}
		if (CreditsText) { CreditsText->SetText(FText::FromString(FString::Printf(TEXT("CREDITS  %lld"), static_cast<int64>(FMath::RoundToDouble(CreditsShown))))); }
	}

	// Win text pulse.
	if (WinPulse >= 0.0f && WinText)
	{
		WinPulse += Dt;
		const float Scale = 1.0f + 0.22f * FMath::Exp(-2.5f * WinPulse) * FMath::Abs(FMath::Sin(WinPulse * 14.0f));
		WinText->SetRenderScale(FVector2D(Scale, Scale));
		if (WinPulse > 1.2f) { WinText->SetRenderScale(FVector2D(1.0f, 1.0f)); WinPulse = -1.0f; }
	}

	// Payline glow pulse.
	if (GlowCellIdx.Num() > 0)
	{
		GlowPhase += Dt;
		const float A = 0.32f + 0.26f * FMath::Sin(GlowPhase * 7.0f);
		const FLinearColor GlowCol(1.0f, 0.78f, 0.22f, A);
		for (int32 Idx : GlowCellIdx)
		{
			if (CellGlows.IsValidIndex(Idx) && CellGlows[Idx]) { CellGlows[Idx]->SetBrushColor(GlowCol); }
		}
	}

	// Banner: pop in, hold, fade.
	if (BannerTime >= 0.0f && BannerBox)
	{
		BannerTime += Dt;
		const float In = FMath::Min(1.0f, BannerTime / 0.22f);
		const float Scale = 0.7f + 0.3f * (1.0f - FMath::Pow(1.0f - In, 3.0f));
		BannerBox->SetRenderScale(FVector2D(Scale, Scale));
		const float Op = (BannerTime > 2.0f) ? 1.0f - (BannerTime - 2.0f) / 0.4f : 1.0f;
		BannerBox->SetRenderOpacity(FMath::Max(0.0f, Op));
		if (BannerTime > 2.4f)
		{
			BannerBox->SetVisibility(ESlateVisibility::Collapsed);
			BannerTime = -1.0f;
		}
	}

	// The reels.
	if (bRevealing)
	{
		SpinElapsed += Dt;
		bool bAllStopped = true;
		for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
		{
			TickReel(Reel, Reels[Reel], Dt);
			if (Reels[Reel].State != EReelSpinState::Stopped) { bAllStopped = false; }
		}
		if (bAllStopped)
		{
			FinishReveal();
		}
	}
}

FReply USlotScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		// Leaving mid-spin still pays: settle the pending result first so a
		// nervous Esc can never eat a win (or a trial-crossing win).
		if (bRevealing)
		{
			SlamStop();
			FinishReveal();
		}
		OnClosed.ExecuteIfBound();   // SC1: the ONE close path (cabinet restores input)
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter)
	{
		if (bRevealing)
		{
			SlamStop();   // casino standard: second press lands the reels now
		}
		else
		{
			TrySpin();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USlotScreenWidget::SetTrial(int64 StartCredits, int64 TargetCredits)
{
	Credits = StartCredits;
	CreditsShown = static_cast<double>(StartCredits);
	TrialTarget = TargetCredits;
	bTrialWon = false;
}

void USlotScreenWidget::TrySpin()
{
	if (!bModelReady || bRevealing) { return; }                       // SC7 latch
	if (!Model->IsInFreeSpins() && Credits < TOTAL_BET)               // SC6 guard
	{
		if (HintText)
		{
			HintText->SetText(FText::FromString(TrialTarget > 0
				? TEXT("The shrine keeps its power — Esc, then step in again for a fresh stake")
				: TEXT("Out of credits — Esc to leave (it resets next visit)")));
		}
		return;
	}

	PendingResult = Model->Spin(TOTAL_BET);
	if (!PendingResult.bWasFreeSpin) { Credits -= TOTAL_BET; }        // SC5: model says whether it was free
	CreditsShown = static_cast<double>(Credits);

	bRevealing = true;
	SpinElapsed = 0.0f;
	if (WinText) { WinText->SetText(FText::FromString(TEXT("WIN  —"))); }
	if (LinesText) { LinesText->SetText(FText::GetEmpty()); }
	ClearWinDressing();
	UpdateHud();

	if (!GetWorld())
	{
		// No world (headless oddity) — land instantly, no theater.
		SlamStop();
		FinishReveal();
		return;
	}

	for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
	{
		FSlotReelAnim& R = Reels[Reel];
		R.State = EReelSpinState::Spinning;
		R.Velocity = 0.0f;
		R.StopAt = FIRST_STOP + STOP_STAGGER * Reel;
		R.ShiftsToStop = -1;
		R.BounceTime = 0.0f;
	}

	// The spin whir rides the whole reveal; stopped when the last reel lands.
	StopWhir();
	if (WhirPcm.Num() > 0)
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
		Wave->SetSampleRate(SND_SR);
		Wave->NumChannels = 1;
		Wave->Duration = WhirPcm.Num() / (2.0f * SND_SR);
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(WhirPcm.GetData(), WhirPcm.Num());
		WhirComp = UGameplayStatics::SpawnSound2D(this, Wave, 0.35f);
	}
}

void USlotScreenWidget::TickReel(int32 Reel, FSlotReelAnim& R, float Dt)
{
	switch (R.State)
	{
	case EReelSpinState::Spinning:
	{
		R.Velocity = FMath::Min(SPIN_VMAX, R.Velocity + SPIN_ACCEL * Dt);
		R.Pos += R.Velocity * Dt;
		while (R.Pos >= STEP)
		{
			R.Pos -= STEP;
			ShiftFeed(Reel, NextStripSymbol(Reel, R));
		}
		if (SpinElapsed >= R.StopAt)
		{
			R.State = EReelSpinState::Stopping;
			R.ShiftsToStop = LAND_SHIFTS;
		}
		break;
	}
	case EReelSpinState::Stopping:
	{
		const float T = 1.0f - static_cast<float>(R.ShiftsToStop) / LAND_SHIFTS;
		R.Velocity = FMath::Lerp(SPIN_VMAX, STOP_VEND, T);
		R.Pos += R.Velocity * Dt;
		while (R.Pos >= STEP && R.State == EReelSpinState::Stopping)
		{
			R.Pos -= STEP;
			--R.ShiftsToStop;
			// The last three feeds are the result column: fed bottom-row first,
			// each lands one row lower as the strip keeps scrolling.
			ESlotSymbol Feed;
			switch (R.ShiftsToStop)
			{
			case 3:  Feed = PendingResult.At(Reel, 2); break;
			case 2:  Feed = PendingResult.At(Reel, 1); break;
			case 1:  Feed = PendingResult.At(Reel, 0); break;
			default: Feed = NextStripSymbol(Reel, R); break;
			}
			ShiftFeed(Reel, Feed);
			if (R.ShiftsToStop <= 0)
			{
				R.Pos = 0.0f;
				R.BounceTime = 0.0f;
				R.State = EReelSpinState::Bounce;
				PlayPcm(TickPcm, 0.5f);
				if (Reel == USlotGameModel::REELS - 1) { StopWhir(); }
			}
		}
		break;
	}
	case EReelSpinState::Bounce:
	{
		R.BounceTime += Dt;
		if (R.BounceTime >= BOUNCE_SECS)
		{
			R.State = EReelSpinState::Stopped;
		}
		break;
	}
	default:
		break;
	}

	const float Bounce = (R.State == EReelSpinState::Bounce)
		? BOUNCE_PX * FMath::Sin(PI * FMath::Min(1.0f, R.BounceTime / BOUNCE_SECS))
		: 0.0f;
	if (StripBoxes.IsValidIndex(Reel) && StripBoxes[Reel])
	{
		StripBoxes[Reel]->SetRenderTranslation(FVector2D(0.0f, R.Pos + Bounce));
	}
}

ESlotSymbol USlotScreenWidget::NextStripSymbol(int32 Reel, FSlotReelAnim& R)
{
	// Feed from the model's REAL strip — the blur the player squints at is the
	// actual par sheet scrolling by.
	const TArray<ESlotSymbol>& S = USlotGameModel::Strip(Reel);
	if (S.Num() == 0) { return ESlotSymbol::Star; }
	R.StripCursor = (R.StripCursor + 1) % S.Num();
	return S[R.StripCursor];
}

void USlotScreenWidget::ShiftFeed(int32 Reel, ESlotSymbol NewTop)
{
	FSlotReelAnim& R = Reels[Reel];
	R.Symbols[3] = R.Symbols[2];
	R.Symbols[2] = R.Symbols[1];
	R.Symbols[1] = R.Symbols[0];
	R.Symbols[0] = NewTop;
	RefreshReelImages(Reel);
}

void USlotScreenWidget::SlamStop()
{
	bool bAnyLanded = false;
	for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
	{
		FSlotReelAnim& R = Reels[Reel];
		if (R.State == EReelSpinState::Stopped || R.State == EReelSpinState::Bounce) { continue; }
		R.Symbols[0] = NextStripSymbol(Reel, R);
		R.Symbols[1] = PendingResult.At(Reel, 0);
		R.Symbols[2] = PendingResult.At(Reel, 1);
		R.Symbols[3] = PendingResult.At(Reel, 2);
		RefreshReelImages(Reel);
		R.Pos = 0.0f;
		R.BounceTime = 0.0f;
		R.State = EReelSpinState::Bounce;
		bAnyLanded = true;
	}
	if (bAnyLanded)
	{
		PlayPcm(TickPcm, 0.5f);
		StopWhir();
	}
}

void USlotScreenWidget::FinishReveal()
{
	if (!bRevealing) { return; }   // Esc's settle-then-close can race the tick
	bRevealing = false;

	// Land every reel visually (the Esc path arrives with bounces mid-flight).
	for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
	{
		Reels[Reel].State = EReelSpinState::Stopped;
		if (StripBoxes.IsValidIndex(Reel) && StripBoxes[Reel]) { StripBoxes[Reel]->SetRenderTranslation(FVector2D(0.0f, 0.0f)); }
	}

	Credits += static_cast<int64>(PendingResult.TotalWin);            // SC5: credited exactly once
	if (PendingResult.TotalWin > 0.0)
	{
		// Count the win up over ~0.9 s; pulse the win text; ring the sting.
		CreditRate = FMath::Max(300.0, PendingResult.TotalWin / 0.9);
		WinPulse = 0.0f;
		PlayPcm(WinPcm, 0.45f);
	}
	else
	{
		CreditsShown = static_cast<double>(Credits);
	}

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

	// Payline glow: light every cell of every winning line (leftmost Count
	// reels, rows from the model's own line patterns) — plus the Earths on a
	// bonus, so the scatter moment reads.
	TSet<int32> WinCells;   // visible cells as reel*ROWS + row
	for (const FSlotLineWin& W : PendingResult.LineWins)
	{
		const TArray<int8>& L = USlotGameModel::Line(W.LineIndex);
		for (int32 Reel = 0; Reel < W.Count && Reel < L.Num(); ++Reel)
		{
			WinCells.Add(Reel * USlotGameModel::ROWS + L[Reel]);
		}
	}
	if (PendingResult.bBonusTriggered)
	{
		for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
		{
			for (int32 Row = 0; Row < USlotGameModel::ROWS; ++Row)
			{
				if (PendingResult.At(Reel, Row) == ESlotSymbol::Earth) { WinCells.Add(Reel * USlotGameModel::ROWS + Row); }
			}
		}
	}
	GlowCellIdx.Reset();
	GlowPhase = 0.0f;
	if (WinCells.Num() > 0)
	{
		for (int32 Reel = 0; Reel < USlotGameModel::REELS; ++Reel)
		{
			for (int32 Row = 0; Row < USlotGameModel::ROWS; ++Row)
			{
				const int32 CellIdx = Reel * STRIP_CELLS + (Row + 1);   // visible row -> strip slot
				UImage* Img = Cells.IsValidIndex(CellIdx) ? Cells[CellIdx].Get() : nullptr;
				if (WinCells.Contains(Reel * USlotGameModel::ROWS + Row))
				{
					GlowCellIdx.Add(CellIdx);
					if (Img) { Img->SetRenderOpacity(1.0f); }
				}
				else if (Img)
				{
					Img->SetRenderOpacity(0.45f);   // losers step back so the win reads
				}
			}
		}
	}

	if (PendingResult.bBonusTriggered)
	{
		ShowBanner(FString::Printf(TEXT("✦  %d FREE SPINS — ALL WINS ×%d  ✦"), USlotGameModel::FREE_SPINS_AWARD, USlotGameModel::FREE_SPIN_MULTIPLIER));
		PlayPcm(FanfarePcm, 0.5f);
	}
	UpdateHud();

	// SHRINE TRIAL: target reached — the machine yields exactly once.
	if (TrialTarget > 0 && !bTrialWon && Credits >= TrialTarget)
	{
		bTrialWon = true;
		if (HintText) { HintText->SetText(FText::FromString(TEXT("THE MACHINE YIELDS"))); }
		ShowBanner(TEXT("THE  MACHINE  YIELDS"));
		PlayPcm(FanfarePcm, 0.55f);
		OnTrialWon.ExecuteIfBound();
	}
}

void USlotScreenWidget::SetStripCell(int32 Reel, int32 StripIdx, ESlotSymbol Symbol)
{
	const int32 CellIdx = Reel * STRIP_CELLS + StripIdx;
	UImage* Img = Cells.IsValidIndex(CellIdx) ? Cells[CellIdx].Get() : nullptr;
	if (!Img) { return; }

	if (TObjectPtr<UTexture2D>* Tex = SymbolTextures.Find(Symbol); Tex && *Tex)
	{
		Img->SetBrushFromTexture(*Tex, /*bMatchSize=*/false);
		Img->SetColorAndOpacity(FLinearColor(1, 1, 1, 1));
	}
	else
	{
		// SC4 fallback: a colored block beats an invisible cell.
		Img->SetBrushFromTexture(nullptr);
		Img->SetColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.6f, 1.0f));
	}
}

void USlotScreenWidget::RefreshReelImages(int32 Reel)
{
	for (int32 StripIdx = 0; StripIdx < STRIP_CELLS; ++StripIdx)
	{
		SetStripCell(Reel, StripIdx, Reels[Reel].Symbols[StripIdx]);
	}
}

void USlotScreenWidget::ClearWinDressing()
{
	GlowCellIdx.Reset();
	for (int32 Idx = 0; Idx < Cells.Num(); ++Idx)
	{
		if (Cells[Idx]) { Cells[Idx]->SetRenderOpacity(1.0f); }
		if (CellGlows.IsValidIndex(Idx) && CellGlows[Idx]) { CellGlows[Idx]->SetBrushColor(FLinearColor(0, 0, 0, 0)); }
	}
}

void USlotScreenWidget::ShowBanner(const FString& Msg)
{
	if (!BannerBox || !BannerText) { return; }
	BannerText->SetText(FText::FromString(Msg));
	BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	BannerBox->SetRenderOpacity(1.0f);
	BannerTime = 0.0f;
}

void USlotScreenWidget::StopWhir()
{
	if (WhirComp)
	{
		WhirComp->FadeOut(0.25f, 0.0f);
		WhirComp = nullptr;
	}
}

void USlotScreenWidget::UpdateHud()
{
	if (CreditsText) { CreditsText->SetText(FText::FromString(FString::Printf(TEXT("CREDITS  %lld"), static_cast<int64>(FMath::RoundToDouble(CreditsShown))))); }
	if (FreeSpinsText)
	{
		const int32 FS = Model ? Model->GetFreeSpinsRemaining() : 0;
		FreeSpinsText->SetText(FS > 0
			? FText::FromString(FString::Printf(TEXT("FREE SPINS  %d  (×%d)"), FS, USlotGameModel::FREE_SPIN_MULTIPLIER))
			: FText::FromString(FString::Printf(TEXT("BET  %d"), TOTAL_BET)));
	}
	if (HintText)
	{
		HintText->SetText(FText::FromString(TrialTarget > 0
			? FString::Printf(TEXT("REACH %lld CREDITS TO CLAIM THE POWER        SPACE — spin        Esc — retreat"), TrialTarget)
			: FString(TEXT("SPACE — spin        Esc — leave"))));
	}
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

void USlotScreenWidget::BuildSoundBank()
{
	if (WhirPcm.Num() > 0) { return; }   // built once per widget

	// SPIN WHIR — 4 s of mechanical rumble: low-passed noise + a slowly rising
	// double hum + a 26 Hz tick train (the reel clatter). Faded out on landing.
	{
		FRandomStream Noise(1971);
		const int32 N = SND_SR * 4;
		WhirPcm.Reserve(N * 2);
		float LP = 0.0f;
		for (int32 i = 0; i < N; ++i)
		{
			const float t = static_cast<float>(i) / SND_SR;
			const float n = Noise.FRandRange(-1.0f, 1.0f);
			LP += 0.10f * (n - LP);
			const float Hum = 0.10f * FMath::Sin(2.0f * PI * 82.0f * t * (1.0f + 0.05f * t))
			                + 0.06f * FMath::Sin(2.0f * PI * 164.0f * t * (1.0f + 0.05f * t));
			const float ClatterPhase = FMath::Fmod(t * 26.0f, 1.0f);
			const float Clatter = (ClatterPhase < 0.06f) ? (1.0f - ClatterPhase / 0.06f) : 0.0f;
			const float Env = FMath::Min(1.0f, t / 0.08f);
			AppendSample(WhirPcm, Env * (0.28f * LP + Hum + 0.12f * Clatter * LP));
		}
	}

	// REEL STOP TICK — an 80 ms damped ping with a click of noise at the front.
	{
		FRandomStream Noise(1948);
		const int32 N = static_cast<int32>(SND_SR * 0.08f);
		TickPcm.Reserve(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			const float t = static_cast<float>(i) / SND_SR;
			const float Ping = 0.6f * FMath::Sin(2.0f * PI * 880.0f * t) * FMath::Exp(-t * 45.0f);
			const float Click = 0.35f * Noise.FRandRange(-1.0f, 1.0f) * FMath::Exp(-t * 300.0f);
			AppendSample(TickPcm, Ping + Click);
		}
	}

	// WIN STING — a bright ascending C-major arpeggio (C5 E5 G5 C6).
	{
		const float NoteF[4] = { 523.25f, 659.25f, 783.99f, 1046.50f };
		const float NoteT[4] = { 0.0f, 0.09f, 0.18f, 0.27f };
		const int32 N = static_cast<int32>(SND_SR * 0.85f);
		WinPcm.Reserve(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			const float t = static_cast<float>(i) / SND_SR;
			float S = 0.0f;
			for (int32 k = 0; k < 4; ++k)
			{
				if (t >= NoteT[k])
				{
					const float u = t - NoteT[k];
					const float E = FMath::Exp(-u * 8.0f);
					S += E * (0.28f * FMath::Sin(2.0f * PI * NoteF[k] * u) + 0.10f * FMath::Sin(2.0f * PI * NoteF[k] * 2.0f * u));
				}
			}
			AppendSample(WinPcm, S);
		}
	}

	// BONUS FANFARE — a longer rising run (G4 up to C6) into a held C-major
	// chord. Also the machine-yields sound: the biggest moment gets the most air.
	{
		const float RunF[5] = { 392.00f, 523.25f, 659.25f, 783.99f, 1046.50f };
		const float ChordF[4] = { 523.25f, 659.25f, 783.99f, 1046.50f };
		const float ChordT = 0.62f;
		const int32 N = static_cast<int32>(SND_SR * 1.6f);
		FanfarePcm.Reserve(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			const float t = static_cast<float>(i) / SND_SR;
			float S = 0.0f;
			for (int32 k = 0; k < 5; ++k)
			{
				const float T0 = 0.12f * k;
				if (t >= T0)
				{
					const float u = t - T0;
					S += 0.22f * FMath::Exp(-u * 6.0f) * FMath::Sin(2.0f * PI * RunF[k] * u);
				}
			}
			if (t >= ChordT)
			{
				const float u = t - ChordT;
				const float E = FMath::Exp(-u * 2.2f);
				for (int32 k = 0; k < 4; ++k)
				{
					S += E * 0.14f * FMath::Sin(2.0f * PI * ChordF[k] * u);
				}
			}
			AppendSample(FanfarePcm, S);
		}
	}
}

void USlotScreenWidget::PlayPcm(const TArray<uint8>& Pcm, float Volume)
{
	if (Pcm.Num() == 0 || !GetWorld()) { return; }
	// A fresh procedural wave per play: these waves stream their queue once and
	// are done — tiny buffers, GC'd with the widget.
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(SND_SR);
	Wave->NumChannels = 1;
	Wave->Duration = Pcm.Num() / (2.0f * SND_SR);
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	UGameplayStatics::PlaySound2D(this, Wave, Volume);
}
