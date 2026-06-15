// CarouselCharm.cpp — SIB-46 slice Charm effects. See header.

#include "CarouselCharm.h"

DEFINE_LOG_CATEGORY_STATIC(LogCarouselCharm, Log, All);

UCarouselCharm* UCarouselCharm::Make(const FName& Id, UObject* Outer)
{
	UCarouselCharm* Charm = nullptr;
	if (Id == TEXT("Wildfire"))            { Charm = NewObject<UCharm_Wildfire>(Outer); }
	else if (Id == TEXT("MundaneRiches"))  { Charm = NewObject<UCharm_MundaneRiches>(Outer); }
	else if (Id == TEXT("HighRoller"))     { Charm = NewObject<UCharm_HighRoller>(Outer); }
	else if (Id == TEXT("Compounder"))     { Charm = NewObject<UCharm_Compounder>(Outer); }
	else if (Id == TEXT("Cascade"))        { Charm = NewObject<UCharm_Cascade>(Outer); }
	else
	{
		// Not-yet-coded Charm (e.g. ScatterShrine, NearMissMercy, Hoarder, TwinReels, StickyFate):
		// a no-op base so a build that owns it still sims. Effect lands when its subclass is added.
		UE_LOG(LogCarouselCharm, Warning, TEXT("[Carousel] Charm '%s' has no coded effect yet — no-op."), *Id.ToString());
		Charm = NewObject<UCarouselCharm>(Outer);
	}
	Charm->CharmId = Id;
	return Charm;
}

void UCharm_Wildfire::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger == ECharmTrigger::OnSpinStart)
	{
		Ctx.ExtraWildSymbols.Add(TEXT("Flame"));   // Flame substitutes like a Wild this spin
	}
}

void UCharm_MundaneRiches::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger == ECharmTrigger::OnSpinStart)
	{
		Ctx.SymbolPayoutBoostPercent.Add(TEXT("Cog"), 200);
		Ctx.SymbolPayoutBoostPercent.Add(TEXT("Key"), 200);
		Ctx.SymbolPayoutBoostPercent.Add(TEXT("Coin"), 200);
	}
}

void UCharm_HighRoller::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger == ECharmTrigger::OnSpinStart)
	{
		Ctx.GlobalMultiplierPercent += 50;   // +50% payouts (Quota side is applied in the run loop)
	}
}

void UCharm_Compounder::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger == ECharmTrigger::OnSpinStart)
	{
		Ctx.GlobalMultiplierPercent += 50 * Streak;   // +0.5x per consecutive prior win
	}
	else if (Trigger == ECharmTrigger::OnSpinResolved)
	{
		Streak = (Ctx.SpinPayout > 0) ? (Streak + 1) : 0;   // snowball, reset on a dud
	}
}

void UCharm_Cascade::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger == ECharmTrigger::OnLineWin && Ctx.Build)
	{
		// Re-spin the rightmost reel to chase a longer chain. Depth-capped by the sim (no infinite loop).
		Ctx.ReelsToRespin.Add(Ctx.Build->NumReels - 1);
	}
}
