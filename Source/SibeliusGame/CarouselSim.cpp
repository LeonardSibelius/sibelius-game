// CarouselSim.cpp — SIB-46 headless spin pipeline + default slice + carousel.SimRuns. See header.

#include "CarouselSim.h"
#include "CarouselCharm.h"
#include "UObject/Package.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCarousel, Log, All);

// Live tuning levers (also reachable from the SIB-45 bridge via exec-console). 100 = the baked
// defaults. Read at BuildDefaultSlice / SimRuns time so a change takes effect on the next run.
static TAutoConsoleVariable<int32> CVarCarouselPayoutScalePct(
	TEXT("carousel.PayoutScalePct"), 100,
	TEXT("Scale ALL line payouts by this percent (tuning lever; 100 = baked defaults)."));
static TAutoConsoleVariable<int32> CVarCarouselQuotaScalePct(
	TEXT("carousel.QuotaScalePct"), 100,
	TEXT("Scale round quotas by this percent (last-resort tuning lever; 100 = baked curve)."));

namespace CarouselSliceNS
{
	// The slice's Wild symbol Id — a pure-wild line pays as this (it carries payouts).
	static const FName WildPaysAs = TEXT("Fate");

	static void AddSymbol(TMap<FName, FSymbolDef>& Map, const TCHAR* Id, ESymbolType Type,
		int32 Pay3, int32 Pay4, int32 Pay5, int32 Weight,
		bool bWild = false, bool bScatter = false, int32 MultValue = 1)
	{
		FSymbolDef D;
		D.Id = FName(Id);
		D.DisplayName = FText::FromString(Id);
		D.Type = Type;
		if (Pay3 > 0) D.LinePayouts.Add(3, Pay3);
		if (Pay4 > 0) D.LinePayouts.Add(4, Pay4);
		if (Pay5 > 0) D.LinePayouts.Add(5, Pay5);
		D.bSubstitutesAsWild = bWild;
		D.bPaysAnywhere = bScatter;
		D.MultiplierValue = MultValue;
		D.BaseWeight = Weight;
		Map.Add(D.Id, D);
	}

	// Round-robin interleave: build a reel strip where each symbol appears BaseWeight times, spread
	// across the strip (not clumped) so the 3-row window sees varied symbols. Deterministic (no RNG).
	static FReelStrip BuildInterleavedStrip(const TMap<FName, FSymbolDef>& Symbols)
	{
		TArray<TPair<FName, int32>> Remaining;
		for (const TPair<FName, FSymbolDef>& Pair : Symbols)
		{
			if (Pair.Value.BaseWeight > 0)
			{
				Remaining.Add(TPair<FName, int32>(Pair.Key, Pair.Value.BaseWeight));
			}
		}
		Remaining.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B)
			{ return A.Key.LexicalLess(B.Key); });   // stable, deterministic order

		FReelStrip Strip;
		bool bAny = true;
		while (bAny)
		{
			bAny = false;
			for (TPair<FName, int32>& R : Remaining)
			{
				if (R.Value > 0)
				{
					Strip.Symbols.Add(R.Key);
					--R.Value;
					bAny = true;
				}
			}
		}
		return Strip;
	}
}

/* ------------------------------------------------------------------ content */

