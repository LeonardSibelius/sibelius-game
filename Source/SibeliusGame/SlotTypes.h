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
