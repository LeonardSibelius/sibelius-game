// SlotGameModel.cpp
//
// SIB-34 / S1 — pure slot game model. See header.

#include "SlotGameModel.h"

namespace SlotParSheetNS
{
/* ============================================================================
   PAR SHEET — the ONLY place game math lives (SL1). Tune here, then re-run
   -run=SlotSmokeTest and read the printed report before accepting a change.

   Strip codes: *=Star m=Moon g=Galaxy t=Saturn r=Mars c=Crown 7=Seven
                W=Wild E=Earth(bonus)

   v1 strip composition (per 40 stops): Star 9, Moon 8, Galaxy 7, Saturn 5,
   Mars 5, Wild 1, Earth 2, Crown 2, Seven 1. All five reels use the same
   strip in v1 — per-reel variation is a future tuning pass.
   ==========================================================================*/
static const TCHAR* StripDef = TEXT("*mg*tmrgmr*tgrE*mc*g*mW*tmr*gm7t*mEgrtcg");

struct FPay { ESlotSymbol Sym; double Pay3; double Pay4; double Pay5; };
static const FPay PayTable[] = {
	{ ESlotSymbol::Star,   5,   15,   30 },
	{ ESlotSymbol::Moon,   5,   15,   40 },
	{ ESlotSymbol::Galaxy, 10,  25,   60 },
	{ ESlotSymbol::Saturn, 20,  50,  100 },
	{ ESlotSymbol::Mars,   30,  90,  220 },
	{ ESlotSymbol::Crown,  50, 180,  500 },
	{ ESlotSymbol::Seven, 100, 300, 1000 },
	{ ESlotSymbol::Wild,  100, 300, 1000 },
	// Earth: bonus only — never pays on lines.
};

// 15 fixed paylines (row index per reel, rows 0=top 1=mid 2=bottom).
static const int8 LineDefs[15][5] = {
	{1,1,1,1,1},{0,0,0,0,0},{2,2,2,2,2},
	{0,1,2,1,0},{2,1,0,1,2},{0,0,1,2,2},{2,2,1,0,0},
	{1,0,1,2,1},{1,2,1,0,1},{0,1,1,1,0},{2,1,1,1,2},
	{1,0,0,0,1},{1,2,2,2,1},{0,1,0,1,0},{2,1,2,1,2}
};
/* ========================== end PAR SHEET =================================*/

static ESlotSymbol CharToSym(TCHAR C)
{
	switch (C)
	{
	case '*': return ESlotSymbol::Star;
	case 'm': return ESlotSymbol::Moon;
	case 'g': return ESlotSymbol::Galaxy;
	case 't': return ESlotSymbol::Saturn;
	case 'r': return ESlotSymbol::Mars;
	case 'c': return ESlotSymbol::Crown;
	case '7': return ESlotSymbol::Seven;
	case 'W': return ESlotSymbol::Wild;
	case 'E': return ESlotSymbol::Earth;
	default:  return ESlotSymbol::None;
	}
}

static const TArray<ESlotSymbol>& ParsedStrip()
{
	static TArray<ESlotSymbol> Strip = []()
	{
		TArray<ESlotSymbol> S;
		const FString Def(StripDef);
		for (int32 i = 0; i < Def.Len(); ++i)
		{
			const ESlotSymbol Sym = CharToSym(Def[i]);
			checkf(Sym != ESlotSymbol::None, TEXT("Bad strip char at %d"), i);
			S.Add(Sym);
		}
		return S;
	}();
	return Strip;
}

static const TArray<int8>& LineArray(int32 Index)
{
	static TArray<TArray<int8>> Lines = []()
	{
		TArray<TArray<int8>> L;
		for (int32 i = 0; i < 15; ++i)
		{
			TArray<int8> Row;
			for (int32 r = 0; r < 5; ++r) Row.Add(LineDefs[i][r]);
			L.Add(MoveTemp(Row));
		}
		return L;
	}();
	return Lines[Index];
}
} // namespace SlotParSheetNS

/* ---------------- static accessors ---------------- */

int32 USlotGameModel::NumLines() { return 15; }
const TArray<int8>& USlotGameModel::Line(int32 Index) { return SlotParSheetNS::LineArray(Index); }
const TArray<ESlotSymbol>& USlotGameModel::Strip(int32 /*Reel*/) { return SlotParSheetNS::ParsedStrip(); }

double USlotGameModel::PayFor(ESlotSymbol Symbol, int32 Count, double PerLineBet)
{
	if (Count < 3) return 0.0;
	for (const SlotParSheetNS::FPay& P : SlotParSheetNS::PayTable)
	{
		if (P.Sym == Symbol)
		{
			const double Mult = (Count >= 5) ? P.Pay5 : (Count == 4 ? P.Pay4 : P.Pay3);
			return Mult * PerLineBet;
		}
	}
	return 0.0; // Earth / None — bonus only
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
	const TArray<ESlotSymbol>& StripArr = SlotParSheetNS::ParsedStrip();
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

	// THE ONE BONUS RULE: 3+ Earths anywhere -> 8 free spins (retrigger adds 8).
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
		const TArray<int8>& L = SlotParSheetNS::LineArray(li);

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
