// SlotGameModel.cpp
//
// SIB-34 / S1 — pure slot game model. See header.
//
// The PAR SHEET used to live here as file-scope statics. It moved to SlotParSheet.cpp
// on 2026-08-05 so the model can HOLD its math instead of reading globals — see
// docs/PAR_SHEET_PANEL.md. Behaviour is unchanged; RTP must still measure 95.43%.

#include "SlotGameModel.h"
#include "SibeliusGame.h"

bool USlotGameModel::SetParSheet(const FSlotParSheet& InParSheet)
{
	FString Why;
	if (!InParSheet.IsStructurallyValid(&Why))
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Slot] rejected par sheet '%s': %s — keeping '%s'"),
			*InParSheet.Name, *Why, *ParSheet.Name);
		return false;
	}

	ParSheet = InParSheet;
	return true;
}

/* ---------------- model ---------------- */

void USlotGameModel::Init(int32 Seed)
{
	Rng.Initialize(Seed);
	FreeSpinsRemaining = 0;
	bInitialized = true;
}

FSlotSpinResult USlotGameModel::Spin(int32 TotalBet)
{
	checkf(bInitialized, TEXT("Call Init(Seed) before Spin()"));

	FSlotSpinResult Out;
	Out.bWasFreeSpin = (FreeSpinsRemaining > 0);
	if (Out.bWasFreeSpin)
	{
		--FreeSpinsRemaining;
	}
	const int32 WinMult = Out.bWasFreeSpin ? FREE_SPIN_MULTIPLIER : 1;

	// Spin the reels: one uniform stop per reel; window = 3 consecutive stops.
	const TArray<ESlotSymbol>& StripArr = ParSheet.Strip;
	const int32 Len = StripArr.Num();
	Out.Grid.SetNum(REELS * ROWS);
	for (int32 r = 0; r < REELS; ++r)
	{
		const int32 Stop = Rng.RandRange(0, Len - 1);
		for (int32 Row = 0; Row < ROWS; ++Row)
		{
			Out.Grid[r * 3 + Row] = StripArr[(Stop + Row) % Len];
		}
	}

	// Evaluate paylines.
	const double PerLineBet = static_cast<double>(TotalBet) / NumLines();
	EvaluateLines(Out.Grid, PerLineBet, WinMult, Out);

	// THE ONE BONUS RULE: 3+ Earths anywhere -> free spins (retrigger adds more).
	int32 Scatters = 0;
	for (const ESlotSymbol S : Out.Grid)
	{
		if (S == ESlotSymbol::Earth) ++Scatters;
	}
	Out.ScatterCount = Scatters;
	if (Scatters >= SCATTERS_TO_TRIGGER)
	{
		Out.bBonusTriggered = true;
		FreeSpinsRemaining += FREE_SPINS_AWARD;
	}
	Out.FreeSpinsRemaining = FreeSpinsRemaining;
	return Out;
}

void USlotGameModel::EvaluateLines(const TArray<ESlotSymbol>& Grid, double PerLineBet, int32 WinMult, FSlotSpinResult& Out) const
{
	double Total = 0.0;
	for (int32 li = 0; li < NumLines(); ++li)
	{
		const TArray<int32>& L = ParSheet.Line(li);
		if (L.Num() != REELS)
		{
			continue;   // malformed line; IsStructurallyValid should have caught it
		}

		ESlotSymbol Base = ESlotSymbol::None;
		int32 Count = 0;
		for (int32 r = 0; r < REELS; ++r)
		{
			const ESlotSymbol S = Grid[r * 3 + L[r]];
			if (S == ESlotSymbol::Earth) break;             // Earths break lines (bonus only)
			if (S == ESlotSymbol::Wild) { ++Count; continue; } // wild extends any run
			if (Base == ESlotSymbol::None) { Base = S; ++Count; }
			else if (S == Base) { ++Count; }
			else break;
		}
		if (Base == ESlotSymbol::None && Count > 0)
		{
			Base = ESlotSymbol::Wild;                        // pure-wild line pays as Wild
		}
		if (Count >= 3 && Base != ESlotSymbol::None)
		{
			const double Pay = PayFor(Base, Count, PerLineBet) * WinMult;
			if (Pay > 0.0)
			{
				FSlotLineWin W;
				W.LineIndex = li; W.Symbol = Base; W.Count = Count; W.Pay = Pay;
				Out.LineWins.Add(W);
				Total += Pay;
			}
		}
	}
	Out.TotalWin = Total;
}
