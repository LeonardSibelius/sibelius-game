// SlotGameModel.h
//
// SIB-34 / S1 — The slot machine's brain: a PURE game model. No UI, no actors,
// no timers. Seeded RNG, real reel strips, deterministic and headlessly
// simulatable — the par sheet made executable. The UMG screen (S2) and the
// cabinet actor (S3) only PRESENT what this class decides.
//
// Game rules (parity with the live Celestial Fortune web game, Teresita-spec):
//   • 5 reels × 3 rows, 15 fixed paylines, pays left-to-right, 3+ matching
//   • WILD substitutes for everything except Earth
//   • THE ONE BONUS RULE: 3+ Earths anywhere = 6 free spins, all wins ×3
//     (retrigger adds 8 more; Earths don't pay on lines)
//
// Par sheet (strip + pays + paylines) is FSlotParSheet — see SlotParSheet.h. The model
// HOLDS one rather than reading globals, so a second cabinet can run different math and
// the technician's panel can edit it at runtime (docs/PAR_SHEET_PANEL.md).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Math/RandomStream.h"
#include "SlotTypes.h"
#include "SlotParSheet.h"
#include "SlotGameModel.generated.h"

UCLASS(BlueprintType)
class SIBELIUSGAME_API USlotGameModel : public UObject
{
	GENERATED_BODY()

public:
	// Must be called before Spin(). Seed makes the whole game deterministic (SL2).
	UFUNCTION(BlueprintCallable, Category = "Slot")
	void Init(int32 Seed);

	// One spin at TotalBet credits (multiple of 15). If free spins remain, the
	// spin consumes one instead of a bet (caller checks bWasFreeSpin for credits).
	UFUNCTION(BlueprintCallable, Category = "Slot")
	FSlotSpinResult Spin(int32 TotalBet);

	UFUNCTION(BlueprintPure, Category = "Slot")
	bool IsInFreeSpins() const { return FreeSpinsRemaining > 0; }

	UFUNCTION(BlueprintPure, Category = "Slot")
	int32 GetFreeSpinsRemaining() const { return FreeSpinsRemaining; }

	/**
	 * The machine's math. Defaults to Celestial Fortune (the shipped par sheet).
	 *
	 * A rejected par sheet leaves the model on the one it already had — a machine with
	 * no valid math would spin and pay nothing, which is worse than refusing the change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slot")
	bool SetParSheet(const FSlotParSheet& InParSheet);

	const FSlotParSheet& GetParSheet() const { return ParSheet; }

	/**
	 * THE SESSION METERS — docs/FLOOR_REPORT.md.
	 *
	 * The model counts because it is the only thing that knows what a spin actually paid,
	 * and because MeasureBySimulation() already drives a real model: that makes the meters
	 * self-testing against the closed form rather than merely eyeballed (SlotSmokeTest).
	 *
	 * The model holds SESSION meters only. Lifetime totals live in the save and are folded
	 * in at display time, so nothing here needs to survive a level load.
	 */
	const FSlotMeters& GetSessionMeters() const { return SessionMeters; }

	/**
	 * Clear the session meters. Called by Init() and on an ACCEPTED par sheet change —
	 * locked decision 3: turning a dial takes a fresh baseline, exactly as an attendant
	 * would before and after a par change. There is deliberately no player-facing reset;
	 * hard meters are interesting precisely because they cannot be cleared.
	 */
	void ResetSessionMeters() { SessionMeters.Reset(); }

	// Exposed for the smoke test + UI (paytable screen, line highlighting). These were
	// static until 2026-08-05; they read the INSTANCE's par sheet now, because two
	// cabinets may legitimately disagree about the answer.
	int32 NumLines() const { return ParSheet.NumLines(); }
	const TArray<int32>& Line(int32 Index) const { return ParSheet.Line(Index); }
	double PayFor(ESlotSymbol Symbol, int32 Count, double PerLineBet) const { return ParSheet.PayFor(Symbol, Count, PerLineBet); }
	const TArray<ESlotSymbol>& Strip(int32 /*Reel*/) const { return ParSheet.Strip; }

	static constexpr int32 REELS = 5;
	static constexpr int32 ROWS = 3;
	static constexpr int32 FREE_SPINS_AWARD = 6;
	static constexpr int32 FREE_SPIN_MULTIPLIER = 3;
	static constexpr int32 SCATTERS_TO_TRIGGER = 3;

private:
	void EvaluateLines(const TArray<ESlotSymbol>& Grid, double PerLineBet, int32 WinMult, FSlotSpinResult& Out) const;

	UPROPERTY()
	FSlotParSheet ParSheet = FSlotParSheet::CelestialFortune();

	FRandomStream Rng;
	int32 FreeSpinsRemaining = 0;
	bool bInitialized = false;

	/** Counted in Spin(). Session scope — see GetSessionMeters(). */
	FSlotMeters SessionMeters;
};
