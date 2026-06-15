// CarouselCharm.h
//
// SIB-46 — the Charm ("Joker" equivalent) behavior layer. FCharmDef (CarouselTypes.h) is the DATA;
// each slice Charm's EFFECT is a small UCarouselCharm subclass keyed by Id, reacting to pipeline
// phases via OnEvent. Long-term the common effects go data-driven (op enum + params); for the slice
// they're code. Charms are created ONCE per run (not per spin) — the hot sim loop allocates nothing.
//
// Resolution order = FMachineBuild::OwnedCharms order (a Charm earlier in the list resolves first).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CarouselTypes.h"
#include "CarouselCharm.generated.h"

UCLASS(Abstract)
class SIBELIUSGAME_API UCarouselCharm : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY() FName CharmId;

	// Called for every phase; subclasses switch on Trigger. Mutates Ctx (the spin's working state).
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) {}

	// Clear cross-spin internal state at the start of a run (SimRuns reuses instances across runs).
	virtual void Reset() {}

	// Factory: map a FCharmDef Id to its effect subclass. Unknown/unimplemented Ids return a no-op
	// base instance (logged once) so a build referencing a not-yet-coded Charm never crashes the sim.
	static UCarouselCharm* Make(const FName& Id, UObject* Outer);
};

// --- Slice Charms (representative set proving the pipeline across phases) -------------------------

// Wildfire — Flame symbols also act as Wild. (Wilds build). OnSpinStart.
UCLASS()
class UCharm_Wildfire : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
};

// Mundane Riches — low symbols (Cog/Key/Coin) pay double. (Cheap-build). OnSpinStart sets boosts.
UCLASS()
class UCharm_MundaneRiches : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
};

// High Roller — +50% payouts (the quota side lives in the run loop). (Volatility). OnSpinStart.
UCLASS()
class UCharm_HighRoller : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
};

// Compounder — +0.5x global mult per consecutive winning spin; resets on a dud. (Snowball).
// Stateful across spins (created once per run). OnSpinStart applies, OnSpinResolved updates the streak.
UCLASS()
class UCharm_Compounder : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
	virtual void Reset() override { Streak = 0; }
private:
	int32 Streak = 0;
};

// Cascade — on any line win, re-spin that line's reels once. (Chain build). OnLineWin requests the
// re-spin; the sim's phase-6 cascade loop (depth-capped) consumes ReelsToRespin.
UCLASS()
class UCharm_Cascade : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
};

// Scatter Shrine — Carousel scatter triggers on 2 instead of 3. (Free-spin loop). OnSpinStart.
UCLASS()
class UCharm_ScatterShrine : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
};

// Twin Reels — duplicate the center reel onto its neighbor for more matching density. (Density).
// OnReelResolved (after the last reel lands, so the copy isn't overwritten by a later draw).
UCLASS()
class UCharm_TwinReels : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
};

// Sticky Fate — Wilds lock in place for the NEXT spin (one spin only). (Setup). Stateful: records
// naturally-landed wild cells OnSpinResolved, forces them OnSpinStart, then they expire.
UCLASS()
class UCharm_StickyFate : public UCarouselCharm
{
	GENERATED_BODY()
public:
	virtual void OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx) override;
	virtual void Reset() override { Pending.Reset(); AppliedThisSpin.Reset(); }
private:
	TMap<int32, FName> Pending;       // wild cells to force on the next spin
	TSet<int32> AppliedThisSpin;      // cells we forced this spin (so they expire, not re-stick)
};

// Near-Miss Mercy — miss the Quota by <10% -> refund one spin (once per round). (Comeback).
// RUN-LEVEL: the effect lives in FCarouselRun (it needs the round's quota + refund state), so the
// spin-pipeline OnEvent is intentionally empty. This shell just makes the Charm a known, ownable Id.
UCLASS()
class UCharm_NearMissMercy : public UCarouselCharm
{
	GENERATED_BODY()
};

// Hoarder — +interest rate (and cap) on banked currency. (Economy). RUN-LEVEL: applied in
// FCarouselRun::ClearRound; the spin-pipeline OnEvent is intentionally empty.
UCLASS()
class UCharm_Hoarder : public UCarouselCharm
{
	GENERATED_BODY()
};
