// PokerScreenWidget.cpp — SIDE_GAMES G5. See header.

#include "PokerScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "ProgressionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPokerScreen, Log, All);

namespace
{
	constexpr float CARD_W = 150.0f;
	constexpr float CARD_H = 210.0f;
	constexpr int32 SND_SR = 22050;

	const TCHAR* RankGlyph(int32 Rank)
	{
		static const TCHAR* Glyphs[13] = {
			TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"),
			TEXT("9"), TEXT("10"), TEXT("J"), TEXT("Q"), TEXT("K"), TEXT("A")
		};
		return (Rank >= 0 && Rank < 13) ? Glyphs[Rank] : TEXT("?");
	}

	const TCHAR* SuitGlyph(int32 Suit)
	{
		static const TCHAR* Glyphs[4] = { TEXT("♠"), TEXT("♥"), TEXT("♦"), TEXT("♣") };   // ♠ ♥ ♦ ♣
		return (Suit >= 0 && Suit < 4) ? Glyphs[Suit] : TEXT("?");
	}

	void AppendSample(TArray<uint8>& Pcm, float S)
	{
		const int16 V = static_cast<int16>(FMath::Clamp(S, -1.0f, 1.0f) * 32767.0f);
		Pcm.Add(static_cast<uint8>(V & 0xFF));
		Pcm.Add(static_cast<uint8>((V >> 8) & 0xFF));
	}
}

UPokerScreenWidget::UPokerScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UPokerScreenWidget::InitModel(int32 Seed)
{
	if (!Model)
	{
		Model = NewObject<UPokerGameModel>(this);
	}
	Model->Init(Seed);
	bModelReady = true;
	UE_LOG(LogPokerScreen, Display, TEXT("[Poker] model seeded (%d), stake %d"), Seed, Stake);
}

void UPokerScreenWidget::SetStake(int32 SaucePerHand)
{
	Stake = FMath::Max(1, SaucePerHand);
}