void FCarouselSim::BuildDefaultSlice(TMap<FName, FSymbolDef>& OutSymbols, FMachineBuild& OutBuild)
{
	using namespace CarouselSliceNS;
	OutSymbols.Reset();

	// 12 Sibelius-flavored symbols (placeholder payouts/weights — tune via carousel.SimRuns).
	// Tuned starter baseline (SIB-46, sim seed 7, 3000 runs): hit frequency 50.4% (dud ~49.6%),
	// round-1 clear ~55%, EV ~105/spin. Weights set hit frequency (Wild is the dominant lever +
	// denser commons); payouts (the 9x baseline below) set EV/clear; the quota curve was left alone
	// (last lever, untouched). carousel.PayoutScalePct / QuotaScalePct stay as live tuning knobs.
	//          Id            Type                       P3    P4    P5   Wt  wild  scat  mult
	AddSymbol(OutSymbols, TEXT("Cog"),       ESymbolType::Normal,      27,   72,  180, 16);
	AddSymbol(OutSymbols, TEXT("Key"),       ESymbolType::Normal,      27,   72,  180, 14);
	AddSymbol(OutSymbols, TEXT("Coin"),      ESymbolType::Normal,      27,   72,  180, 14);
	AddSymbol(OutSymbols, TEXT("Gear"),      ESymbolType::Normal,      45,  135,  360, 10);
	AddSymbol(OutSymbols, TEXT("Book"),      ESymbolType::Normal,      45,  135,  360, 10);
	AddSymbol(OutSymbols, TEXT("Eye"),       ESymbolType::Normal,      54,  162,  405,  6);
	AddSymbol(OutSymbols, TEXT("Flame"),     ESymbolType::Normal,      54,  162,  405,  6);
	AddSymbol(OutSymbols, TEXT("Star"),      ESymbolType::Multiplier,  45,  108,  270,  3, false, false, 2);
	AddSymbol(OutSymbols, TEXT("Dragon"),    ESymbolType::Normal,     180,  540, 1800,  2);
	AddSymbol(OutSymbols, TEXT("SauceDrop"), ESymbolType::Normal,     225,  720, 2700,  1);
	AddSymbol(OutSymbols, TEXT("Fate"),      ESymbolType::Wild,       135,  450, 1350, 12, true);
	AddSymbol(OutSymbols, TEXT("Carousel"),  ESymbolType::Scatter,     45,  135,  360,  2, false, true);

	// Live payout scaling (tuning / bridge lever).
	const int32 PayoutScale = FMath::Max(1, CVarCarouselPayoutScalePct.GetValueOnAnyThread());
	if (PayoutScale != 100)
	{
		for (TPair<FName, FSymbolDef>& Pair : OutSymbols)
		{
			for (TPair<int32, int32>& P : Pair.Value.LinePayouts)
			{
				P.Value = FMath::Max(1, P.Value * PayoutScale / 100);
			}
		}
	}

	OutBuild = FMachineBuild();
	OutBuild.NumReels = 5;
	OutBuild.RowsVisible = 3;

	// v1: all five reels share the interleaved strip (per-reel variation is a future tuning pass).
	const FReelStrip Strip = BuildInterleavedStrip(OutSymbols);
	for (int32 r = 0; r < OutBuild.NumReels; ++r)
	{
		OutBuild.ReelStrips.Add(Strip);
	}

	// Start with 5 paylines (buyable up to ~15 later).
	const int32 Lines[5][5] = {
		{1,1,1,1,1}, {0,0,0,0,0}, {2,2,2,2,2}, {0,1,2,1,0}, {2,1,0,1,2}
	};
	for (int32 li = 0; li < 5; ++li)
	{
		FPaylineDef L;
		for (int32 r = 0; r < OutBuild.NumReels; ++r) L.Rows.Add(Lines[li][r]);
		OutBuild.ActivePaylines.Add(L);
	}

	// Starter loadout (placeholder) — exercises the Charm pipeline in the sim end-to-end.
	OutBuild.OwnedCharms = { TEXT("Wildfire"), TEXT("Compounder") };
}

const TArray<int32>& FCarouselSim::DefaultQuotas()
{
	static const TArray<int32> Quotas = { 300, 550, 900, 1400, 2100, 3200, 4800, 7000 };  // rounds 4 & 8 are boss
	return Quotas;
}

int32 FCarouselSim::DefaultSpinBudget() { return 5; }

/* ------------------------------------------------------------------ pipeline */

bool FCarouselSim::IsWild(const FName& Id, const FSpinContext& Ctx) const
{
	if (Ctx.ExtraWildSymbols.Contains(Id)) return true;
	const FSymbolDef* D = Ctx.Find(Id);
	return D && (D->Type == ESymbolType::Wild || D->bSubstitutesAsWild);
}

