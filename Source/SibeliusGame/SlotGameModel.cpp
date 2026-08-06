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

	// Locked decision 3 (docs/FLOOR_REPORT.md): a par change takes a fresh baseline. Only
	// on an ACCEPTED sheet — a rejected one leaves the machine untouched, so its meters
	// must stay untouched too.
	ResetSessionMeters();
	return true;
}

/* ---------------- model ---------------- */

void USlotGameModel::Init(int32 Seed)
{
	Rng.Initialize(Seed);
	FreeSpinsRemaining = 0;
	bInitialized = true;
	ResetSessionMeters();
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

	/* ---------- THE METERS (docs/FLOOR_REPORT.md) ----------
	   Counted here rather than in the screen because this is the only place that knows
	   what the spin paid, and because the headless suite drives this function directly —
	   so the meters are gated against the closed form rather than eyeballed.

	   Rounding: pays are Mult * (TotalBet / 15), and every bet the game actually uses is a
	   multiple of 15 (the screen bets 150, the suite bets 15), so TotalWin is integral and
	   this rounds nothing. The round is here so an odd bet degrades to sub-credit noise
	   instead of silently truncating every win. */
	const int64 Won = FMath::RoundToInt64(Out.TotalWin);

	if (Out.bWasFreeSpin)
	{
		++SessionMeters.FreeSpins;   // no CoinIn: a free spin wagers nothing
	}
	else
	{
		++SessionMeters.BaseSpins;
		SessionMeters.CoinIn += TotalBet;
		if (Won > 0) { ++SessionMeters.PayingSpins; }   // hit frequency is a PAID-spin statistic
	}

	SessionMeters.CoinOut += Won;
	SessionMeters.BiggestWin = FMath::Max(SessionMeters.BiggestWin, Won);
	if (Out.bBonusTriggered) { ++SessionMeters.BonusTriggers; }

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

		// THE LINE PAYS ITS BEST READING (Walt, 2026-08-05).
		//
		// Wild substitutes for everything, so one line can be read several ways and the
		// machine must pay whichever is worth most. The previous rule fixed the base
		// symbol at the first non-wild reel and never looked back, which UNDERPAID: on
		// W W W * *, three Wilds are worth 100 but it paid five Stars for 30. Real
		// machines pay the best reading; so does this one now.
		//
		// Each candidate symbol's run is the leading reels showing that symbol or a
		// Wild. Earth matches nothing and is absent from the paytable, so it still
		// breaks every run — scatters never pay on lines.
		ESlotSymbol BestSymbol = ESlotSymbol::None;
		int32 BestCount = 0;
		double BestPay = 0.0;

		for (const FSlotPayRow& Row : ParSheet.PayTable)
		{
			const ESlotSymbol Candidate = Row.Symbol;
			if (Candidate == ESlotSymbol::None || Candidate == ESlotSymbol::Earth)
			{
				continue;
			}

			int32 Count = 0;
			for (int32 r = 0; r < REELS; ++r)
			{
				const ESlotSymbol S = Grid[r * 3 + L[r]];
				if (S == Candidate || S == ESlotSymbol::Wild) { ++Count; }
				else { break; }
			}

			const double Pay = PayFor(Candidate, Count, PerLineBet) * WinMult;
			if (Pay > BestPay)
			{
				BestPay = Pay;
				BestSymbol = Candidate;
				BestCount = Count;
			}
		}

		if (BestPay > 0.0)
		{
			FSlotLineWin W;
			W.LineIndex = li; W.Symbol = BestSymbol; W.Count = BestCount; W.Pay = BestPay;
			Out.LineWins.Add(W);
			Total += BestPay;
		}
	}
	Out.TotalWin = Total;
}
