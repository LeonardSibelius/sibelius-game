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