void FCarouselSim::Fire(ECharmTrigger Trigger, FSpinContext& Ctx) const
{
	for (UCarouselCharm* C : Ctx.Charms)
	{
		if (C) { C->OnEvent(Trigger, Ctx); }
	}
}

void FCarouselSim::SpinReels(FSpinContext& Ctx) const
{
	const int32 R = Build->NumReels, Rows = Build->RowsVisible;
	Ctx.Grid.SetNum(R * Rows);
	for (int32 r = 0; r < R; ++r)
	{
		const FReelStrip& Strip = Build->ReelStrips[r % Build->ReelStrips.Num()];
		const int32 Len = Strip.Symbols.Num();
		const int32 Stop = (Len > 0) ? Ctx.Rng->RandRange(0, Len - 1) : 0;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const int32 Idx = r * Rows + Row;
			FName Sym = (Len > 0) ? Strip.Symbols[(Stop + Row) % Len] : NAME_None;
			if (const FName* Forced = Ctx.ForcedCells.Find(Idx)) { Sym = *Forced; }
			Ctx.Grid[Idx] = Sym;
			Ctx.EventCell = Idx;
			Fire(ECharmTrigger::OnSymbolLanded, Ctx);
		}
		Ctx.EventReel = r;
		Fire(ECharmTrigger::OnReelResolved, Ctx);
	}
}

void FCarouselSim::RespinReels(FSpinContext& Ctx, const TArray<int32>& Reels) const
{
	const int32 R = Build->NumReels, Rows = Build->RowsVisible;
	for (int32 r : Reels)
	{
		if (r < 0 || r >= R) continue;
		const FReelStrip& Strip = Build->ReelStrips[r % Build->ReelStrips.Num()];
		const int32 Len = Strip.Symbols.Num();
		const int32 Stop = (Len > 0) ? Ctx.Rng->RandRange(0, Len - 1) : 0;
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const int32 Idx = r * Rows + Row;
			Ctx.Grid[Idx] = (Len > 0) ? Strip.Symbols[(Stop + Row) % Len] : NAME_None;
		}
	}
}

int32 FCarouselSim::EvaluateLines(FSpinContext& Ctx) const
{
	const int32 R = Build->NumReels, Rows = Build->RowsVisible;
	Ctx.LineWins.Reset();
	int32 Total = 0;

	for (int32 li = 0; li < Build->ActivePaylines.Num(); ++li)
	{
		const TArray<int32>& LineRows = Build->ActivePaylines[li].Rows;
		FName Base = NAME_None;
		int32 Count = 0;
		for (int32 r = 0; r < R; ++r)
		{
			const int32 Row = LineRows.IsValidIndex(r) ? LineRows[r] : 0;
			const FName S = Ctx.At(r, Row, Rows);
			const FSymbolDef* D = Ctx.Find(S);
			if (D && D->bPaysAnywhere) break;          // scatters break paylines (bonus only)
			if (IsWild(S, Ctx)) { ++Count; continue; } // wild extends any run
			if (Base.IsNone()) { Base = S; ++Count; }
			else if (S == Base) { ++Count; }
			else break;
		}
		if (Base.IsNone() && Count > 0) { Base = CarouselSliceNS::WildPaysAs; }  // pure-wild line

		if (Count >= 3 && !Base.IsNone())
		{
			int32 Pay = 0;
			if (Base != Ctx.Curse.DebuffedSymbol)   // boss curse: debuffed symbol pays 0
			{
				if (const FSymbolDef* BD = Ctx.Find(Base))
				{
					Pay = BD->LinePayouts.FindRef(Count);
					if (const int32* Boost = Ctx.SymbolPayoutBoostPercent.Find(Base))
					{
						Pay = Pay * (*Boost) / 100;   // e.g. Mundane Riches doubles low symbols
					}
				}
			}
			if (Pay > 0)
			{
				FLineWin W;
				W.LineIndex = li; W.Symbol = Base; W.Count = Count; W.Pay = Pay;
				Ctx.LineWins.Add(W);
				Total += Pay;
				Ctx.CurrentLineWinIndex = Ctx.LineWins.Num() - 1;
				Fire(ECharmTrigger::OnLineWin, Ctx);
			}
		}
	}
	return Total;
}

