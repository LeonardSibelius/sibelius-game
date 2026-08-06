// SlotTypes.h
//
// SIB-34 / S1 — Slot track. Shared types for the slot game model.
// The model is PURE logic (no UI, no actors) — see SlotGameModel.h for the
// architecture rationale. These types are what the UMG screen (S2) will read.

#pragma once

#include "CoreMinimal.h"
#include "SlotTypes.generated.h"

UENUM(BlueprintType)
enum class ESlotSymbol : uint8
{
	Star    UMETA(DisplayName = "Star"),
	Moon    UMETA(DisplayName = "Moon"),
	Galaxy  UMETA(DisplayName = "Galaxy"),
	Saturn  UMETA(DisplayName = "Saturn"),
	Mars    UMETA(DisplayName = "Mars"),
	Crown   UMETA(DisplayName = "Crown"),
	Seven   UMETA(DisplayName = "Lucky 7"),
	Wild    UMETA(DisplayName = "Wild"),
	Earth   UMETA(DisplayName = "Earth (Bonus)"),
	None    UMETA(Hidden)
};

// One winning payline in a spin result.
USTRUCT(BlueprintType)
struct FSlotLineWin
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 LineIndex = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Slot") ESlotSymbol Symbol = ESlotSymbol::None;
	UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 Count = 0;     // leftmost matches (3..5)
	UPROPERTY(BlueprintReadOnly, Category = "Slot") double Pay = 0.0;    // credits (multiplier applied)
};

// Full result of one Spin().
USTRUCT(BlueprintType)
struct FSlotSpinResult
{
	GENERATED_BODY()

	// Grid[reel*3 + row], reels 0..4 left->right, rows 0..2 top->bottom.
	UPROPERTY(BlueprintReadOnly, Category = "Slot") TArray<ESlotSymbol> Grid;

	UPROPERTY(BlueprintReadOnly, Category = "Slot") TArray<FSlotLineWin> LineWins;
	UPROPERTY(BlueprintReadOnly, Category = "Slot") double TotalWin = 0.0;     // credits, multiplier applied
	UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 ScatterCount = 0;    // Earths anywhere
	UPROPERTY(BlueprintReadOnly, Category = "Slot") bool bBonusTriggered = false; // 3+ Earths this spin
	UPROPERTY(BlueprintReadOnly, Category = "Slot") bool bWasFreeSpin = false;    // this spin consumed a free spin
	UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 FreeSpinsRemaining = 0; // after this spin

	ESlotSymbol At(int32 Reel, int32 Row) const { return Grid.IsValidIndex(Reel * 3 + Row) ? Grid[Reel * 3 + Row] : ESlotSymbol::None; }
};

/**
 * THE METERS — what the machine actually did, as opposed to what par says it should.
 * Step 1 of docs/FLOOR_REPORT.md.
 *
 * Real cabinets carry two sets: HARD meters (lifetime, the regulator's number, never
 * resettable) and SOFT meters (the current session, what an attendant reads on a service
 * call). Same struct serves both — the model owns a session copy, the save owns the
 * lifetime one, and the panel shows them side by side.
 *
 * int64 throughout: lifetime coin-in on a machine played for hours passes int32 sooner
 * than feels plausible, and the wider type costs nothing.
 */
USTRUCT(BlueprintType)
struct FSlotMeters
{
	GENERATED_BODY()

	/** Spins the player paid for. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slot") int64 BaseSpins = 0;

	/** Free spins consumed. These wager nothing and pay plenty — see CoinIn. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slot") int64 FreeSpins = 0;

	/**
	 * Total wagered. FREE SPINS NEVER ADD TO THIS — they cost nothing.
	 *
	 * Getting that backwards is exactly how a real floor report ends up disagreeing with
	 * par: the bonus round would look like it returned its own stake, and measured RTP
	 * would come in low by the whole value of the feature.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slot") int64 CoinIn = 0;

	/** Total paid out, base and free spins alike. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slot") int64 CoinOut = 0;

	/**
	 * BASE spins that returned anything. Free spins are excluded on purpose: they are
	 * not player-initiated, so counting them would inflate hit frequency — the same
	 * convention SlotParSheetMath::MeasureBySimulation uses, so the two are comparable.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slot") int64 PayingSpins = 0;

	/**
	 * Bonus triggers on ANY spin, base or free (a retrigger counts). The closed form's
	 * TriggerPercent is the per-spin probability and does not care which kind of spin it
	 * was, so this denominator is BaseSpins + FreeSpins — matching SlotSmokeTest.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slot") int64 BonusTriggers = 0;

	/** Largest single-spin payout ever seen, in credits. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Slot") int64 BiggestWin = 0;

	int64 TotalSpins() const { return BaseSpins + FreeSpins; }

	/** True once a single credit has been wagered. Guards every divide below. */
	bool HasPlay() const { return CoinIn > 0 && BaseSpins > 0; }

	/** Measured return, in percent. -1 when nothing has been played. */
	double MeasuredRtpPercent() const
	{
		return (CoinIn > 0) ? (100.0 * static_cast<double>(CoinOut) / static_cast<double>(CoinIn)) : -1.0;
	}

	/** Measured share of PAID spins that returned anything, in percent. -1 when none. */
	double MeasuredHitPercent() const
	{
		return (BaseSpins > 0) ? (100.0 * static_cast<double>(PayingSpins) / static_cast<double>(BaseSpins)) : -1.0;
	}

	/** Measured bonus rate as "1 in N spins". 0 when never triggered. */
	double MeasuredBonusOneIn() const
	{
		return (BonusTriggers > 0) ? (static_cast<double>(TotalSpins()) / static_cast<double>(BonusTriggers)) : 0.0;
	}

	/** Fold another set in — used to add the live session onto the saved lifetime. */
	void Add(const FSlotMeters& O)
	{
		BaseSpins     += O.BaseSpins;
		FreeSpins     += O.FreeSpins;
		CoinIn        += O.CoinIn;
		CoinOut       += O.CoinOut;
		PayingSpins   += O.PayingSpins;
		BonusTriggers += O.BonusTriggers;
		BiggestWin     = FMath::Max(BiggestWin, O.BiggestWin);
	}

	void Reset() { *this = FSlotMeters(); }
};
