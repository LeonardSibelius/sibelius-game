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
	else if (Id == TEXT("ScatterShrine"))  { Charm = NewObject<UCharm_ScatterShrine>(Outer); }
	else if (Id == TEXT("TwinReels"))      { Charm = NewObject<UCharm_TwinReels>(Outer); }
	else if (Id == TEXT("StickyFate"))     { Charm = NewObject<UCharm_StickyFate>(Outer); }
	else if (Id == TEXT("NearMissMercy"))  { Charm = NewObject<UCharm_NearMissMercy>(Outer); }  // effect in FCarouselRun
	else if (Id == TEXT("Hoarder"))        { Charm = NewObject<UCharm_Hoarder>(Outer); }         // effect in FCarouselRun
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

void UCharm_ScatterShrine::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger == ECharmTrigger::OnSpinStart)
	{
		Ctx.ScatterThreshold = FMath::Min(Ctx.ScatterThreshold, 2);   // 2 scatters trigger, not 3
	}
}

void UCharm_TwinReels::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger != ECharmTrigger::OnReelResolved || !Ctx.Build) { return; }
	const int32 N = Ctx.Build->NumReels;
	const int32 Rows = Ctx.Build->RowsVisible;
	if (Ctx.EventReel != N - 1) { return; }   // wait until every reel has landed, then twin

	const int32 Center = N / 2;
	const int32 Dest = Center + 1;
	if (Dest >= N) { return; }
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const int32 Src = Center * Rows + Row;
		const int32 Dst = Dest * Rows + Row;
		if (Ctx.Grid.IsValidIndex(Src) && Ctx.Grid.IsValidIndex(Dst))
		{
			Ctx.Grid[Dst] = Ctx.Grid[Src];   // duplicate the center reel onto its neighbor (density)
		}
	}
}

void UCharm_StickyFate::OnEvent(ECharmTrigger Trigger, FSpinContext& Ctx)
{
	if (Trigger == ECharmTrigger::OnSpinStart)
	{
		// Force last spin's recorded wilds to land again this spin (one spin only).
		AppliedThisSpin.Reset();
		for (const TPair<int32, FName>& Cell : Pending)
		{
			Ctx.ForcedCells.Add(Cell.Key, Cell.Value);
			AppliedThisSpin.Add(Cell.Key);
		}
		Pending.Reset();
	}
	else if (Trigger == ECharmTrigger::OnSpinResolved)
	{
		// Record naturally-landed wilds for next spin; the ones we forced this spin expire (not re-stuck).
		Pending.Reset();
		for (int32 i = 0; i < Ctx.Grid.Num(); ++i)
		{
			if (AppliedThisSpin.Contains(i)) { continue; }
			const FSymbolDef* D = Ctx.Find(Ctx.Grid[i]);
			if (D && (D->Type == ESymbolType::Wild || D->bSubstitutesAsWild))
			{
				Pending.Add(i, Ctx.Grid[i]);
			}
		}
	}
}