void FCarouselSim::ResolveScatters(FSpinContext& Ctx) const
{
	Ctx.ScatterCount = 0;
	if (Ctx.Curse.bScattersDisabled) { return; }   // boss curse: The Drought

	FName ScatterId = NAME_None;
	for (const FName& S : Ctx.Grid)
	{
		const FSymbolDef* D = Ctx.Find(S);
		if (D && D->bPaysAnywhere) { ++Ctx.ScatterCount; ScatterId = S; }
	}

	const int32 Threshold = FMath::Max(1, Ctx.ScatterThreshold);   // Scatter Shrine lowers this to 2
	if (Ctx.ScatterCount >= Threshold && !ScatterId.IsNone())
	{
		Ctx.bBonusTriggered = true;
		Ctx.FreeSpinsRemaining += FreeSpinsAward;
		Fire(ECharmTrigger::OnScatter, Ctx);

		// Scatter pay (pays anywhere): a pseudo-line (LineIndex -1) so Tally sums it.
		if (const FSymbolDef* SD = Ctx.Find(ScatterId))
		{
			const int32 Pay = SD->LinePayouts.FindRef(FMath::Min(Ctx.ScatterCount, 5));
			if (Pay > 0)
			{
				FLineWin W; W.LineIndex = -1; W.Symbol = ScatterId; W.Count = Ctx.ScatterCount; W.Pay = Pay;
				Ctx.LineWins.Add(W);
			}
		}
	}
}

void FCarouselSim::ApplyMultipliers(FSpinContext& Ctx) const
{
	// Symbol-based multipliers (e.g. Star x2) stack additively into the global percent. Charm-based
	// contributions were already added at OnSpinStart (High Roller, Compounder).
	for (const FName& S : Ctx.Grid)
	{
		const FSymbolDef* D = Ctx.Find(S);
		if (D && D->Type == ESymbolType::Multiplier && D->MultiplierValue > 1)
		{
			Ctx.GlobalMultiplierPercent += (D->MultiplierValue - 1) * 100;
		}
	}
}

void FCarouselSim::ResolveCascades(FSpinContext& Ctx) const
{
	// Charm cascades (Cascade): re-spin requested reels and keep the BEST grid seen. Depth-capped —
	// the #1 predict-bug guard against infinite chains. (Placeholder "keep-best" rule; tune later.)
	int32 BestTotal = 0;
	for (const FLineWin& W : Ctx.LineWins) { BestTotal += W.Pay; }
	TArray<FName> BestGrid = Ctx.Grid;
	TArray<FLineWin> BestWins = Ctx.LineWins;

	while (Ctx.ReelsToRespin.Num() > 0 && Ctx.CascadeCount < MaxCascadeDepth)
	{
		const TArray<int32> Reels = Ctx.ReelsToRespin.Array();
		Ctx.ReelsToRespin.Reset();
		RespinReels(Ctx, Reels);
		const int32 T = EvaluateLines(Ctx);   // refills LineWins, fires OnLineWin (Cascade may re-add)
		++Ctx.CascadeCount;
		if (T > BestTotal)
		{
			BestTotal = T;
			BestGrid = Ctx.Grid;
			BestWins = Ctx.LineWins;
		}
	}

	Ctx.Grid = BestGrid;
	Ctx.LineWins = BestWins;
}

void FCarouselSim::Tally(FSpinContext& Ctx) const
{
	int32 Base = 0;
	for (const FLineWin& W : Ctx.LineWins) { Base += W.Pay; }
	Ctx.SpinPayout = Base * Ctx.GlobalMultiplierPercent / 100;   // integer chips throughout
}