TSharedRef<SWidget> UPokerScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PokerRoot"));
		WidgetTree->RootWidget = Canvas;

		// The family cabinet look: gold rim wrapping dark navy.
		UBorder* Gold = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PokerGoldRim"));
		Gold->SetBrushColor(FLinearColor(0.83f, 0.66f, 0.21f, 1.0f));
		Gold->SetPadding(FMargin(6.0f));

		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PokerBg"));
		Bg->SetBrushColor(FLinearColor(0.030f, 0.055f, 0.045f, 0.98f));   // a whisper of felt green
		Bg->SetPadding(FMargin(28.0f, 20.0f));
		Gold->SetContent(Bg);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PokerBox"));
		Bg->SetContent(Box);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PokerTitle"));
		Title->SetText(FText::FromString(TEXT("J A C K S   O R   B E T T E R")));
		Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 36));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.30f, 1.0f)));
		Title->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Title)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 8)); }

		// Paytable — the whole contract, always visible.
		UTextBlock* Paytable = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PokerPaytable"));
		Paytable->SetText(FText::FromString(
			TEXT("ROYAL FLUSH 250     STRAIGHT FLUSH 50     FOUR OF A KIND 25     FULL HOUSE 9     FLUSH 6\n")
			TEXT("STRAIGHT 4     THREE OF A KIND 3     TWO PAIR 2     JACKS OR BETTER 1        (x your bet)")));
		Paytable->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 18));
		Paytable->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.70f, 0.50f, 1.0f)));
		Paytable->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Paytable)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 16)); }

		// The five cards, each with a HELD tag riding above it.
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PokerCards"));
		CardFaces.SetNum(UPokerGameModel::HAND_SIZE);
		CardTexts.SetNum(UPokerGameModel::HAND_SIZE);
		HeldTexts.SetNum(UPokerGameModel::HAND_SIZE);
		for (int32 i = 0; i < UPokerGameModel::HAND_SIZE; ++i)
		{
			UVerticalBox* CardCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

			UTextBlock* Held = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Held->SetText(FText::FromString(TEXT(" ")));
			Held->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
			Held->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.30f, 1.0f)));
			Held->SetJustification(ETextJustify::Center);
			HeldTexts[i] = Held;
			if (UVerticalBoxSlot* HS = CardCol->AddChildToVerticalBox(Held)) { HS->SetHorizontalAlignment(HAlign_Center); HS->SetPadding(FMargin(0, 0, 0, 6)); }

			USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CardSize->SetWidthOverride(CARD_W);
			CardSize->SetHeightOverride(CARD_H);

			UBorder* Face = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Face->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.14f, 1.0f));   // card back until dealt
			Face->SetHorizontalAlignment(HAlign_Center);
			Face->SetVerticalAlignment(VAlign_Center);
			CardFaces[i] = Face;
			CardSize->AddChild(Face);

			UTextBlock* CardText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			CardText->SetText(FText::FromString(TEXT("✦")));   // ✦ on the back
			CardText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 44));
			CardText->SetColorAndOpacity(FSlateColor(FLinearColor(0.83f, 0.66f, 0.21f, 1.0f)));
			CardText->SetJustification(ETextJustify::Center);
			CardTexts[i] = CardText;
			Face->SetContent(CardText);

			CardCol->AddChildToVerticalBox(CardSize);
			if (UHorizontalBoxSlot* CS = CardRow->AddChildToHorizontalBox(CardCol)) { CS->SetPadding(FMargin(9, 0)); }
		}
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(CardRow)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0, 0, 0, 12)); }

		// Result line (the hand's name, big).
		ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PokerResult"));
		ResultText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 30));
		ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 1.0f, 0.65f, 1.0f)));
		ResultText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(ResultText)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 10)); }

		// HUD row: SAUCE | BET | WIN.
		UHorizontalBox* Hud = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PokerHud"));
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
		SauceText = MakeHudText(TEXT("PokerSauce"));
		BetText = MakeHudText(TEXT("PokerBet"));
		WinText = MakeHudText(TEXT("PokerWin"));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Hud)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 10)); }

		// Hint (the key summary).
		HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PokerHint"));
		HintText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 22));
		HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.70f, 0.68f, 1.0f)));
		HintText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(HintText)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0, 0, 0, 12)); }

		// The lesson block (Walt: a child could play it). HOW TO PLAY between
		// hands; while cards are up, what's worth keeping + the house's advice.
		LessonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PokerLesson"));
		LessonText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
		LessonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.78f, 0.60f, 1.0f)));
		LessonText->SetJustification(ETextJustify::Center);
		LessonText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(LessonText)) { S->SetHorizontalAlignment(HAlign_Fill); }

		// SC3: stretch anchors, never point-anchor + SetSize.
		if (UCanvasPanelSlot* CSlot = Canvas->AddChildToCanvas(Gold))
		{
			CSlot->SetAnchors(FAnchors(0.18f, 0.08f, 0.82f, 0.92f));
			CSlot->SetOffsets(FMargin(0.0f));
		}
	}

	return Super::RebuildWidget();
}

void UPokerScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildSoundBank();
	if (ResultText) { ResultText->SetText(FText::GetEmpty()); }
	if (WinText) { WinText->SetText(FText::FromString(TEXT("WIN  —"))); }
	UpdateHud();
	UpdateLesson();
}

