// SlotParSheetMath.h
//
// The par sheet, solved. Step 2 of docs/PAR_SHEET_PANEL.md.
//
// The technician's panel must update RTP AS A KNOB MOVES. A million-spin simulation
// takes a fifth of a second — fine for a gate, far too slow to feel live, and it would
// make the panel a "press Simulate and wait" screen instead of a lesson. So RTP is
// computed in CLOSED FORM here, in microseconds.
//
// ---------------------------------------------------------------------------
// WHY EVERY PAYLINE HAS THE SAME EXPECTED PAY
//
// A reel stops uniformly at random and shows a 3-stop window. So the symbol appearing
// at ANY given row of a reel is a uniform draw from the strip — the row does not matter.
// Reels are independent. Therefore all 15 paylines see 5 i.i.d. draws from the same
// distribution, and every line has identical expected pay regardless of its shape.
//
// That collapses the whole base-game calculation:
//
//     RTP(lines) = E[payout multiplier of ONE line]
//
// because TotalWin sums 15 lines each betting TotalBet/15.
//
// ---------------------------------------------------------------------------
// THE WILD RULE — AND WHY THE BASE GAME IS ENUMERATED, NOT SOLVED
//
// A line pays its BEST reading. Wild substitutes for everything, so one line can be
// read as several different symbols and the machine pays whichever is worth most —
// each candidate's run being the leading reels showing that symbol or a Wild. Earth
// matches nothing and is not in the paytable, so it breaks every run.
//
// (Until 2026-08-05 the model fixed the base symbol at the first non-wild reel and
// never looked back, which UNDERPAID: W W W * * paid five Stars for 30 when three
// Wilds were worth 100. Walt's call to fix it; RTP rose accordingly.)
//
// That best-of rule is what makes a neat closed form treacherous. Solving it needs the
// joint distribution over every candidate symbol's run length at once — derivable, but
// precisely the sort of subtle reasoning that yields a formula which looks right and
// quietly misprices the machine. In a feature built to TEACH what a par sheet is, a
// plausible-but-wrong number is the worst possible failure.
//
// So the base game is computed by EXACT ENUMERATION instead. A line is five i.i.d.
// draws from the strip's alphabet, which holds at most nine distinct symbols: at most
// 9^5 = 59,049 lines, each weighted by its probability. Well under a millisecond, and
// it cannot drift from the rule because it evaluates the rule — the same loop the model
// runs. SlotSmokeTest still asserts it against a million-spin simulation.
//
// ---------------------------------------------------------------------------
// WHAT IS EXACT AND WHAT IS SIMULATED
//
//   EXACT   RTP, per-symbol contribution, bonus trigger probability.
//   SIM     Hit frequency and volatility. "Does this spin pay anything?" needs
//           inclusion-exclusion across 15 overlapping paylines; 10,000 spins answer it
//           in about a millisecond, comfortably inside a frame.

#pragma once

#include "CoreMinimal.h"
#include "SlotTypes.h"
#include "SlotParSheet.h"

class USlotGameModel;

/** One symbol's share of RTP — the anatomy the panel shows so a knob becomes a lesson. */
struct FSlotSymbolContribution
{
	ESlotSymbol Symbol = ESlotSymbol::None;

	/** Stops on the strip carrying this symbol. */
	int32 StripCount = 0;

	/** Percentage points of BASE-GAME RTP this symbol supplies. */
	double BaseRtpPercent = 0.0;

	/** Chance a single line pays on this symbol at all. */
	double LineHitPercent = 0.0;
};

/** Everything the panel and the gate want to know about a par sheet. */
struct FSlotParSheetReport
{
	/** Total return including the free-spin round, in percent. THE number. */
	double RtpPercent = 0.0;

	/** Lines only, ignoring free spins. Useful for seeing what the bonus is worth. */
	double BaseRtpPercent = 0.0;

	/** House edge, the accountant's way of saying the same thing: 100 - RTP. */
	double HoldPercent = 0.0;

	/** Probability any single spin triggers the bonus. */
	double TriggerPercent = 0.0;

	/** Expected free spins earned per base spin, retriggers included. */
	double FreeSpinsPerBaseSpin = 0.0;

	/** Per-symbol anatomy, richest contributor first. */
	TArray<FSlotSymbolContribution> Contributions;

