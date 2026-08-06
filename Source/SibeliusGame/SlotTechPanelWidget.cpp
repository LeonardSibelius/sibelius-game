// SlotTechPanelWidget.cpp — see header.

#include "SlotTechPanelWidget.h"

#include "SlotGameModel.h"
#include "ProgressionSubsystem.h"   // the saved dials live in the progression save

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FLinearColor ColPanel   (0.02f, 0.03f, 0.05f, 0.94f);
	const FLinearColor ColTitle   (1.00f, 0.85f, 0.20f, 1.0f);   // the cabinet's gold
	const FLinearColor ColBody    (0.85f, 0.90f, 0.95f, 1.0f);

	// The key line gets its OWN strip and near-white text. The report can grow taller
	// than the panel — the help page especially — and when it does, the last row spills
	// past the panel's background onto the cathedral behind it, where grey-on-stone is
	// unreadable. An opaque strip keeps the controls legible however tall the body runs.
	const FLinearColor ColFooter  (0.92f, 0.94f, 0.97f, 1.0f);
	const FLinearColor ColFooterBg(0.16f, 0.18f, 0.22f, 1.0f);

	FString SymbolName(ESlotSymbol S)
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
		case ESlotSymbol::Wild:   return TEXT("WILD");
		case ESlotSymbol::Earth:  return TEXT("Earth");
		default:                  return TEXT("?");
		}
	}

	/** Left-pad to a column width so the report lines up in a proportional font. */
	FString Pad(const FString& S, int32 Width)
	{
		FString Out = S;
		while (Out.Len() < Width) { Out.AppendChar(' '); }
		return Out;
	}
}

void USlotTechPanelWidget::Setup(USlotGameModel* InModel)
{
	Model = InModel;

	// FACTORY IS ALWAYS THE SHIPPED SHEET, never the model's current one. If it took the
	// model's sheet, opening the panel on an already-edited machine would enshrine that
	// edit as "factory" and REVERT would return to it instead of to the real default.
	Factory = FSlotParSheet::CelestialFortune();

	// Factory positions for the dials...
	PaysMultiplier = 1.0;
	WildCount  = Factory.CountOf(ESlotSymbol::Wild);
	BonusCount = Factory.CountOf(ESlotSymbol::Earth);
	for (const FSlotPayRow& Row : Factory.PayTable)
	{
		if (Row.Symbol == ESlotSymbol::Seven) { JackpotPay = Row.Pay5; }
	}

	// ...then whatever the player last left them at, CLAMPED. A save written before a
	// range changed must be pulled into legal bounds, never trusted blindly.
	if (const UProgressionSubsystem* Prog = UProgressionSubsystem::Get(this))
	{
		const FProgressionState& S = Prog->GetStateForRead();
		if (S.SlotPaysMultiplier > 0.0f) { PaysMultiplier = FMath::Clamp<double>(S.SlotPaysMultiplier, 0.70, 1.30); }
		if (S.SlotWildCount     >= 0)    { WildCount      = FMath::Clamp(S.SlotWildCount, 0, 3); }
		if (S.SlotJackpotPay    > 0.0f)  { JackpotPay     = FMath::Clamp<double>(S.SlotJackpotPay, 250.0, 4000.0); }
		if (S.SlotBonusCount    >= 0)    { BonusCount     = FMath::Clamp(S.SlotBonusCount, 1, 4); }
	}
}