void FCarouselSim::Spin(FSpinContext& Ctx) const
{
	check(Build && Symbols && Ctx.Rng);

	// Reset per-spin working state (cross-spin state — FreeSpinsRemaining, Charm internals — persists).
	Ctx.Grid.Reset();
	Ctx.LineWins.Reset();
	Ctx.GlobalMultiplierPercent = 100;
	Ctx.SpinPayout = 0;
	Ctx.ScatterCount = 0;
	Ctx.ScatterThreshold = 3;
	Ctx.bBonusTriggered = false;
	Ctx.CascadeCount = 0;
	Ctx.CurrentLineWinIndex = -1;
	Ctx.ExtraWildSymbols.Reset();
	Ctx.SymbolPayoutBoostPercent.Reset();
	Ctx.ForcedCells.Reset();
	Ctx.ReelsToRespin.Reset();

	Ctx.bWasFreeSpin = (Ctx.FreeSpinsRemaining > 0);
	if (Ctx.bWasFreeSpin) { --Ctx.FreeSpinsRemaining; }

	Fire(ECharmTrigger::OnSpinStart, Ctx);   // 1 — Charms adjust wilds/boosts/multipliers
	SpinReels(Ctx);                          // 2 — weighted draw (+ OnSymbolLanded / OnReelResolved)
	EvaluateLines(Ctx);                      // 3 — paylines (+ OnLineWin)
	ResolveCascades(Ctx);                    // 6 — re-spins on the final grid (depth-capped)
	ResolveScatters(Ctx);                    // 4 — pay-anywhere count on the settled grid (+ OnScatter)
	ApplyMultipliers(Ctx);                   // 5 — symbol + charm multipliers into the global percent
	Tally(Ctx);                              // 7 — sum to SpinPayout
	Fire(ECharmTrigger::OnSpinResolved, Ctx);
}

FSpinResult FCarouselSim::MakeResult(const FSpinContext& Ctx)
{
	FSpinResult R;
	R.Grid = Ctx.Grid;
	R.LineWins = Ctx.LineWins;
	R.SpinPayout = Ctx.SpinPayout;
	R.GlobalMultiplierPercent = Ctx.GlobalMultiplierPercent;
	R.ScatterCount = Ctx.ScatterCount;
	R.bBonusTriggered = Ctx.bBonusTriggered;
	R.bWasFreeSpin = Ctx.bWasFreeSpin;
	R.FreeSpinsRemaining = Ctx.FreeSpinsRemaining;
	R.CascadeCount = Ctx.CascadeCount;
	return R;
}

/* ------------------------------------------------------------------ SimRuns */

