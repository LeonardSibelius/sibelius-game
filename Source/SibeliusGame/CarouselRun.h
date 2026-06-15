// CarouselRun.h
//
// SIB-46 — the run/round/quota/currency + shop state machine. PURE C++ (headless, deterministic) so
// it sims and gates without rendering, per the spec's simulation/presentation split. Drives the
// FCarouselSim spin pipeline across a Run of Rounds (Antes); UCarouselRunSubsystem is a thin UObject
// wrapper that exposes this to Blueprint/presentation and broadcasts events.
//
// Currency != chips: chips clear a round's Quota; clearing pays Currency (base + per-unused-spin
// bonus + capped interest on the bank), and Currency buys shop Offerings (Charms / Paylines /
// Symbols). The banking-vs-spending tension is the Balatro economy.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "UObject/StrongObjectPtr.h"
#include "CarouselTypes.h"
#include "CarouselSim.h"

class UCarouselCharm;

struct SIBELIUSGAME_API FCarouselRun
{
	// Non-copyable: SpinCtx holds raw pointers into this run's own members.
	FCarouselRun() = default;
	FCarouselRun(const FCarouselRun&) = delete;
	FCarouselRun& operator=(const FCarouselRun&) = delete;

	// --- Read-only-ish state (the subsystem surfaces these to presentation) ---
	ECarouselRunPhase Phase = ECarouselRunPhase::NotStarted;
	int32 RoundIndex = 0;          // 0-based
	int32 CurrentQuota = 0;
	int32 SpinsRemaining = 0;
	int32 RoundChips = 0;          // accumulated this round
	int32 Currency = 0;            // banked currency (buys shop items)
	int32 LastReward = 0;          // currency awarded by the last clear
	FSpinResult LastSpin;
	FBossCurse CurrentCurse;
	TArray<FShopItem> Offerings;

	// --- The evolving build + content registries ---
	FMachineBuild Build;
	TMap<FName, FSymbolDef> Symbols;

	// --- Economy tunables (placeholders; tune like the sim) ---
	int32 StartingCurrency   = 4;
	int32 ClearBaseReward    = 3;
	int32 PerUnusedSpinBonus = 1;
	int32 InterestRatePct    = 20;   // % of banked currency, paid on clear
	int32 InterestCap        = 5;
	int32 RerollCost         = 1;
	int32 ShopSize           = 3;
	int32 CharmCost          = 5;
	int32 PaylineCost        = 4;
	int32 SymbolCost         = 3;

	// --- API (the subsystem forwards to these) ---
	void StartRun(int32 Seed);
	bool Spin();                 // one spin in the current round; resolves clear/bust
	bool Reroll();               // re-roll the shop (costs RerollCost)
	bool Buy(int32 OfferIndex);  // purchase an offering, applying it to the build
	bool AdvanceToNextRound();   // leave the shop into the next round (or Won)

	bool CanSpin() const { return Phase == ECarouselRunPhase::Spinning && SpinsRemaining > 0; }
	int32 NumRounds() const { return Quotas.Num(); }

private:
	FRandomStream Rng;
	int32 Budget = 5;
	TArray<int32> Quotas;
	TArray<FPaylineDef> PaylinePool;                         // patterns not yet active (buyable)
	TArray<TStrongObjectPtr<UCarouselCharm>> CharmInstances; // kept alive without a UPROPERTY
	FCarouselSim Sim;
	FSpinContext SpinCtx;

	void BeginRound();             // set quota/curse/budget for RoundIndex, Phase=Spinning
	void ClearRound();             // award currency, open the shop
	void GenerateOfferings();
	void ApplyItem(const FShopItem& Item);
	void RebuildCharms();          // (re)create charm instances from Build.OwnedCharms; repoint SpinCtx
	void RepointContext();         // point SpinCtx at Build/Symbols/Rng/charms (stable across edits)
};