FReply UPokerScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		// Settle first: leaving mid-hand auto-draws with current holds so the
		// bet is never eaten (the slot facelift's rule).
		if (Phase == EPokerPhase::Holding)
		{
			DoDraw();
		}
		OnClosed.ExecuteIfBound();
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter)
	{
		if (Phase == EPokerPhase::Ready) { TryDeal(); }
		else { DoDraw(); }
		return FReply::Handled();
	}
	if (Phase == EPokerPhase::Holding)
	{
		if (Key == EKeys::One)   { ToggleHold(0); return FReply::Handled(); }
		if (Key == EKeys::Two)   { ToggleHold(1); return FReply::Handled(); }
		if (Key == EKeys::Three) { ToggleHold(2); return FReply::Handled(); }
		if (Key == EKeys::Four)  { ToggleHold(3); return FReply::Handled(); }
		if (Key == EKeys::Five)  { ToggleHold(4); return FReply::Handled(); }
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPokerScreenWidget::TryDeal()
{
	if (!bModelReady) { return; }

	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (Progression && !Progression->TrySpendSauce(Stake))
	{
		if (HintText)
		{
			HintText->SetText(FText::FromString(FString::Printf(
				TEXT("A hand costs %d sauce — you carry %d. The fountain, the books, the Refusers."),
				Stake, Progression->GetSauce())));
		}
		return;
	}
	// No progression subsystem (bare test worlds): play free, same as the carousel.

	Hand = Model->Deal();
	HoldMask = 0;
	Phase = EPokerPhase::Holding;
	for (int32 i = 0; i < Hand.Num(); ++i) { ShowCard(i, Hand[i]); }
	RefreshHolds();
	if (ResultText) { ResultText->SetText(FText::GetEmpty()); }
	if (WinText) { WinText->SetText(FText::FromString(TEXT("WIN  —"))); }
	PlayPcm(DealPcm, 0.45f);
	UpdateHud();
	UpdateLesson();
}

void UPokerScreenWidget::DoDraw()
{
	if (!bModelReady || Phase != EPokerPhase::Holding) { return; }

	const FPokerHandResult Result = Model->Draw(HoldMask);
	Hand = Result.Cards;
	Phase = EPokerPhase::Ready;
	for (int32 i = 0; i < Hand.Num(); ++i) { ShowCard(i, Hand[i]); }
	HoldMask = 0;
	RefreshHolds();

	const int32 Win = Result.PayMultiplier * Stake;
	if (Win > 0)
	{
		if (UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this))
		{
			Progression->GrantSauce(Win);
		}
		if (ResultText)
		{
			ResultText->SetText(FText::FromString(FString::Printf(TEXT("%s  —  +%d SAUCE"),
				UPokerGameModel::RankDisplayName(Result.Rank), Win)));
			ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 1.0f, 0.65f, 1.0f)));
		}
		PlayPcm(WinPcm, 0.45f);
	}
	else if (ResultText)
	{
		ResultText->SetText(FText::FromString(TEXT("NOTHING — the house nods politely")));
		ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.65f, 1.0f)));
	}
	if (WinText) { WinText->SetText(FText::FromString(FString::Printf(TEXT("WIN  %d"), Win))); }
	UpdateHud();
	UpdateLesson();
}

void UPokerScreenWidget::ToggleHold(int32 Index)
{
	HoldMask ^= (1 << Index);
	RefreshHolds();
}

void UPokerScreenWidget::ShowCard(int32 Index, int32 Card)
{
	if (!CardFaces.IsValidIndex(Index) || !CardFaces[Index] || !CardTexts[Index]) { return; }
	const int32 Rank = UPokerGameModel::RankOf(Card);
	const int32 Suit = UPokerGameModel::SuitOf(Card);
	CardFaces[Index]->SetBrushColor(FLinearColor(0.94f, 0.93f, 0.87f, 1.0f));   // ivory card stock
	CardTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s%s"), RankGlyph(Rank), SuitGlyph(Suit))));
	const bool bRed = (Suit == 1 || Suit == 2);   // hearts, diamonds
	CardTexts[Index]->SetColorAndOpacity(FSlateColor(bRed
		? FLinearColor(0.72f, 0.08f, 0.10f, 1.0f)
		: FLinearColor(0.07f, 0.07f, 0.10f, 1.0f)));
}

void UPokerScreenWidget::ShowCardBack(int32 Index)
{
	if (!CardFaces.IsValidIndex(Index) || !CardFaces[Index] || !CardTexts[Index]) { return; }
	CardFaces[Index]->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.14f, 1.0f));
	CardTexts[Index]->SetText(FText::FromString(TEXT("✦")));
	CardTexts[Index]->SetColorAndOpacity(FSlateColor(FLinearColor(0.83f, 0.66f, 0.21f, 1.0f)));
}

void UPokerScreenWidget::RefreshHolds()
{
	for (int32 i = 0; i < HeldTexts.Num(); ++i)
	{
		if (HeldTexts[i])
		{
			HeldTexts[i]->SetText(FText::FromString((HoldMask & (1 << i)) ? TEXT("HELD") : TEXT(" ")));
		}
	}
}