FCarouselSimReport FCarouselSim::RunSim(int32 NumRuns, int32 Seed)
{
	NumRuns = FMath::Max(1, NumRuns);

	TMap<FName, FSymbolDef> Symbols;
	FMachineBuild Build;
	BuildDefaultSlice(Symbols, Build);

	FCarouselSim Sim;
	Sim.Build = &Build;
	Sim.Symbols = &Symbols;

	const int32 QuotaScale = FMath::Max(1, CVarCarouselQuotaScalePct.GetValueOnAnyThread());
	TArray<int32> Quotas = DefaultQuotas();
	if (QuotaScale != 100) { for (int32& Q : Quotas) { Q = FMath::Max(1, Q * QuotaScale / 100); } }
	const int32 Budget = DefaultSpinBudget();
	const bool bOwnsHighRoller = Build.OwnedCharms.Contains(FName(TEXT("HighRoller")));

	// Charms created ONCE; Reset() per run (no per-spin/per-run allocation in the hot loop).
	TArray<UCarouselCharm*> Charms;
	for (const FName& Id : Build.OwnedCharms)
	{
		Charms.Add(UCarouselCharm::Make(Id, GetTransientPackage()));
	}

	// Aggregates.
	int64 TotalSpins = 0, TotalChips = 0, ClearedRoundSpins = 0;
	int32 RunWins = 0, ClearedRounds = 0;
	TArray<int32> BustHist; BustHist.Init(0, Quotas.Num());
	TArray<int32> ReachedRound; ReachedRound.Init(0, Quotas.Num());   // runs that started round r
	TArray<int32> ClearedRound; ClearedRound.Init(0, Quotas.Num());   // runs that cleared round r
	const int32 Buckets[] = { 0, 1, 11, 51, 201, 1001 };   // payout distribution edges
	int32 Dist[6] = { 0,0,0,0,0,0 };

	for (int32 Run = 0; Run < NumRuns; ++Run)
	{
		FRandomStream Rng(Seed + Run);   // reproducible per run
		for (UCarouselCharm* C : Charms) { if (C) C->Reset(); }

		FSpinContext Ctx;
		Ctx.Build = &Build;
		Ctx.Symbols = &Symbols;
		Ctx.Rng = &Rng;
		Ctx.Charms = Charms;
		Ctx.FreeSpinsRemaining = 0;

		bool bBusted = false;
		for (int32 RoundIdx = 0; RoundIdx < Quotas.Num(); ++RoundIdx)
		{
			int32 Quota = Quotas[RoundIdx];
			if (bOwnsHighRoller) { Quota = Quota * 125 / 100; }   // High Roller: +25% quotas

			// Boss curses (slice uses 2): round 4 = The Debuff, round 8 = The Drought.
			Ctx.Curse = FBossCurse();
			if (RoundIdx == 3) { Ctx.Curse.DebuffedSymbol = TEXT("Dragon"); }
			if (RoundIdx == 7) { Ctx.Curse.bScattersDisabled = true; }

			for (UCarouselCharm* C : Charms) { if (C) C->OnEvent(ECharmTrigger::OnRoundStart, Ctx); }
			++ReachedRound[RoundIdx];

			int32 RoundChips = 0, SpinsUsed = 0;
			for (int32 s = 0; s < Budget; ++s)
			{
				++SpinsUsed; ++TotalSpins;
				Sim.Spin(Ctx);
				const int32 Pay = Ctx.SpinPayout;
				RoundChips += Pay; TotalChips += Pay;

				int32 b = 0;
				while (b + 1 < 6 && Pay >= Buckets[b + 1]) { ++b; }
				++Dist[b];

				if (RoundChips >= Quota) { break; }
			}

			for (UCarouselCharm* C : Charms) { if (C) C->OnEvent(ECharmTrigger::OnRoundEnd, Ctx); }

			if (RoundChips >= Quota)
			{
				ClearedRoundSpins += SpinsUsed; ++ClearedRounds; ++ClearedRound[RoundIdx];
			}
			else
			{
				bBusted = true;
				BustHist[RoundIdx] += 1;
				break;
			}
		}
		if (!bBusted) { ++RunWins; }
	}

	FCarouselSimReport Rep;
	Rep.NumRuns = NumRuns; Rep.Seed = Seed; Rep.Budget = Budget; Rep.RunWins = RunWins;
	Rep.NumPaylines = Build.ActivePaylines.Num();
	Rep.TotalSpins = TotalSpins; Rep.TotalChips = TotalChips;
	Rep.EvPerSpin = (TotalSpins > 0) ? (double)TotalChips / TotalSpins : 0.0;
	Rep.HitFreqPct = (TotalSpins > 0) ? 100.0 * (TotalSpins - Dist[0]) / TotalSpins : 0.0;
	Rep.WinRatePct = 100.0 * RunWins / NumRuns;
	Rep.Round1ClearPct = (ReachedRound[0] > 0) ? 100.0 * ClearedRound[0] / ReachedRound[0] : 0.0;
	Rep.AvgSpinsToClear = (ClearedRounds > 0) ? (double)ClearedRoundSpins / ClearedRounds : 0.0;
	Rep.Dist.Append(Dist, UE_ARRAY_COUNT(Dist));
	Rep.Quotas = Quotas; Rep.ReachedRound = ReachedRound; Rep.ClearedRound = ClearedRound; Rep.BustHist = BustHist;
	for (const FName& Id : Build.OwnedCharms) { Rep.CharmList += Id.ToString() + TEXT(" "); }
	return Rep;
}

