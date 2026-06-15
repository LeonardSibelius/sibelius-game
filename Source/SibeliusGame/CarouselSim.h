// CarouselSim.h
//
// SIB-46 — the headless spin engine: the 7-phase pipeline (the heart), the default slice content,
// and the carousel.SimRuns balancing instrument. PURE C++ math, deterministic under a seeded
// FRandomStream — no UI, no actors. Reuses the Celestial Fortune reel algorithm (SlotGameModel):
// weighted strips, uniform-stop windowing, left-to-right payline match with Wild substitution.
//
// Presentation (later) subscribes to results; it never computes payouts.

#pragma once

#include "CoreMinimal.h"
#include "CarouselTypes.h"

class UCarouselCharm;

// Per-build spin engine. Bind a build + symbol registry + resolved Charms, then Spin() repeatedly.
struct SIBELIUSGAME_API FCarouselSim
{
	const FMachineBuild* Build = nullptr;
	const TMap<FName, FSymbolDef>* Symbols = nullptr;

	// Hard cap on cascade re-spins per spin — the #1 predict-bug guard (no infinite chains).
	static constexpr int32 MaxCascadeDepth = 8;
	// Free spins awarded when the scatter triggers (slice placeholder; tune via SimRuns).
	static constexpr int32 FreeSpinsAward = 6;

	// Run one spin against Ctx (which carries the run RNG, Charms, free spins, boss curse). Resets the
	// per-spin working state, runs all 7 phases, and leaves the outcome in Ctx (read via MakeResult).
	void Spin(FSpinContext& Ctx) const;

	static FSpinResult MakeResult(const FSpinContext& Ctx);

	// Build the hardcoded slice: ~12 symbols + a starter Machine (strips/paylines/charms). Data-shaped
	// (FSymbolDef rows) so it can move to a DataTable later without touching the pipeline.
	static void BuildDefaultSlice(TMap<FName, FSymbolDef>& OutSymbols, FMachineBuild& OutBuild);

	// The starter Quota curve (chips/round) and spin budget — placeholders the sim will tune.
	static const TArray<int32>& DefaultQuotas();
	static int32 DefaultSpinBudget();

	// carousel.SimRuns: headless auto-play of NumRuns runs against the default slice; returns a report
	// (win-rate, avg spins-to-clear, EV/spin, payout distribution, bust-round histogram) and logs it.
	static FString SimRuns(int32 NumRuns, int32 Seed);

private:
	void SpinReels(FSpinContext& Ctx) const;
	void RespinReels(FSpinContext& Ctx, const TArray<int32>& Reels) const;
	int32 EvaluateLines(FSpinContext& Ctx) const;     // fills Ctx.LineWins, fires OnLineWin, returns base chips
	void ResolveScatters(FSpinContext& Ctx) const;
	void ApplyMultipliers(FSpinContext& Ctx) const;
	void ResolveCascades(FSpinContext& Ctx) const;
	void Tally(FSpinContext& Ctx) const;
	void Fire(ECharmTrigger Trigger, FSpinContext& Ctx) const;

	bool IsWild(const FName& Id, const FSpinContext& Ctx) const;   // Wild type, bSubstitutesAsWild, or ExtraWildSymbols
};
