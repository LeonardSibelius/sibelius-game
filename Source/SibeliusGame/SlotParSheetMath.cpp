// SlotParSheetMath.cpp — see header for the derivation.

#include "SlotParSheetMath.h"
#include "SlotGameModel.h"

#include "UObject/Package.h"

FString FSlotParSheetReport::VolatilityWord() const
{
	if (Volatility < 0.0)  { return TEXT("—"); }
	if (Volatility < 2.0)  { return TEXT("LOW"); }
	if (Volatility < 5.0)  { return TEXT("MEDIUM"); }
	if (Volatility < 12.0) { return TEXT("HIGH"); }
	return TEXT("WILD");
}

namespace
{
	/** Probability a given reel position shows this symbol = its share of the strip. */
	double SymbolProbability(const FSlotParSheet& P, ESlotSymbol S)
	{
		const int32 Len = P.Strip.Num();
		return (Len > 0) ? static_cast<double>(P.CountOf(S)) / static_cast<double>(Len) : 0.0;
	}

	/**
	 * Distribution of how many Earths ONE reel shows, index 0..ROWS.
	 *
	 * Rows within a reel are NOT independent — they are three consecutive strip stops,
	 * so an Earth pair sitting side by side on the strip makes two-Earth windows far
	 * more likely than independence would suggest. Enumerating every stop is exact and
	 * costs one pass over the strip.
	 */
	TArray<double> ScattersPerReelDistribution(const FSlotParSheet& P)
	{
		const int32 Rows = USlotGameModel::ROWS;
		TArray<double> Dist;
		Dist.Init(0.0, Rows + 1);

		const int32 Len = P.Strip.Num();
		if (Len <= 0)
		{
			Dist[0] = 1.0;
			return Dist;
		}

		for (int32 Stop = 0; Stop < Len; ++Stop)
		{
			int32 Earths = 0;
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				if (P.Strip[(Stop + Row) % Len] == ESlotSymbol::Earth)
				{
					++Earths;
				}
			}
			Dist[Earths] += 1.0;
		}

		for (double& D : Dist)
		{
			D /= static_cast<double>(Len);
		}
		return Dist;
	}
}

double SlotParSheetMath::ExactTriggerProbability(const FSlotParSheet& ParSheet)
{
	const int32 Reels = USlotGameModel::REELS;
	const int32 Need  = USlotGameModel::SCATTERS_TO_TRIGGER;

	const TArray<double> PerReel = ScattersPerReelDistribution(ParSheet);

	// Convolve the per-reel distribution across all reels — reels ARE independent, even
	// though rows within one reel are not.
	TArray<double> Total;
	Total.Init(0.0, 1);
	Total[0] = 1.0;

	for (int32 r = 0; r < Reels; ++r)
	{
		TArray<double> Next;
		Next.Init(0.0, Total.Num() + PerReel.Num() - 1);
		for (int32 a = 0; a < Total.Num(); ++a)
		{
			if (Total[a] == 0.0) { continue; }
			for (int32 b = 0; b < PerReel.Num(); ++b)
			{
				Next[a + b] += Total[a] * PerReel[b];
			}
		}
		Total = MoveTemp(Next);
	}

	double Trigger = 0.0;
	for (int32 n = Need; n < Total.Num(); ++n)
	{
		Trigger += Total[n];
	}
	return Trigger;
}

