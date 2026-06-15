// CarouselTypes.h
//
// THE CAROUSEL OF FATES — casino-core roguelike, vertical slice (SIB-46).
// Data model for the HEADLESS SIMULATION core. Pure data + math, no UI/actors — the whole game
// must be playable as data so we can sim thousands of spins to balance (carousel.SimRuns) and gate
// it headless. Presentation only READS results (FSpinResult); it never computes payouts.
//
// Reuses the proven Celestial Fortune reel algorithm (SlotGameModel): weighted reel strips,
// uniform-stop windowing, left-to-right payline match with Wild substitution, seeded FRandomStream.
// Generalized here to be data-driven (FName symbol Ids, FSymbolDef rows) with INTEGER chips and the
// Charm event pipeline.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Engine/DataTable.h"
#include "CarouselTypes.generated.h"

class UCarouselCharm;

UENUM(BlueprintType)
enum class ESymbolType : uint8
{
	Normal      UMETA(DisplayName = "Normal"),
	Wild        UMETA(DisplayName = "Wild"),         // substitutes for Normal symbols
	Scatter     UMETA(DisplayName = "Scatter"),      // pays anywhere / triggers free spins
	Multiplier  UMETA(DisplayName = "Multiplier"),   // multiplies wins when it lands
	Bonus       UMETA(DisplayName = "Bonus")
};

// The 7 pipeline phases broadcast to Charms. Bitflags so a FCharmDef can declare which it cares about.
UENUM(BlueprintType, meta = (Bitflags))
enum class ECharmTrigger : uint8
{
	OnSpinStart     UMETA(DisplayName = "On Spin Start"),
	OnSymbolLanded  UMETA(DisplayName = "On Symbol Landed"),
	OnReelResolved  UMETA(DisplayName = "On Reel Resolved"),
	OnLineWin       UMETA(DisplayName = "On Line Win"),
	OnScatter       UMETA(DisplayName = "On Scatter"),
	OnSpinResolved  UMETA(DisplayName = "On Spin Resolved"),
	OnShopEnter     UMETA(DisplayName = "On Shop Enter"),
	OnRoundStart    UMETA(DisplayName = "On Round Start"),
	OnRoundEnd      UMETA(DisplayName = "On Round End")
};

// One symbol's definition — a DataTable row so symbols are CONTENT, not code (no recompile, and the
// SIB-45 bridge can tweak weights/payouts live). LinePayouts/values are INTEGER chips (no float drift).
USTRUCT(BlueprintType)
struct FSymbolDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") ESymbolType Type = ESymbolType::Normal;

	// match-length -> chips, e.g. {3:5, 4:15, 5:50}. Integer chips.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") TMap<int32, int32> LinePayouts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") int32 MultiplierValue = 1;   // Multiplier type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") bool bSubstitutesAsWild = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") bool bPaysAnywhere = false;   // scatter

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") int32 ShopCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") int32 Rarity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") int32 BaseWeight = 1;

	// Presentation refs (soft — the sim never loads them).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Symbol") TSoftObjectPtr<UObject> Icon;
};

// One reel's strip: the weighted list of symbol Ids (repeated Id = higher weight — the authentic
// slot model). Building the machine = editing these strips (the player's agency over their own RNG).
USTRUCT(BlueprintType)
struct FReelStrip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel") TArray<FName> Symbols;
};

// One payline: the row index visited on each reel (0=top). Buyable; start ~5, up to ~15.
USTRUCT(BlueprintType)
struct FPaylineDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Payline") TArray<int32> Rows;
};

// The machine + build state: what the player owns/edits this run.
USTRUCT(BlueprintType)
struct FMachineBuild
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build") int32 NumReels = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build") int32 RowsVisible = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build") TArray<FReelStrip> ReelStrips;       // one per reel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build") TArray<FPaylineDef> ActivePaylines;  // buyable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build") TArray<FName> OwnedCharms;            // ordered = resolution priority
};

// One winning payline in a spin (chips integer).
USTRUCT(BlueprintType)
struct FLineWin
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 LineIndex = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") FName Symbol;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 Count = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 Pay = 0;   // chips, before global multiplier
};

// A boss-curse rule modifier active for one round (slice: Locked Reel, The Debuff, The Drought, ...).
USTRUCT(BlueprintType)
struct FBossCurse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Curse") int32 LockedReel = -1;       // frozen reel, -1 = none
	UPROPERTY(BlueprintReadOnly, Category = "Curse") FName DebuffedSymbol;        // pays 0 this round
	UPROPERTY(BlueprintReadOnly, Category = "Curse") bool bScattersDisabled = false;
};

