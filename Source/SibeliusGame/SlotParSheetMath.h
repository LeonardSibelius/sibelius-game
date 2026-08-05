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
// THE WILD RULE — MODELLED FROM THE CODE, NOT FROM SLOT ORTHODOXY
//
// This is the trap flagged in the design doc. USlotGameModel::EvaluateLines walks the
// line left to right:
//
//     Earth            -> break (scatters never pay on lines)
//     Wild             -> extend the run, whatever the base is
//     first non-wild   -> becomes the base symbol
//     matches base     -> extend
//     anything else    -> break
//     ...and if the run ended with NO base ever set, it pays as Wild.
//
// Two consequences the maths must honour, both easy to get wrong:
//
//   1. A base symbol is established by the FIRST non-wild reel, and the run then
//      continues only on that symbol or Wild. The code never asks whether a shorter
//      all-Wild reading would have paid MORE. (It sometimes would: WWW** currently pays
//      five Stars rather than three Wilds. See the note in the .cpp — that is a
//      pre-existing behaviour, deliberately preserved here.)
//
//   2. "Base = Wild" therefore happens ONLY when the run ends before any non-wild reel:
//      all five reels Wild, or leading Wilds terminated by an EARTH. A leading Wild run
//      broken by a normal symbol does not pay as Wild — it adopts that symbol.
//
// The point of this file is to tell the player the truth about the machine they are
// editing. If it modelled idealised slot maths instead of this code, it would lie.
// SlotSmokeTest asserts the closed form and the million-spin simulation agree.
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
	 * False when the free-spin series does not converge — i.e. free spins retrigger
	 * often enough to award themselves faster than they are consumed, so expected
	 * return is unbounded. A knob range should never permit it; if this is ever false,
	 * RtpPercent is meaningless rather than merely large.
	 */
	bool bConverged = true;

	/** Volatility as a word. Nobody feels a standard deviation. */
	FString VolatilityWord() const;
};

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
