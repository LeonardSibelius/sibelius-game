// SlotParSheet.h
//
// THE PAR SHEET, as data.
//
// "Par sheet" is commonly glossed **Probability Accounting Report** — the theoretical
// version of the reports a casino's data warehouse produces from real play. It is the
// complete mathematical description of a slot machine: what is on the reels, what the
// combinations pay, and which lines are read. Everything else about a machine is
// presentation.
//
// WHY THIS FILE EXISTS (step 1 of docs/PAR_SHEET_PANEL.md)
//
// This data used to be `static` file-scope constants in SlotGameModel.cpp with `static`
// accessors, which meant every machine in the game necessarily shared ONE immutable
// copy. That was fine while there was exactly one machine and nobody could edit it.
// It blocks two things we want:
//
//   1. The technician's panel — the player editing a par sheet at runtime.
//   2. G4's high-limit machine — a second cabinet with different math.
//
// So the model now HOLDS one of these rather than reading globals.
//
// THE CONTRACT FOR THIS STEP: behaviour must not change at all. CelestialFortune()
// returns exactly the values that used to be hard-coded, and SlotSmokeTest must still
// report 95.43% RTP. If that number moves, this refactor broke something.

#pragma once

#include "CoreMinimal.h"
#include "SlotTypes.h"
#include "SlotParSheet.generated.h"

/** One row of the paytable: what N-of-a-kind of this symbol pays, per line bet. */
USTRUCT(BlueprintType)
struct FSlotPayRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	ESlotSymbol Symbol = ESlotSymbol::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	double Pay3 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	double Pay4 = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	double Pay5 = 0.0;
};

/** One payline: the row index read on each reel (0 = top, 1 = middle, 2 = bottom). */
USTRUCT(BlueprintType)
struct FSlotPayline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TArray<int32> Rows;
};

/**
 * A complete slot machine, mathematically. Strip + paytable + paylines.
 *
 * v1 uses ONE strip for all five reels. Per-reel strips are a future tuning pass and
 * would be the natural next widening of this struct.
 */
USTRUCT(BlueprintType)
struct SIBELIUSGAME_API FSlotParSheet
{
	GENERATED_BODY()

	/** Shown on the technician's panel. The shipped machine is "Celestial Fortune". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FString Name;

	/** The reel strip, one entry per stop. All five reels read this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TArray<ESlotSymbol> Strip;

	/** What each symbol pays for 3 / 4 / 5 of a kind. Symbols absent here never pay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TArray<FSlotPayRow> PayTable;

	/** The fixed paylines, read left to right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TArray<FSlotPayline> Paylines;

	/** The shipped machine — the values that were hard-coded before this refactor. */
	static FSlotParSheet CelestialFortune();

	int32 NumLines() const { return Paylines.Num(); }
	int32 NumStops() const { return Strip.Num(); }

	/** Rows for a payline, or an empty array if the index is out of range. */
	const TArray<int32>& Line(int32 Index) const;

	/** Credits paid for Count-of-a-kind of Symbol at this per-line bet. 0 if it does not pay. */
	double PayFor(ESlotSymbol Symbol, int32 Count, double PerLineBet) const;

	/** How many stops on the strip carry this symbol — the number a knob moves. */
	int32 CountOf(ESlotSymbol Symbol) const;

	/**
	 * Structurally usable? Cheap sanity, NOT a judgement about RTP — the licence band
	 * is a separate, later question. Guards against a saved par sheet from an older
	 * build arriving malformed.
	 */
	bool IsStructurallyValid(FString* OutReason = nullptr) const;

	/**
	 * Do these two sheets describe the SAME MACHINE? Strip, paytable and paylines only —
	 * the display Name is deliberately ignored.
	 *
	 * Exists because "the par sheet changed" is what rebaselines the meters, and the panel
	 * calls SetParSheet on every Refresh() — on open, on the H toggle, on any repaint —
	 * usually with identical maths under a different name ("Celestial Fortune" from the
	 * cabinet, "Celestial Fortune (edited)" from the panel). Comparing whole structs, or
	 * names, would clear the player's session meters just for opening the panel.
	 */
	bool EqualsMath(const FSlotParSheet& Other) const;

	/** Strip codes: * Star, m Moon, g Galaxy, t Saturn, r Mars, c Crown, 7 Seven, W Wild, E Earth. */
	static ESlotSymbol CharToSymbol(TCHAR C);
	static TCHAR SymbolToChar(ESlotSymbol S);

	/** Parse a strip string into symbols. Returns false and sets OutError on a bad character. */
	static bool ParseStrip(const FString& Def, TArray<ESlotSymbol>& OutStrip, FString* OutError = nullptr);

	/** The strip as its compact string form — handy for logs and the par sheet report. */
	FString StripToString() const;
};