FSlotParSheet USlotTechPanelWidget::ComposeFromDials(float PaysMultiplier, int32 WildCount,
	float JackpotPay, int32 BonusCount)
{
	FSlotParSheet P = FSlotParSheet::CelestialFortune();

	// WILDS — swap Star stops for Wild, or back. Star is the filler symbol, and swapping
	// rather than inserting keeps the strip LENGTH fixed: change the length and every
	// other symbol's odds move too, which would make the dial impossible to reason about.
	int32 Have = P.CountOf(ESlotSymbol::Wild);
	for (int32 i = 0; i < P.Strip.Num() && Have < WildCount; ++i)
	{
		if (P.Strip[i] == ESlotSymbol::Star) { P.Strip[i] = ESlotSymbol::Wild; ++Have; }
	}
	for (int32 i = 0; i < P.Strip.Num() && Have > WildCount; ++i)
	{
		if (P.Strip[i] == ESlotSymbol::Wild) { P.Strip[i] = ESlotSymbol::Star; --Have; }
	}

	// BONUS — the same trade against Moon, the other filler. Earth never pays on a line,
	// so this dial moves the free-spin RHYTHM without touching what any symbol is worth.
	int32 Earths = P.CountOf(ESlotSymbol::Earth);
	for (int32 i = 0; i < P.Strip.Num() && Earths < BonusCount; ++i)
	{
		if (P.Strip[i] == ESlotSymbol::Moon) { P.Strip[i] = ESlotSymbol::Earth; ++Earths; }
	}
	for (int32 i = 0; i < P.Strip.Num() && Earths > BonusCount; ++i)
	{
		if (P.Strip[i] == ESlotSymbol::Earth) { P.Strip[i] = ESlotSymbol::Moon; --Earths; }
	}

	// PAYS — scale the whole paytable. RTP moves in proportion; volatility and hit
	// frequency barely stir. The one clean cause-and-effect on the panel.
	for (FSlotPayRow& Row : P.PayTable)
	{
		Row.Pay3 *= PaysMultiplier;
		Row.Pay4 *= PaysMultiplier;
		Row.Pay5 *= PaysMultiplier;
	}

	// JACKPOT — set LAST and NOT scaled, so the number on the dial is the number that
	// pays. Scaling it too would mean the dial says 4000 and the machine pays 4800.
	for (FSlotPayRow& Row : P.PayTable)
	{
		if (Row.Symbol == ESlotSymbol::Seven) { Row.Pay5 = JackpotPay; }
	}

	return P;
}

FSlotParSheet USlotTechPanelWidget::ComposeSavedParSheet(const UObject* WorldContext)
{
	const FSlotParSheet FactorySheet = FSlotParSheet::CelestialFortune();

	const UProgressionSubsystem* Prog = UProgressionSubsystem::Get(WorldContext);
	if (!Prog || !Prog->GetStateForRead().HasEditedParSheet())
	{
		return FactorySheet;   // untouched — the shipped machine
	}

	const FProgressionState& S = Prog->GetStateForRead();
	return ComposeFromDials(
		(S.SlotPaysMultiplier > 0.0f) ? FMath::Clamp(S.SlotPaysMultiplier, 0.70f, 1.30f) : 1.0f,
		(S.SlotWildCount     >= 0)    ? FMath::Clamp(S.SlotWildCount, 0, 3)              : FactorySheet.CountOf(ESlotSymbol::Wild),
		(S.SlotJackpotPay    > 0.0f)  ? FMath::Clamp(S.SlotJackpotPay, 250.0f, 4000.0f)  : 1000.0f,
		(S.SlotBonusCount    >= 0)    ? FMath::Clamp(S.SlotBonusCount, 1, 4)             : FactorySheet.CountOf(ESlotSymbol::Earth));
}

void USlotTechPanelWidget::PersistDials() const
{
	if (UProgressionSubsystem* Prog = UProgressionSubsystem::Get(this))
	{
		Prog->SaveSlotDials(static_cast<float>(PaysMultiplier), WildCount,
			static_cast<float>(JackpotPay), BonusCount);
	}
}

TSharedRef<SWidget> USlotTechPanelWidget::RebuildWidget()
{
	BuildTree();   // must happen before Slate takes the tree — see the header
	return Super::RebuildWidget();
}

void USlotTechPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);   // required to receive keys in UIOnly, same as the screen
	Refresh();              // the tree already exists by now
}