	/** SIMULATED: share of spins that pay anything. -1 if not measured. */
	double HitFrequencyPercent = -1.0;

	/** SIMULATED: standard deviation of per-spin return, in units of total bet. -1 if not measured. */
	double Volatility = -1.0;

	/**
	 * SIMULATED: standard deviation of return PER UNIT WAGERED. -1 if not measured.
	 *
	 * The difference from Volatility above is the free-spin round, and it matters.
	 * Volatility measures base spins counting only their OWN payout — the bonus a spin
	 * triggers is dropped, because free spins are skipped outright. That is the right
	 * figure for the panel's "feel" word (how a single pull feels) but the wrong one for
	 * a confidence band, and wrong in the dangerous direction: the bonus is the largest
	 * single source of variance in the machine, so leaving it out makes the band TOO
	 * NARROW, and a page built to say "your ordinary session is ordinary" would start
	 * calling ordinary sessions unusual.
	 *
	 * Here each base spin is credited with the free-spin winnings it went on to produce,
	 * so one sample is one CYCLE: the wagered spin plus everything it spawned. That is
	 * the basis the industry's volatility index uses, and the one the standard error
	 * SD/sqrt(N) is valid for.
	 */
	double WageredVolatility = -1.0;

	/**
	 * Half-width of the 95% band around RTP after N wagered spins, in percentage points.
	 * Returns -1 when volatility has not been measured or N < 1.
	 *
	 * The whole floor report rests on this: two standard errors, where the standard
	 * error of a mean of N samples is SD/sqrt(N).
	 */
	SIBELIUSGAME_API double ConfidenceHalfWidth(int64 WageredSpins) const;

	/**
	 * Roughly how many wagered spins to measure this machine to within Tolerance
	 * PERCENTAGE POINTS at 95% confidence. 0 when volatility is unmeasured.
	 *
	 * Inverting the above: N = (2 * SD / t)^2. For a machine like Celestial Fortune this
	 * lands near a million, which is the single most useful number on the page.
	 */
	SIBELIUSGAME_API double SpinsToMeasureWithin(double TolerancePoints) const;

	/**
	 * False when the free-spin series does not converge — i.e. free spins retrigger
	 * often enough to award themselves faster than they are consumed, so expected
	 * return is unbounded. A knob range should never permit it; if this is ever false,
	 * RtpPercent is meaningless rather than merely large.
	 */
	bool bConverged = true;

	/** Volatility as a word. Nobody feels a standard deviation. */
	FString VolatilityWord() const;
};

/**
 * THE HOUSE RULES (locked, docs/PAR_SHEET_PANEL.md).
 *
 * Floor is regulatory — below it no jurisdiction licenses the machine. Ceiling is
 * COMMERCIAL — the house will not run a machine that does not earn. An unlicensed
 * machine refuses to spin: the refusal lands on the machine, not the player, so nobody
 * has winnings taken away after watching the reels land.
 *
 * The shipped sheet returns 95.57%, comfortably inside but not lazily so — the default
 * machine is already a real design decision near the commercial edge.
 */
namespace SlotLicence
{
	constexpr double MinRtpPercent = 85.0;
	constexpr double MaxRtpPercent = 96.0;

	inline bool IsLicensed(double RtpPercent)
	{
		return RtpPercent >= MinRtpPercent && RtpPercent <= MaxRtpPercent;
	}
}

namespace SlotParSheetMath
{
	/** Exact RTP, contributions and trigger probability. No simulation, microseconds. */
	SIBELIUSGAME_API FSlotParSheetReport Analyze(const FSlotParSheet& ParSheet);

	/**
	 * Measure hit frequency and volatility by spinning a real model, and write them into
	 * Report. Uses the ACTUAL game code, so it cannot drift from what players experience.
	 * 10,000 spins is about a millisecond.
	 */
	SIBELIUSGAME_API void MeasureBySimulation(const FSlotParSheet& ParSheet, FSlotParSheetReport& Report,
		int32 Spins = 10000, int32 Seed = 20260805);

	/** Exact probability that one spin shows enough scatters to trigger the bonus. */
	SIBELIUSGAME_API double ExactTriggerProbability(const FSlotParSheet& ParSheet);
}