// The output a Spin() produces — what Presentation reads (it never sees the working FSpinContext).
USTRUCT(BlueprintType)
struct FSpinResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Spin") TArray<FName> Grid;          // [reel*RowsVisible + row]
	UPROPERTY(BlueprintReadOnly, Category = "Spin") TArray<FLineWin> LineWins;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 SpinPayout = 0;        // total chips, multiplier applied
	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 GlobalMultiplierPercent = 100;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 ScatterCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") bool bBonusTriggered = false;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") bool bWasFreeSpin = false;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 FreeSpinsRemaining = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Spin") int32 CascadeCount = 0;      // retriggers this spin
};

// A Charm's data (the "Joker" equivalent) — DataTable row. The EFFECT for the slice is a C++
// UCarouselCharm subclass keyed by Id (see CarouselCharm.h); long-term the common ops go data-driven.
USTRUCT(BlueprintType)
struct FCharmDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charm") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charm") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charm") int32 ShopCost = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charm") int32 Rarity = 0;

	// Which pipeline phases this Charm reacts to (metadata / tooling; the sim calls OnEvent for all).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charm", meta = (Bitmask, BitmaskEnum = "/Script/SibeliusGame.ECharmTrigger"))
	int32 Triggers = 0;
};

// Where a run currently is (run/round/shop state machine).
UENUM(BlueprintType)
enum class ECarouselRunPhase : uint8
{
	NotStarted  UMETA(DisplayName = "Not Started"),
	Spinning    UMETA(DisplayName = "Spinning"),     // playing the current round
	Shop        UMETA(DisplayName = "Shop"),          // cleared a round; spending currency
	Won         UMETA(DisplayName = "Won"),           // cleared the final round
	Lost        UMETA(DisplayName = "Lost")           // missed a quota
};

UENUM(BlueprintType)
enum class EShopItemType : uint8
{
	Charm      UMETA(DisplayName = "Charm"),
	Payline    UMETA(DisplayName = "Payline"),
	Symbol     UMETA(DisplayName = "Symbol")          // added to a chosen reel strip
};

// One purchasable offering in the shop. Currency (not chips) buys these.
USTRUCT(BlueprintType)
struct FShopItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop") EShopItemType Type = EShopItemType::Charm;
	UPROPERTY(BlueprintReadOnly, Category = "Shop") FName Id;            // charm or symbol id
	UPROPERTY(BlueprintReadOnly, Category = "Shop") int32 Cost = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Shop") int32 TargetReel = 0; // Symbol: which reel strip it joins
	UPROPERTY(BlueprintReadOnly, Category = "Shop") FText Label;
};

// Working state threaded by reference through all 7 phases. PLAIN struct (not reflected): it holds a
// live FRandomStream and raw Charm pointers, and only the sim touches it — Presentation reads
// FSpinResult instead. Charms read/mutate this to do their thing.
struct FSpinContext
{
	// Inputs / context (set up before the spin).
	const FMachineBuild* Build = nullptr;
	const TMap<FName, FSymbolDef>* Symbols = nullptr;   // the symbol registry
	TArray<UCarouselCharm*> Charms;                     // resolved from Build->OwnedCharms, in order
	FRandomStream* Rng = nullptr;                       // the run's seeded stream
	FBossCurse Curse;

	// Mutable working state.
	TArray<FName> Grid;                  // [reel*RowsVisible + row]
	TArray<FLineWin> LineWins;
	int32 GlobalMultiplierPercent = 100; // 100 = 1.0x; integer math avoids float drift (Compounder +50, etc.)
	int32 SpinPayout = 0;                // chips
	int32 ScatterCount = 0;
	bool  bBonusTriggered = false;
	bool  bWasFreeSpin = false;
	int32 FreeSpinsRemaining = 0;
	int32 CascadeCount = 0;
	int32 CurrentLineWinIndex = -1;      // set before firing OnLineWin so a Charm can read LineWins[idx]
	int32 EventReel = -1;                // set before OnReelResolved / OnSymbolLanded
	int32 EventCell = -1;                // grid index, set before OnSymbolLanded

	// Charm levers (OnSpinStart): extra wilds (e.g. Wildfire makes Flame wild), per-symbol payout
	// boost percent (e.g. Mundane Riches doubles low symbols), forced cells (reel*Rows+row -> Id).
	TSet<FName> ExtraWildSymbols;
	TMap<FName, int32> SymbolPayoutBoostPercent;  // 100 = unchanged; 200 = double
	TMap<int32, FName> ForcedCells;
	TSet<int32> ReelsToRespin;                    // OnLineWin Charms (Cascade) request reels here; phase 6 consumes

	const FSymbolDef* Find(const FName& Id) const { return Symbols ? Symbols->Find(Id) : nullptr; }
	FName At(int32 Reel, int32 Row, int32 Rows) const
	{
		const int32 Idx = Reel * Rows + Row;
		return Grid.IsValidIndex(Idx) ? Grid[Idx] : NAME_None;
	}
};