void USlotTechPanelWidget::BuildTree()
{
	if (!WidgetTree || Panel)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TechRoot"));
	WidgetTree->RootWidget = Root;

	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TechPanel"));
	Panel->SetBrushColor(ColPanel);
	Panel->SetPadding(FMargin(34.f, 26.f));

	if (UCanvasPanelSlot* CS = Root->AddChildToCanvas(Panel))
	{
		// Stretch anchors with zero offsets — the robust layout the other screens use;
		// a point anchor plus SetSize has produced zero-size boxes in this project.
		// Stops at 0.88 to leave the footer its own band at the bottom.
		CS->SetAnchors(FAnchors(0.14f, 0.03f, 0.86f, 0.88f));
		CS->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TechBox"));
	Panel->SetContent(Box);

	auto MakeText = [&](const TCHAR* Name, const FLinearColor& Col, int32 Size) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo F = T->GetFont();
		F.Size = Size;
		T->SetFont(F);
		T->SetColorAndOpacity(FSlateColor(Col));
		if (UVerticalBoxSlot* VS = Box->AddChildToVerticalBox(T))
		{
			VS->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
		return T;
	};

	TitleText = MakeText(TEXT("TechTitle"), ColTitle, 30);

	// The body lives in a ScrollBox that FILLS the remaining height, so a long page
	// scrolls instead of running off the bottom of the panel.
	BodyScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TechBodyScroll"));
	if (UVerticalBoxSlot* VS = Box->AddChildToVerticalBox(BodyScroll))
	{
		VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechBody"));
	{
		FSlateFontInfo F = BodyText->GetFont();
		F.Size = 18;
		BodyText->SetFont(F);
		BodyText->SetColorAndOpacity(FSlateColor(ColBody));
	}
	BodyScroll->AddChild(BodyText);

	// THE FOOTER IS ANCHORED TO THE CANVAS, NOT PACKED INTO THE VERTICAL BOX.
	//
	// Inside the box it was a sibling of the report, so a long body simply pushed it
	// down — off the bottom of the screen on the help page. Anchored to its own band at
	// the foot of the canvas it cannot be displaced by anything above it, however much
	// the report grows.
	UBorder* FooterBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TechFooterBg"));
	FooterBg->SetBrushColor(ColFooterBg);
	FooterBg->SetPadding(FMargin(16.f, 9.f));
	if (UCanvasPanelSlot* FS = Root->AddChildToCanvas(FooterBg))
	{
		FS->SetAnchors(FAnchors(0.14f, 0.895f, 0.86f, 0.955f));
		FS->SetOffsets(FMargin(0.f));
	}

	FooterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechFooter"));
	{
		FSlateFontInfo F = FooterText->GetFont();
		F.Size = 17;
		FooterText->SetFont(F);
		FooterText->SetColorAndOpacity(FSlateColor(ColFooter));
	}
	FooterBg->SetContent(FooterText);

	// Footer text itself is set by Refresh(), which knows if the help page is showing.
}

FSlotParSheet USlotTechPanelWidget::ComposeParSheet() const
{
	FSlotParSheet P = ComposeFromDials(static_cast<float>(PaysMultiplier), WildCount,
		static_cast<float>(JackpotPay), BonusCount);
	P.Name = TEXT("Celestial Fortune (edited)");
	return P;
}

void USlotTechPanelWidget::AdjustSelected(int32 Direction)
{
	switch (Selected)
	{
	case EKnob::Pays:
		PaysMultiplier = FMath::Clamp(PaysMultiplier + 0.05 * Direction, 0.70, 1.30);
		break;
	case EKnob::Wilds:
		WildCount = FMath::Clamp(WildCount + Direction, 0, 3);
		break;
	case EKnob::Jackpot:
		JackpotPay = FMath::Clamp(JackpotPay + 250.0 * Direction, 250.0, 4000.0);
		break;
	case EKnob::Bonus:
		BonusCount = FMath::Clamp(BonusCount + Direction, 1, 4);
		break;
	default:
		break;
	}
	PersistDials();
	Refresh();
}

void USlotTechPanelWidget::RevertToFactory()
{
	PaysMultiplier = 1.0;
	WildCount  = Factory.CountOf(ESlotSymbol::Wild);
	BonusCount = Factory.CountOf(ESlotSymbol::Earth);
	JackpotPay = 1000.0;
	for (const FSlotPayRow& Row : Factory.PayTable)
	{
		if (Row.Symbol == ESlotSymbol::Seven) { JackpotPay = Row.Pay5; }
	}

	// Forget the edit entirely rather than saving the factory numbers back. The machine
	// then follows whatever the CURRENT build ships, so a later retune reaches players
	// who reverted instead of pinning them to today's values.
	if (UProgressionSubsystem* Prog = UProgressionSubsystem::Get(this))
	{
		Prog->ClearSlotDials();
	}
	Refresh();
}

void USlotTechPanelWidget::Refresh()
{
	const FSlotParSheet Sheet = ComposeParSheet();

	// Exact maths, then the two quantities that are cheaper to measure than to derive.
	FSlotParSheetReport Rep = SlotParSheetMath::Analyze(Sheet);
	SlotParSheetMath::MeasureBySimulation(Sheet, Rep, 8000);

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(bShowHelp
			? TEXT("SERVICE PANEL   —   WHAT ALL THIS MEANS")
			: TEXT("SERVICE PANEL   —   CELESTIAL FORTUNE")));
	}
	if (BodyText)
	{
		// Both pages read at 18 now — the ScrollBox handles the overflow, so there is no
		// reason to shrink the help text to the edge of legibility.
		FSlateFontInfo F = BodyText->GetFont();
		F.Size = 18;
		BodyText->SetFont(F);

		BodyText->SetText(FText::FromString(bShowHelp ? ComposeHelpText() : ComposeReportText(Rep, Sheet)));
	}
	if (FooterText)
	{
		FooterText->SetText(FText::FromString(bShowHelp
			? TEXT("[UP/DOWN] scroll     [H] back to the machine     [ESC] close")
			: TEXT("[UP/DOWN] choose a dial   [LEFT/RIGHT] turn it   [H] what this means   [R] revert   [ESC] close")));
	}

	// Push the edit to the machine immediately. Editing is free and reversible by
	// design — the licence check is what stops an unplayable machine from running, not
	// a confirmation step.
	if (Model)
	{
		Model->SetParSheet(Sheet);
	}
}