void UPokerScreenWidget::UpdateHud()
{
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (SauceText) { SauceText->SetText(FText::FromString(FString::Printf(TEXT("SAUCE  %d"), Progression ? Progression->GetSauce() : 0))); }
	if (BetText) { BetText->SetText(FText::FromString(FString::Printf(TEXT("BET  %d"), Stake))); }
	if (HintText)
	{
		HintText->SetText(FText::FromString(Phase == EPokerPhase::Holding
			? TEXT("[1]-[5] hold a card        SPACE — draw        Esc — leave")
			: FString::Printf(TEXT("SPACE — deal a hand (%d sauce)        Esc — leave"), Stake)));
	}
}

void UPokerScreenWidget::UpdateLesson()
{
	if (!LessonText) { return; }

	if (Phase == EPokerPhase::Holding)
	{
		// The live coach: what's worth keeping, and the house's exact advice —
		// UPokerGameModel::SuggestHoldMask, the same strategy the smoke test
		// measured the 97% RTP under. Following the advice IS that RTP.
		const int32 Suggest = UPokerGameModel::SuggestHoldMask(Hand);
		FString CardNames, KeyNames;
		for (int32 i = 0; i < Hand.Num(); ++i)
		{
			if (Suggest & (1 << i))
			{
				if (!CardNames.IsEmpty()) { CardNames += TEXT("  "); KeyNames += TEXT(", "); }
				CardNames += FString::Printf(TEXT("%s%s"),
					RankGlyph(UPokerGameModel::RankOf(Hand[i])), SuitGlyph(UPokerGameModel::SuitOf(Hand[i])));
				KeyNames += FString::Printf(TEXT("%d"), i + 1);
			}
		}
		const FString Advice = (Suggest != 0)
			? FString::Printf(TEXT("The house suggests: keep %s — press %s.  Then SPACE swaps the rest."), *CardNames, *KeyNames)
			: FString(TEXT("The house suggests: keep nothing — just press SPACE for five fresh cards."));
		LessonText->SetText(FText::FromString(FString::Printf(
			TEXT("Worth keeping: two cards with the same number · four cards of one symbol · any J, Q, K or A.\n%s"),
			*Advice)));
	}
	else
	{
		LessonText->SetText(FText::FromString(FString::Printf(
			TEXT("HOW TO PLAY\n")
			TEXT("1.  Press SPACE — five cards appear (a hand costs %d sauce).\n")
			TEXT("2.  Keep the good cards: press 1-5 to mark them HELD (press again to unmark).\n")
			TEXT("3.  Press SPACE — the unmarked cards are swapped. Your final five cards win by the table at the top."),
			Stake)));
	}
}

void UPokerScreenWidget::BuildSoundBank()
{
	if (DealPcm.Num() > 0) { return; }

	// DEAL — five quick card snaps (a 60 ms noise-thwip each, staggered).
	{
		FRandomStream Noise(1952);
		const float SnapGap = 0.07f;
		const int32 N = static_cast<int32>(SND_SR * (SnapGap * 5 + 0.08f));
		DealPcm.Reserve(N * 2);
		for (int32 i = 0; i < N; ++i)
		{
			const float t = static_cast<float>(i) / SND_SR;
			float S = 0.0f;
			for (int32 k = 0; k < 5; ++k)
			{
				const float T0 = SnapGap * k;
				if (t >= T0)
				{
					const float u = t - T0;
					S += 0.5f * Noise.FRandRange(-1.0f, 1.0f) * FMath::Exp(-u * 120.0f)
					   + 0.25f * FMath::Sin(2.0f * PI * 660.0f * u) * FMath::Exp(-u * 60.0f);
				}
			}
			AppendSample(DealPcm, S);
		}
	}

	// WIN — the family sting (ascending C-major arpeggio, the slot's voice).
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
					S += FMath::Exp(-u * 8.0f) * (0.28f * FMath::Sin(2.0f * PI * NoteF[k] * u) + 0.10f * FMath::Sin(2.0f * PI * NoteF[k] * 2.0f * u));
				}
			}
			AppendSample(WinPcm, S);
		}
	}
}

void UPokerScreenWidget::PlayPcm(const TArray<uint8>& Pcm, float Volume)
{
	if (Pcm.Num() == 0 || !GetWorld()) { return; }
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(SND_SR);
	Wave->NumChannels = 1;
	Wave->Duration = Pcm.Num() / (2.0f * SND_SR);
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	UGameplayStatics::PlaySound2D(this, Wave, Volume);
}