FSlotParSheetReport SlotParSheetMath::Analyze(const FSlotParSheet& ParSheet)
{
	FSlotParSheetReport R;

	const int32 Reels = USlotGameModel::REELS;
	const int32 Len   = ParSheet.Strip.Num();
	if (Len <= 0 || ParSheet.NumLines() <= 0)
	{
		return R;
	}

	// --- Base game: expected multiplier of ONE line (== base-game RTP; see header) ----
	//
	// EXACT BY ENUMERATION. A line is five i.i.d. draws from the strip's alphabet, which
	// holds at most nine distinct symbols — so there are at most 9^5 = 59,049 possible
	// lines. Walking every one and weighting by its probability is exact, costs well
	// under a millisecond, and is provably right because it applies THE SAME best-reading
	// rule the model applies.
	//
	// This replaced a closed-form derivation. Once a line pays its BEST reading rather
	// than its first, the algebra needs the joint distribution over every candidate
	// symbol's run length simultaneously — derivable, but exactly the kind of subtle
	// reasoning that produces a formula which looks right and quietly misprices the
	// machine. Enumeration cannot drift from the rule because it evaluates the rule.
	double BaseRtp = 0.0;

	// The alphabet actually present on this strip, with each symbol's probability.
	TArray<ESlotSymbol> Alphabet;
	TArray<double> AlphabetProb;
	{
		TSet<ESlotSymbol> Seen;
		for (const ESlotSymbol S : ParSheet.Strip)
		{
			if (!Seen.Contains(S))
			{
				Seen.Add(S);
				Alphabet.Add(S);
				AlphabetProb.Add(SymbolProbability(ParSheet, S));
			}
		}
	}

	// Candidate symbols a line can be read as: everything in the paytable except Earth.
	TArray<ESlotSymbol> Candidates;
	for (const FSlotPayRow& Row : ParSheet.PayTable)
	{
		if (Row.Symbol != ESlotSymbol::None && Row.Symbol != ESlotSymbol::Earth)
		{
			Candidates.AddUnique(Row.Symbol);
		}
	}

	TMap<ESlotSymbol, double> RtpBySymbol;
	TMap<ESlotSymbol, double> HitBySymbol;

	const int32 N = Alphabet.Num();
	if (N > 0)
	{
		TArray<int32> Idx;
		Idx.Init(0, Reels);

		for (;;)
		{
			// Probability of this exact line, and its symbols.
			double Prob = 1.0;
			ESlotSymbol Lineup[8];
			for (int32 r = 0; r < Reels; ++r)
			{
				Prob *= AlphabetProb[Idx[r]];
				Lineup[r] = Alphabet[Idx[r]];
			}

			if (Prob > 0.0)
			{
				// The best reading — identical rule to USlotGameModel::EvaluateLines.
				ESlotSymbol BestSym = ESlotSymbol::None;
				double BestMult = 0.0;
				for (const ESlotSymbol Cand : Candidates)
				{
					int32 Count = 0;
					for (int32 r = 0; r < Reels; ++r)
					{
						if (Lineup[r] == Cand || Lineup[r] == ESlotSymbol::Wild) { ++Count; }
						else { break; }
					}
					const double Mult = ParSheet.PayFor(Cand, Count, 1.0);
					if (Mult > BestMult)
					{
						BestMult = Mult;
						BestSym = Cand;
					}
				}

				if (BestMult > 0.0)
				{
					BaseRtp += Prob * BestMult;
					RtpBySymbol.FindOrAdd(BestSym) += Prob * BestMult;
					HitBySymbol.FindOrAdd(BestSym) += Prob;
				}
			}

			// Odometer over the reels.
			int32 Carry = Reels - 1;
			while (Carry >= 0 && ++Idx[Carry] >= N)
			{
				Idx[Carry] = 0;
				--Carry;
			}
			if (Carry < 0)
			{
				break;
			}
		}
	}

	R.BaseRtpPercent = BaseRtp * 100.0;

	// Per-symbol anatomy, credited to whichever symbol the line actually paid as.
	for (const ESlotSymbol Cand : Candidates)
	{
		FSlotSymbolContribution C;
		C.Symbol         = Cand;
		C.StripCount     = ParSheet.CountOf(Cand);
		C.BaseRtpPercent = RtpBySymbol.FindRef(Cand) * 100.0;
		C.LineHitPercent = HitBySymbol.FindRef(Cand) * 100.0;
		R.Contributions.Add(C);
	}

	// --- The free-spin round ---------------------------------------------------------
	// Every spin, base or free, triggers with the same probability q and awards A free
	// spins. Free spins retrigger on the same terms, so with B base spins and F free
	// spins the awards balance at
	//
	//     F = q·A·(B + F)   =>   F/B = qA / (1 - qA)
	//
	// which diverges as qA approaches 1: free spins would then generate themselves
	// faster than they are used up.
	const double Q = ExactTriggerProbability(ParSheet);
	const double A = static_cast<double>(USlotGameModel::FREE_SPINS_AWARD);
	const double M = static_cast<double>(USlotGameModel::FREE_SPIN_MULTIPLIER);

	R.TriggerPercent = Q * 100.0;

	const double QA = Q * A;
	if (QA >= 1.0)
	{
		R.bConverged = false;
		R.FreeSpinsPerBaseSpin = TNumericLimits<double>::Max();
		R.RtpPercent = TNumericLimits<double>::Max();
		R.HoldPercent = -TNumericLimits<double>::Max();
		return R;
	}

	R.FreeSpinsPerBaseSpin = QA / (1.0 - QA);

	// A free spin costs nothing and pays M times a normal spin.
	R.RtpPercent  = R.BaseRtpPercent * (1.0 + R.FreeSpinsPerBaseSpin * M);
	R.HoldPercent = 100.0 - R.RtpPercent;

	R.Contributions.Sort([](const FSlotSymbolContribution& A2, const FSlotSymbolContribution& B2)
	{
		return A2.BaseRtpPercent > B2.BaseRtpPercent;
	});

	return R;
}

void SlotParSheetMath::MeasureBySimulation(const FSlotParSheet& ParSheet, FSlotParSheetReport& Report,
	int32 Spins, int32 Seed)
{
	if (Spins <= 0)
	{
		return;
	}

	USlotGameModel* Model = NewObject<USlotGameModel>(GetTransientPackage());
	if (!Model || !Model->SetParSheet(ParSheet))
	{
		return;
	}
	Model->Init(Seed);

	const int32 Bet = Model->NumLines();   // 1 credit per line keeps the arithmetic honest
	int32 Paying = 0;
	int32 Counted = 0;
	double Sum = 0.0;
	double SumSq = 0.0;

	for (int32 i = 0; i < Spins; ++i)
	{
		const FSlotSpinResult Res = Model->Spin(Bet);

		// Free spins are not player-initiated, so they are not "spins that paid" from
		// the player's point of view — counting them would inflate hit frequency.
		if (Res.bWasFreeSpin)
		{
			continue;
		}

		++Counted;
		if (Res.TotalWin > 0.0)
		{
			++Paying;
		}

		const double Ret = Res.TotalWin / static_cast<double>(Bet);
		Sum   += Ret;
		SumSq += Ret * Ret;
	}

	if (Counted > 0)
	{
		Report.HitFrequencyPercent = 100.0 * static_cast<double>(Paying) / static_cast<double>(Counted);

		const double Mean = Sum / static_cast<double>(Counted);
		const double Var  = FMath::Max(0.0, SumSq / static_cast<double>(Counted) - Mean * Mean);
		Report.Volatility = FMath::Sqrt(Var);
	}
}