FString FCarouselSim::SimRuns(int32 NumRuns, int32 Seed)
{
	const FCarouselSimReport R = RunSim(NumRuns, Seed);

	FString Out;
	Out += TEXT("\n===== carousel.SimRuns report =====\n");
	Out += FString::Printf(TEXT("runs=%d  seed=%d  charms=[ %s]  paylines=%d  budget=%d/round\n"),
		R.NumRuns, R.Seed, *R.CharmList, R.NumPaylines, R.Budget);
	Out += FString::Printf(TEXT("ROUND-1 CLEAR RATE: %.1f%% (%d/%d)   [target ~50-60%%]\n"),
		R.Round1ClearPct, R.ClearedRound.IsValidIndex(0) ? R.ClearedRound[0] : 0, R.ReachedRound.IsValidIndex(0) ? R.ReachedRound[0] : 0);
	Out += FString::Printf(TEXT("HIT FREQUENCY (spins that pay): %.1f%%   [target ~45-55%% dud => 45-55%% hit]\n"), R.HitFreqPct);
	Out += FString::Printf(TEXT("EV per spin: %.3f chips   (total %lld chips over %lld spins)\n"), R.EvPerSpin, R.TotalChips, R.TotalSpins);
	Out += FString::Printf(TEXT("WIN RATE (cleared all %d rounds): %.2f%% (%d/%d)\n"), R.Quotas.Num(), R.WinRatePct, R.RunWins, R.NumRuns);
	Out += FString::Printf(TEXT("avg spins-to-clear a round: %.2f / %d\n"), R.AvgSpinsToClear, R.Budget);
	Out += TEXT("per-round clear rate (cleared / reached):\n");
	for (int32 i = 0; i < R.Quotas.Num(); ++i)
	{
		const double Pct = (R.ReachedRound[i] > 0) ? 100.0 * R.ClearedRound[i] / R.ReachedRound[i] : 0.0;
		Out += FString::Printf(TEXT("  round %d (quota %5d): %5.1f%%  (%d/%d)\n"), i + 1, R.Quotas[i], Pct, R.ClearedRound[i], R.ReachedRound[i]);
	}
	Out += TEXT("payout distribution per spin:\n");
	const TCHAR* Labels[6] = { TEXT("0 (dud)"), TEXT("1-10"), TEXT("11-50"), TEXT("51-200"), TEXT("201-1000"), TEXT("1001+") };
	for (int32 i = 0; i < 6 && i < R.Dist.Num(); ++i)
	{
		const double Pct = (R.TotalSpins > 0) ? 100.0 * R.Dist[i] / R.TotalSpins : 0.0;
		Out += FString::Printf(TEXT("  %-10s : %6.2f%%  (%d)\n"), Labels[i], Pct, R.Dist[i]);
	}
	Out += TEXT("bust-round histogram (round where the run died):\n");
	for (int32 i = 0; i < R.Quotas.Num(); ++i)
	{
		Out += FString::Printf(TEXT("  round %d (quota %5d): %d\n"), i + 1, R.Quotas[i], R.BustHist[i]);
	}
	Out += TEXT("===================================\n");

	UE_LOG(LogCarousel, Display, TEXT("%s"), *Out);
	return Out;
}

/* ------------------------------------------------------------------ console */

static void CarouselSimRunsConsole(const TArray<FString>& Args)
{
	const int32 NumRuns = (Args.Num() >= 1) ? FCString::Atoi(*Args[0]) : 1000;
	const int32 Seed = (Args.Num() >= 2) ? FCString::Atoi(*Args[1]) : 12345;
	UE_LOG(LogCarousel, Display, TEXT("[Carousel] SimRuns N=%d seed=%d ..."), NumRuns, Seed);
	FCarouselSim::SimRuns(NumRuns, Seed);

	// Headless one-shot (-unattended -ExecCmds): request exit so the editor doesn't hang open. In the
	// interactive editor / SIB-45 bridge (IsUnattended()==false) leave it running.
	if (FApp::IsUnattended())
	{
		RequestEngineExit(TEXT("carousel.SimRuns complete"));
	}
}

static FAutoConsoleCommand GCarouselSimRunsCmd(
	TEXT("carousel.SimRuns"),
	TEXT("Headless auto-play N runs against the default Carousel slice. Usage: carousel.SimRuns N [seed]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&CarouselSimRunsConsole));
