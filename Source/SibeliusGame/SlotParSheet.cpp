// SlotParSheet.cpp — see header.

#include "SlotParSheet.h"

/* ============================================================================
   PAR SHEET — the ONLY place game math lives (SL1). Tune here, then re-run
   -run=SlotSmokeTest and read the printed report before accepting a change.

   Strip codes: *=Star m=Moon g=Galaxy t=Saturn r=Mars c=Crown 7=Seven
                W=Wild E=Earth(bonus)

   v1 strip composition (per 40 stops): Star 9, Moon 8, Galaxy 7, Saturn 5,
   Mars 5, Wild 1, Earth 2, Crown 2, Seven 1. All five reels use the same
   strip in v1 — per-reel variation is a future tuning pass.

   These values were file-scope statics in SlotGameModel.cpp until 2026-08-05.
   Moving them here changed nothing: RTP must still measure 95.43%.
   ==========================================================================*/
namespace
{
	const TCHAR* const DefaultStripDef = TEXT("*mg*tmrgmr*tgrE*mc*g*mW*tmr*gm7t*mEgrtcg");

	// 15 fixed paylines (row index per reel, rows 0=top 1=mid 2=bottom).
	const int32 DefaultLineDefs[15][5] = {
		{1,1,1,1,1},{0,0,0,0,0},{2,2,2,2,2},
		{0,1,2,1,0},{2,1,0,1,2},{0,0,1,2,2},{2,2,1,0,0},
		{1,0,1,2,1},{1,2,1,0,1},{0,1,1,1,0},{2,1,1,1,2},
		{1,0,0,0,1},{1,2,2,2,1},{0,1,0,1,0},{2,1,2,1,2}
	};

	const TArray<int32> EmptyRows;
}

ESlotSymbol FSlotParSheet::CharToSymbol(TCHAR C)
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

TCHAR FSlotParSheet::SymbolToChar(ESlotSymbol S)
{
	switch (S)
	{
	case ESlotSymbol::Star:   return '*';
	case ESlotSymbol::Moon:   return 'm';
	case ESlotSymbol::Galaxy: return 'g';
	case ESlotSymbol::Saturn: return 't';
	case ESlotSymbol::Mars:   return 'r';
	case ESlotSymbol::Crown:  return 'c';
	case ESlotSymbol::Seven:  return '7';
	case ESlotSymbol::Wild:   return 'W';
	case ESlotSymbol::Earth:  return 'E';
	default:                  return '?';
	}
}

bool FSlotParSheet::ParseStrip(const FString& Def, TArray<ESlotSymbol>& OutStrip, FString* OutError)
{
	OutStrip.Reset();
	OutStrip.Reserve(Def.Len());

	for (int32 i = 0; i < Def.Len(); ++i)
	{
		const ESlotSymbol Sym = CharToSymbol(Def[i]);
		if (Sym == ESlotSymbol::None)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("bad strip character '%c' at index %d"), Def[i], i);
			}
			OutStrip.Reset();
			return false;
		}
		OutStrip.Add(Sym);
	}
	return true;
}

FString FSlotParSheet::StripToString() const
{
	FString Out;
	Out.Reserve(Strip.Num());
	for (const ESlotSymbol S : Strip)
	{
		Out.AppendChar(SymbolToChar(S));
	}
	return Out;
}

FSlotParSheet FSlotParSheet::CelestialFortune()
{
	FSlotParSheet P;
	P.Name = TEXT("Celestial Fortune");

	FString Error;
	const bool bParsed = ParseStrip(DefaultStripDef, P.Strip, &Error);
	checkf(bParsed, TEXT("The DEFAULT strip failed to parse: %s"), *Error);

	P.PayTable = {
		{ ESlotSymbol::Star,     5.0,  15.0,   30.0 },
		{ ESlotSymbol::Moon,     5.0,  15.0,   40.0 },
		{ ESlotSymbol::Galaxy,  10.0,  25.0,   60.0 },
		{ ESlotSymbol::Saturn,  20.0,  50.0,  100.0 },
		{ ESlotSymbol::Mars,    30.0,  90.0,  220.0 },
		{ ESlotSymbol::Crown,   50.0, 180.0,  500.0 },
		{ ESlotSymbol::Seven,  100.0, 300.0, 1000.0 },
		{ ESlotSymbol::Wild,   100.0, 300.0, 1000.0 },
		// Earth is deliberately absent: bonus only, never pays on lines.
	};

	P.Paylines.Reserve(15);
	for (int32 i = 0; i < 15; ++i)
	{
		FSlotPayline L;
		L.Rows.Reserve(5);
		for (int32 r = 0; r < 5; ++r)
		{
			L.Rows.Add(DefaultLineDefs[i][r]);
		}
		P.Paylines.Add(MoveTemp(L));
	}

	return P;
}