FString USlotTechPanelWidget::ComposeReportText(const FSlotParSheetReport& Rep, const FSlotParSheet& Sheet) const
{
	auto Marker = [this](EKnob K) { return (Selected == K) ? TEXT(">") : TEXT(" "); };

	FString S;

	// --- the three dials ---
	S += FString::Printf(TEXT("%s  PAYS          x %.2f        the whole paytable\n"),
		Marker(EKnob::Pays), PaysMultiplier);
	S += FString::Printf(TEXT("%s  WILDS           %d of %d stops\n"),
		Marker(EKnob::Wilds), WildCount, Sheet.NumStops());
	S += FString::Printf(TEXT("%s  JACKPOT         %.0f          Lucky 7, five of a kind\n"),
		Marker(EKnob::Jackpot), JackpotPay);
	S += FString::Printf(TEXT("%s  BONUS           %d of %d stops    Earth — the free-spin round\n"),
		Marker(EKnob::Bonus), BonusCount, Sheet.NumStops());

	S += TEXT("\n");
	S += TEXT("--------------------------------------------------------------\n");

	// --- the licence lamp ---
	const bool bLicensed = SlotLicence::IsLicensed(Rep.RtpPercent) && Rep.bConverged;
	if (bLicensed)
	{
		S += TEXT("   LICENSED — the house will run this machine\n");
	}
	else if (!Rep.bConverged)
	{
		S += TEXT("   UNLICENSED — free spins never stop. This is not a machine.\n");
	}
	else
	{
		const bool bTooGenerous = Rep.RtpPercent > SlotLicence::MaxRtpPercent;
		const double Limit = bTooGenerous ? SlotLicence::MaxRtpPercent : SlotLicence::MinRtpPercent;

		S += bTooGenerous
			? FString::Printf(TEXT("   UNLICENSED — pays %.2f%%, above the %.0f%% ceiling.\n")
			                  TEXT("   The house won't take it: it doesn't earn.\n"),
				Rep.RtpPercent, Limit)
			: FString::Printf(TEXT("   UNLICENSED — pays %.2f%%, below the %.0f%% floor.\n")
			                  TEXT("   No jurisdiction licenses it.\n"),
				Rep.RtpPercent, Limit);

		// TELL THEM THE MOVE, not just the verdict.
		//
		// PAYS scales the whole paytable, so RTP moves in proportion to it — which makes
		// the multiplier that lands exactly on the limit a straight ratio. Naming it is
		// the difference between a rule and a teacher: "wrong" becomes "here is the fix,
		// and here is why that dial is the one that fixes it."
		//
		// Only offered when the answer is inside the dial's range. Suggesting x0.4 when
		// the dial stops at 0.70 would be worse than saying nothing.
		if (Rep.RtpPercent > 0.0)
		{
			const double Needed = PaysMultiplier * (Limit / Rep.RtpPercent);
			if (Needed >= 0.70 && Needed <= 1.30)
			{
				S += FString::Printf(TEXT("   PAYS x %.2f would bring it inside.\n"), Needed);
			}
			else if (bTooGenerous)
			{
				// Beyond what PAYS alone can claw back — the wilds are usually the culprit.
				S += TEXT("   Too far for the PAYS dial alone — try fewer WILDS as well.\n");
			}
			else
			{
				S += TEXT("   Too far for the PAYS dial alone — try more WILDS as well.\n");
			}
		}
	}

	S += TEXT("--------------------------------------------------------------\n\n");

	// --- the numbers ---
	S += FString::Printf(TEXT("   RTP  %7.3f %%        HOLD  %6.3f %%\n"), Rep.RtpPercent, Rep.HoldPercent);
	S += FString::Printf(TEXT("   Hit frequency  %5.2f %%     Volatility  %s\n"),
		Rep.HitFrequencyPercent, *Rep.VolatilityWord());
	S += FString::Printf(TEXT("   Bonus  1 in %.0f spins      Free spins  %.3f per spin\n"),
		(Rep.TriggerPercent > 0.0) ? (100.0 / Rep.TriggerPercent) : 0.0, Rep.FreeSpinsPerBaseSpin);
	S += FString::Printf(TEXT("   Base game %.2f %%  +  free spins %.2f %%\n\n"),
		Rep.BaseRtpPercent, Rep.RtpPercent - Rep.BaseRtpPercent);

	// --- what that actually means at the machine ---
	S += ComposeFeelText(Rep);
	S += TEXT("\n");

	// --- the anatomy: WHERE the return comes from ---
	// This is the part that turns a dial into a lesson. A total tells the player what
	// happened; this tells them why.
	S += TEXT("   WHERE THE RETURN COMES FROM   (base game)\n");
	int32 Shown = 0;
	for (const FSlotSymbolContribution& C : Rep.Contributions)
	{
		if (C.BaseRtpPercent <= 0.0001 || Shown >= 6)
		{
			continue;
		}
		S += FString::Printf(TEXT("     %s %2d stops   %6.2f %% of RTP\n"),
			*Pad(SymbolName(C.Symbol), 9), C.StripCount, C.BaseRtpPercent);
		++Shown;
	}

	return S;
}

