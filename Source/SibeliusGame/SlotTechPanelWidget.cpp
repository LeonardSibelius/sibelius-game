// SlotTechPanelWidget.cpp — see header.

#include "SlotTechPanelWidget.h"

#include "SlotGameModel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FLinearColor ColPanel   (0.02f, 0.03f, 0.05f, 0.94f);
	const FLinearColor ColTitle   (1.00f, 0.85f, 0.20f, 1.0f);   // the cabinet's gold
	const FLinearColor ColBody    (0.85f, 0.90f, 0.95f, 1.0f);
	const FLinearColor ColFooter  (0.60f, 0.65f, 0.72f, 1.0f);

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

	// Snapshot the machine as it stands. Everything is composed from this.
	Factory = Model ? Model->GetParSheet() : FSlotParSheet::CelestialFortune();

	// Seed the knobs from the sheet so the panel opens showing the truth rather than
	// its own defaults.
	WildCount = Factory.CountOf(ESlotSymbol::Wild);
	PaysMultiplier = 1.0;
	for (const FSlotPayRow& Row : Factory.PayTable)
	{
		if (Row.Symbol == ESlotSymbol::Seven) { JackpotPay = Row.Pay5; }
	}
}

void USlotTechPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);   // required to receive keys in UIOnly, same as the screen
	BuildTree();
	Refresh();
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
		CS->SetAnchors(FAnchors(0.16f, 0.06f, 0.84f, 0.94f));
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

	TitleText  = MakeText(TEXT("TechTitle"),  ColTitle,  30);
	BodyText   = MakeText(TEXT("TechBody"),   ColBody,   20);
	FooterText = MakeText(TEXT("TechFooter"), ColFooter, 17);

	FooterText->SetText(FText::FromString(
		TEXT("[UP/DOWN] choose a dial     [LEFT/RIGHT] turn it     [R] revert to factory     [ESC] close")));
}

FSlotParSheet USlotTechPanelWidget::ComposeParSheet() const
{
	FSlotParSheet P = Factory;
	P.Name = TEXT("Celestial Fortune (edited)");

	// WILDS — convert Star stops to Wild, or back. Star is the filler symbol, and
	// swapping rather than inserting keeps the strip LENGTH fixed: change the length and
	// every other symbol's odds move too, which would make the knob impossible to reason
	// about.
	int32 Have = P.CountOf(ESlotSymbol::Wild);
	for (int32 i = 0; i < P.Strip.Num() && Have < WildCount; ++i)
	{
		if (P.Strip[i] == ESlotSymbol::Star) { P.Strip[i] = ESlotSymbol::Wild; ++Have; }
	}
	for (int32 i = 0; i < P.Strip.Num() && Have > WildCount; ++i)
	{
		if (P.Strip[i] == ESlotSymbol::Wild) { P.Strip[i] = ESlotSymbol::Star; --Have; }
	}

	// PAYS x — scale the whole paytable. RTP moves in proportion; volatility and hit
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
	default:
		break;
	}
	Refresh();
}

void USlotTechPanelWidget::RevertToFactory()
{
	PaysMultiplier = 1.0;
	WildCount = Factory.CountOf(ESlotSymbol::Wild);
	JackpotPay = 1000.0;
	for (const FSlotPayRow& Row : Factory.PayTable)
	{
		if (Row.Symbol == ESlotSymbol::Seven) { JackpotPay = Row.Pay5; }
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
		TitleText->SetText(FText::FromString(TEXT("SERVICE PANEL   —   CELESTIAL FORTUNE")));
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(ComposeReportText(Rep, Sheet)));
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
	else if (Rep.RtpPercent > SlotLicence::MaxRtpPercent)
	{
		S += FString::Printf(TEXT("   UNLICENSED — pays %.2f%%, above the %.0f%% ceiling.\n")
		                     TEXT("   The house won't take it: it doesn't earn.\n"),
			Rep.RtpPercent, SlotLicence::MaxRtpPercent);
	}
	else
	{
		S += FString::Printf(TEXT("   UNLICENSED — pays %.2f%%, below the %.0f%% floor.\n")
		                     TEXT("   No jurisdiction licenses it.\n"),
			Rep.RtpPercent, SlotLicence::MinRtpPercent);
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

FReply USlotTechPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape || Key == EKeys::T)
	{
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