const TArray<int32>& FSlotParSheet::Line(int32 Index) const
{
	return Paylines.IsValidIndex(Index) ? Paylines[Index].Rows : EmptyRows;
}

double FSlotParSheet::PayFor(ESlotSymbol Symbol, int32 Count, double PerLineBet) const
{
	if (Count < 3)
	{
		return 0.0;
	}

	for (const FSlotPayRow& Row : PayTable)
	{
		if (Row.Symbol == Symbol)
		{
			const double Mult = (Count >= 5) ? Row.Pay5 : (Count == 4 ? Row.Pay4 : Row.Pay3);
			return Mult * PerLineBet;
		}
	}
	return 0.0;   // Earth / None — bonus only
}

int32 FSlotParSheet::CountOf(ESlotSymbol Symbol) const
{
	int32 N = 0;
	for (const ESlotSymbol S : Strip)
	{
		if (S == Symbol)
		{
			++N;
		}
	}
	return N;
}

bool FSlotParSheet::EqualsMath(const FSlotParSheet& Other) const
{
	if (Strip != Other.Strip)                      { return false; }
	if (Paylines.Num() != Other.Paylines.Num())    { return false; }
	if (PayTable.Num() != Other.PayTable.Num())    { return false; }

	for (int32 i = 0; i < Paylines.Num(); ++i)
	{
		if (Paylines[i].Rows != Other.Paylines[i].Rows) { return false; }
	}
	for (int32 i = 0; i < PayTable.Num(); ++i)
	{
		const FSlotPayRow& A = PayTable[i];
		const FSlotPayRow& B = Other.PayTable[i];
		if (A.Symbol != B.Symbol
			|| !FMath::IsNearlyEqual(A.Pay3, B.Pay3)
			|| !FMath::IsNearlyEqual(A.Pay4, B.Pay4)
			|| !FMath::IsNearlyEqual(A.Pay5, B.Pay5))
		{
			return false;
		}
	}
	return true;
}

bool FSlotParSheet::IsStructurallyValid(FString* OutReason) const
{
	auto Fail = [OutReason](const FString& Why)
	{
		if (OutReason) { *OutReason = Why; }
		return false;
	};

	if (Strip.Num() < 3)
	{
		return Fail(FString::Printf(TEXT("strip has %d stops; needs at least 3"), Strip.Num()));
	}
	for (const ESlotSymbol S : Strip)
	{
		if (S == ESlotSymbol::None)
		{
			return Fail(TEXT("strip contains ESlotSymbol::None"));
		}
	}
	if (Paylines.Num() < 1)
	{
		return Fail(TEXT("no paylines"));
	}

	// Every payline must name a row for every reel, or the evaluator would read off
	// the end of the grid. A saved par sheet from a build with a different reel count
	// is exactly how that would arrive.
	for (int32 i = 0; i < Paylines.Num(); ++i)
	{
		if (Paylines[i].Rows.Num() != 5)
		{
			return Fail(FString::Printf(TEXT("payline %d has %d entries; needs 5"), i, Paylines[i].Rows.Num()));
		}
		for (const int32 Row : Paylines[i].Rows)
		{
			if (Row < 0 || Row > 2)
			{
				return Fail(FString::Printf(TEXT("payline %d has row %d; must be 0..2"), i, Row));
			}
		}
	}

	// A machine that pays nothing is structurally legal but certainly a mistake.
	if (PayTable.Num() < 1)
	{
		return Fail(TEXT("paytable is empty"));
	}

	if (OutReason) { OutReason->Reset(); }
	return true;
}