FString USlotTechPanelWidget::ComposeFeelText(const FSlotParSheetReport& Rep) const
{
	FString S;
	S += TEXT("   WHAT THIS MACHINE WILL FEEL LIKE\n");

	if (!Rep.bConverged)
	{
		S += TEXT("     It would never stop paying free spins. This is not a machine.\n");
		return S;
	}

	// RTP, said the way a person would say it.
	S += FString::Printf(
		TEXT("     Bet 100 credits over a long evening and about %.0f come back.\n")
		TEXT("     The machine keeps roughly %.0f of every 100.\n"),
		Rep.RtpPercent, FMath::Max(0.0, Rep.HoldPercent));

	// Hit frequency as a fraction people can picture.
	if (Rep.HitFrequencyPercent > 0.0)
	{
		const double OneIn = 100.0 / Rep.HitFrequencyPercent;
		S += FString::Printf(TEXT("     About 1 spin in %.1f pays something, however small.\n"), OneIn);
	}

	// Volatility is the one that decides whether a session is pleasant or brutal, and
	// it is invisible in the RTP. Spell it out.
	const FString Vol = Rep.VolatilityWord();
	if (Vol == TEXT("LOW"))
	{
		S += TEXT("     Wins are frequent and small — your credits drift, they don't swing.\n");
	}
	else if (Vol == TEXT("MEDIUM"))
	{
		S += TEXT("     A steady mix: mostly small wins, with the occasional real one.\n");
	}
	else if (Vol == TEXT("HIGH"))
	{
		S += TEXT("     Long dry spells broken by big wins. It will test your nerve.\n");
	}
	else if (Vol == TEXT("WILD"))
	{
		S += TEXT("     Savage swings. Most sessions die quietly; a few pay enormously.\n");
	}

	if (Rep.TriggerPercent > 0.0)
	{
		S += FString::Printf(TEXT("     The free-spin round arrives about every %.0f spins.\n"),
			100.0 / Rep.TriggerPercent);
	}
	else
	{
		S += TEXT("     There is no free-spin round at all on these settings.\n");
	}

	return S;
}

FString USlotTechPanelWidget::ComposeHelpText() const
{
	// Written for someone who has never been in a casino. No jargon that is not
	// immediately defined, and every idea tied back to a dial on this panel.
	return FString(
		TEXT("   WHAT IS A PAR SHEET?\n")
		TEXT("     A slot machine is really just a few dozen numbers: what is painted on\n")
		TEXT("     the reels, and what each combination pays. That list is the par sheet.\n")
		TEXT("     There is no other secret. The outcome is decided the instant you press\n")
		TEXT("     spin; the reels turning is theatre.\n")
		TEXT("\n")
		TEXT("   RTP  —  return to player\n")
		TEXT("     Of every 100 credits bet, how many go back to players over a very long\n")
		TEXT("     run. It does NOT promise what happens tonight. HOLD is the same number\n")
		TEXT("     from the casino's side: what the machine earns.\n")
		TEXT("\n")
		TEXT("   HIT FREQUENCY\n")
		TEXT("     How often a spin pays ANYTHING, even a few credits. A machine can feel\n")
		TEXT("     generous on a high hit frequency while still keeping your money.\n")
		TEXT("\n")
		TEXT("   VOLATILITY\n")
		TEXT("     How rough the ride is. Two machines with the SAME RTP can feel nothing\n")
		TEXT("     alike: one drips small wins, the other starves you then pays big. This\n")
		TEXT("     is the difference the JACKPOT dial makes.\n")
		TEXT("\n")
		TEXT("   THE THREE DIALS\n")
		TEXT("     PAYS      scales every payout at once. The clean one — RTP moves with\n")
		TEXT("               it and almost nothing else does.\n")
		TEXT("     WILDS     how many WILD symbols sit on the reel strip. A Wild stands in\n")
		TEXT("               for any symbol. One stop in forty is worth more than you think.\n")
		TEXT("     JACKPOT   what five Lucky 7s pay. Barely touches RTP; transforms feel.\n")
		TEXT("     BONUS     how many EARTH symbols are on the strip. Earth never pays on\n")
		TEXT("               a line — three anywhere start the free spins. This dial sets\n")
		TEXT("               the machine's RHYTHM: how often the good part arrives.\n")
		TEXT("\n")
		TEXT("   WHY THE HOUSE HAS RULES\n")
		TEXT("     Pay out too much and no casino will run your machine — it does not earn\n")
		TEXT("     its floor space. Pay too little and no regulator will license it. Every\n")
		TEXT("     real designer works inside that band, and so do you: outside it, this\n")
		TEXT("     machine refuses to spin.\n")
		TEXT("\n")
		TEXT("   WHERE THE RETURN COMES FROM\n")
		TEXT("     The list on the main page shows which symbols actually pay for the\n")
		TEXT("     machine. Turn a dial and watch which lines move. That is the whole job.\n"));
}

FReply USlotTechPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::H)
	{
		bShowHelp = !bShowHelp;
		if (BodyScroll) { BodyScroll->SetScrollOffset(0.f); }   // always start at the top
		Refresh();
		return FReply::Handled();
	}

	// On the help page the arrows are free — no dials to drive — so they scroll. The
	// mouse wheel works too, but a reader whose hands are on the keyboard should not
	// have to reach for the mouse to see the last paragraph.
	if (bShowHelp && BodyScroll && (Key == EKeys::Up || Key == EKeys::Down ||
	                                Key == EKeys::PageUp || Key == EKeys::PageDown))
	{
		const float Step = (Key == EKeys::PageUp || Key == EKeys::PageDown) ? 320.f : 90.f;
		const float Dir  = (Key == EKeys::Up || Key == EKeys::PageUp) ? -1.f : 1.f;
		BodyScroll->SetScrollOffset(FMath::Max(0.f, BodyScroll->GetScrollOffset() + Step * Dir));
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::T)
	{
		// Esc from the help page returns to the dials rather than leaving outright —
		// backing out one layer at a time is what every reader expects.
		if (bShowHelp)
		{
			bShowHelp = false;
			Refresh();
			return FReply::Handled();
		}
		OnClosed.Broadcast();
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::W)
	{
		Selected = static_cast<EKnob>((static_cast<uint8>(Selected) + static_cast<uint8>(EKnob::Count) - 1) % static_cast<uint8>(EKnob::Count));
		Refresh();
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::S)
	{
		Selected = static_cast<EKnob>((static_cast<uint8>(Selected) + 1) % static_cast<uint8>(EKnob::Count));
		Refresh();
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::A)
	{
		AdjustSelected(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::D)
	{
		AdjustSelected(+1);
		return FReply::Handled();
	}
	if (Key == EKeys::R)
	{
		RevertToFactory();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
