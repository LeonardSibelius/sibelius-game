// CarouselRun.cpp — SIB-46 run/round/quota/currency + shop state machine. See header.

#include "CarouselRun.h"
#include "CarouselCharm.h"
#include "UObject/Package.h"   // GetTransientPackage
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogCarouselRun, Log, All);

namespace CarouselRunNS
{
	// The full 10 slice Charms (effects coded for a subset; the rest are no-op shells until written).
	static const TCHAR* AllCharms[] = {
		TEXT("Wildfire"), TEXT("Cascade"), TEXT("Compounder"), TEXT("MundaneRiches"), TEXT("HighRoller"),
		TEXT("ScatterShrine"), TEXT("NearMissMercy"), TEXT("Hoarder"), TEXT("TwinReels"), TEXT("StickyFate")
	};

	// Walt 2026-07-17: shop offers must explain themselves — "Charm: HighRoller"
	// told him nothing. Every offer label says what the thing DOES, in the words
	// the HUD already taught (chips, coins, pulls, the round bar).
	static FString CharmOfferLabel(const FName& Id)
	{
		const FString S = Id.ToString();
		if (S == TEXT("Wildfire"))      { return TEXT("Wildfire charm — Flame symbols match anything, like a Wild"); }
		if (S == TEXT("Cascade"))       { return TEXT("Cascade charm — every line win re-spins the last reel for more"); }
		if (S == TEXT("Compounder"))    { return TEXT("Compounder charm — each win in a row pays +0.5x more (dud resets it)"); }
		if (S == TEXT("MundaneRiches")) { return TEXT("Mundane Riches charm — Cog, Key and Coin pay double"); }
		if (S == TEXT("HighRoller"))    { return TEXT("High Roller charm — all wins pay +50%, but round bars grow 25%"); }
		if (S == TEXT("ScatterShrine")) { return TEXT("Scatter Shrine charm — free pulls trigger on 2 Carousels, not 3"); }
		if (S == TEXT("NearMissMercy")) { return TEXT("Near-Miss Mercy charm — bust within 10% of the bar: one extra pull"); }
		if (S == TEXT("Hoarder"))       { return TEXT("Hoarder charm — banked coins earn better interest between rounds"); }
		if (S == TEXT("TwinReels"))     { return TEXT("Twin Reels charm — the middle reel copies itself onto its neighbor"); }
		if (S == TEXT("StickyFate"))    { return TEXT("Sticky Fate charm — Wilds that land stay put for one more pull"); }
		return FString::Printf(TEXT("%s charm"), *S);
	}

	// Extra payline patterns (beyond the starter 5) the shop can sell — the slot's 15-line set tail.
	static const int32 ExtraLines[10][5] = {
		{0,0,1,2,2}, {2,2,1,0,0}, {1,0,1,2,1}, {1,2,1,0,1}, {0,1,1,1,0},
		{2,1,1,1,2}, {1,0,0,0,1}, {1,2,2,2,1}, {0,1,0,1,0}, {2,1,2,1,2}
	};
}

void FCarouselRun::RepointContext()
{
	Sim.Build = &Build;
	Sim.Symbols = &Symbols;
	SpinCtx.Build = &Build;
	SpinCtx.Symbols = &Symbols;
	SpinCtx.Rng = &Rng;
	SpinCtx.Charms.Reset();
	for (const TStrongObjectPtr<UCarouselCharm>& C : CharmInstances)
	{
		if (C.IsValid()) { SpinCtx.Charms.Add(C.Get()); }
	}
}

void FCarouselRun::RebuildCharms()
{
	CharmInstances.Reset();   // TStrongObjectPtr release => unroot the old set
	for (const FName& Id : Build.OwnedCharms)
	{
		CharmInstances.Add(TStrongObjectPtr<UCarouselCharm>(UCarouselCharm::Make(Id, GetTransientPackage())));
	}
	RepointContext();
}

void FCarouselRun::LoadCharmCosts()
{
	CharmCostById.Reset();
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_CarouselCharms.DT_CarouselCharms"));
	if (!Table) { return; }   // fallback: GenerateOfferings uses the flat CharmCost
	TArray<FCharmDef*> Rows;
	Table->GetAllRows<FCharmDef>(TEXT("CarouselRun"), Rows);
	for (const FCharmDef* Row : Rows)
	{
		if (Row && !Row->Id.IsNone())
		{
			CharmCostById.Add(Row->Id, Row->ShopCost > 0 ? Row->ShopCost : CharmCost);
		}
	}
}

void FCarouselRun::StartRun(int32 Seed, const TArray<FName>& ExtraCharms)
{
	Rng.Initialize(Seed);
	FCarouselSim::BuildDefaultSlice(Symbols, Build);
	LoadCharmCosts();
	for (const FName& Id : ExtraCharms) { Build.OwnedCharms.AddUnique(Id); }

	Quotas = FCarouselSim::DefaultQuotas();
	Budget = FCarouselSim::DefaultSpinBudget();

	// Buyable payline pool (patterns not already active).
	PaylinePool.Reset();
	for (int32 i = 0; i < 10; ++i)
	{
		FPaylineDef L;
		for (int32 r = 0; r < Build.NumReels; ++r) { L.Rows.Add(CarouselRunNS::ExtraLines[i][r]); }
		PaylinePool.Add(L);
	}

	Currency = StartingCurrency;
	RoundIndex = 0;
	LastReward = 0;
	SpinCtx = FSpinContext();
	SpinCtx.FreeSpinsRemaining = 0;
	RebuildCharms();          // also repoints the context
	BeginRound();
}

void FCarouselRun::BeginRound()
{
	CurrentQuota = Quotas.IsValidIndex(RoundIndex) ? Quotas[RoundIndex] : 0;
	if (Build.OwnedCharms.Contains(FName(TEXT("HighRoller")))) { CurrentQuota = CurrentQuota * 125 / 100; }

	// Boss curses (slice uses 2): round 4 = The Debuff, round 8 = The Drought.
	CurrentCurse = FBossCurse();
	if (RoundIndex == 3) { CurrentCurse.DebuffedSymbol = TEXT("Dragon"); }
	if (RoundIndex == 7) { CurrentCurse.bScattersDisabled = true; }
	SpinCtx.Curse = CurrentCurse;

	SpinsRemaining = Budget;
	RoundChips = 0;
	bMercyUsedThisRound = false;
	SpinCtx.FreeSpinsRemaining = 0;   // free spins are a within-round bonus; start each round fresh
	Phase = ECarouselRunPhase::Spinning;
}

bool FCarouselRun::Spin()
{
	if (!CanSpin()) { return false; }

	RepointContext();
	Sim.Spin(SpinCtx);
	LastSpin = FCarouselSim::MakeResult(SpinCtx);
	RoundChips += SpinCtx.SpinPayout;
	if (!LastSpin.bWasFreeSpin)
	{
		--SpinsRemaining;   // a free spin (from a scatter) doesn't cost a budget spin
	}

	if (RoundChips >= CurrentQuota)
	{
		ClearRound();
	}
	else if (SpinsRemaining <= 0 && SpinCtx.FreeSpinsRemaining <= 0)
	{
		// Near-Miss Mercy: missed by < 10% -> refund one spin (once per round) instead of busting.
		if (!bMercyUsedThisRound
			&& Build.OwnedCharms.Contains(FName(TEXT("NearMissMercy")))
			&& RoundChips * 10 >= CurrentQuota * 9)
		{
			bMercyUsedThisRound = true;
			SpinsRemaining += 1;   // stay in Spinning for one more shot
			UE_LOG(LogCarouselRun, Display, TEXT("    Near-Miss Mercy: refunded a spin (%d/%d, within 10%%)"),
				RoundChips, CurrentQuota);
		}
		else
		{
			Phase = ECarouselRunPhase::Lost;
		}
	}
	return true;
}

void FCarouselRun::ClearRound()
{
	const int32 Unused = FMath::Max(0, SpinsRemaining);
	// Hoarder: + interest rate and cap on the banked currency (economy archetype).
	int32 Rate = InterestRatePct, Cap = InterestCap;
	if (Build.OwnedCharms.Contains(FName(TEXT("Hoarder")))) { Rate += 20; Cap += 5; }
	const int32 Interest = FMath::Min(Currency * Rate / 100, Cap);
	LastReward = ClearBaseReward + PerUnusedSpinBonus * Unused + Interest;
	Currency += LastReward;
	GenerateOfferings();
	Phase = ECarouselRunPhase::Shop;
}

void FCarouselRun::GenerateOfferings()
{
	Offerings.Reset();

	// Pools available this shop.
	TArray<FName> BuyableCharms;
	for (const TCHAR* C : CarouselRunNS::AllCharms)
	{
		const FName Id(C);
		if (!Build.OwnedCharms.Contains(Id)) { BuyableCharms.Add(Id); }
	}
	TArray<FName> SymbolIds;
	for (const TPair<FName, FSymbolDef>& P : Symbols) { SymbolIds.Add(P.Key); }
	SymbolIds.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

	for (int32 i = 0; i < ShopSize; ++i)
	{
		// Pick a type that has stock (charm -> payline -> symbol fallback).
		int32 Roll = Rng.RandRange(0, 2);
		for (int32 Tries = 0; Tries < 3; ++Tries, Roll = (Roll + 1) % 3)
		{
			if (Roll == 0 && BuyableCharms.Num() > 0)
			{
				const int32 Idx = Rng.RandRange(0, BuyableCharms.Num() - 1);
				FShopItem It; It.Type = EShopItemType::Charm; It.Id = BuyableCharms[Idx];
				It.Cost = CharmCostById.Contains(It.Id) ? CharmCostById[It.Id] : CharmCost;
				It.Label = FText::FromString(CarouselRunNS::CharmOfferLabel(It.Id));
				Offerings.Add(It);
				BuyableCharms.RemoveAt(Idx);   // no dupes in one shop
				break;
			}
			if (Roll == 1 && PaylinePool.Num() > 0)
			{
				FShopItem It; It.Type = EShopItemType::Payline; It.Cost = PaylineCost;
				It.Label = FText::FromString(TEXT("Extra payline — one more line across the reels that can win"));
				Offerings.Add(It);
				break;
			}
			if (Roll == 2 && SymbolIds.Num() > 0 && Build.NumReels > 0)
			{
				const FName SymId = SymbolIds[Rng.RandRange(0, SymbolIds.Num() - 1)];
				const int32 Reel = Rng.RandRange(0, Build.NumReels - 1);
				FShopItem It; It.Type = EShopItemType::Symbol; It.Id = SymId; It.TargetReel = Reel; It.Cost = SymbolCost;
				// Reels are numbered 1-based for the player (reel 0 is programmer-speak).
				It.Label = FText::FromString(FString::Printf(TEXT("Add a %s to reel %d — it will land there more often"), *SymId.ToString(), Reel + 1));
				Offerings.Add(It);
				break;
			}
		}
	}
}

bool FCarouselRun::Reroll()
{
	if (Phase != ECarouselRunPhase::Shop || Currency < RerollCost) { return false; }
	Currency -= RerollCost;
	GenerateOfferings();
	return true;
}

bool FCarouselRun::Buy(int32 OfferIndex)
{
	if (Phase != ECarouselRunPhase::Shop || !Offerings.IsValidIndex(OfferIndex)) { return false; }
	const FShopItem Item = Offerings[OfferIndex];
	if (Currency < Item.Cost) { return false; }
	Currency -= Item.Cost;
	ApplyItem(Item);
	Offerings.RemoveAt(OfferIndex);
	return true;
}

void FCarouselRun::ApplyItem(const FShopItem& Item)
{
	switch (Item.Type)
	{
	case EShopItemType::Charm:
		Build.OwnedCharms.AddUnique(Item.Id);
		RebuildCharms();
		break;
	case EShopItemType::Payline:
		if (PaylinePool.Num() > 0)
		{
			Build.ActivePaylines.Add(PaylinePool[0]);
			PaylinePool.RemoveAt(0);
		}
		break;
	case EShopItemType::Symbol:
		if (Build.ReelStrips.IsValidIndex(Item.TargetReel))
		{
			// Add a few copies so the buy meaningfully raises that symbol's weight on the reel.
			for (int32 k = 0; k < 4; ++k) { Build.ReelStrips[Item.TargetReel].Symbols.Add(Item.Id); }
		}
		break;
	}
}

bool FCarouselRun::AdvanceToNextRound()
{
	if (Phase != ECarouselRunPhase::Shop) { return false; }
	++RoundIndex;
	if (RoundIndex >= Quotas.Num())
	{
		Phase = ECarouselRunPhase::Won;
	}
	else
	{
		BeginRound();
	}
	return true;
}

/* ------------------------------------------------------------------ demo console */

static void CarouselRunDemoConsole(const TArray<FString>& Args)
{
	const int32 Seed = (Args.Num() >= 1) ? FCString::Atoi(*Args[0]) : 7;

	// Optional 2nd arg: comma-separated Charm Ids granted at start (debug/tuning a specific combo).
	TArray<FName> Extra;
	if (Args.Num() >= 2)
	{
		TArray<FString> Parts;
		Args[1].ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts) { const FString T = P.TrimStartAndEnd(); if (!T.IsEmpty()) { Extra.Add(FName(*T)); } }
	}

	FCarouselRun Run;
	Run.StartRun(Seed, Extra);

	FString CharmStr;
	for (const FName& Id : Run.Build.OwnedCharms) { CharmStr += Id.ToString() + TEXT(" "); }
	UE_LOG(LogCarouselRun, Display, TEXT("[Carousel] RunDemo seed=%d charms=[ %s]: %d rounds, currency=%d (auto-buy cheapest)."),
		Seed, *CharmStr, Run.NumRounds(), Run.Currency);

	int32 Guard = 0;
	int32 RoundSpins = 0;
	while (Run.Phase != ECarouselRunPhase::Won && Run.Phase != ECarouselRunPhase::Lost && Guard++ < 2000)
	{
		if (Run.Phase == ECarouselRunPhase::Spinning)
		{
			const int32 RoundNum = Run.RoundIndex + 1;
			const int32 Quota = Run.CurrentQuota;
			Run.Spin();
			++RoundSpins;
			if (Run.Phase != ECarouselRunPhase::Spinning)   // round resolved (cleared or busted)
			{
				const bool bCleared = (Run.Phase == ECarouselRunPhase::Shop);
				UE_LOG(LogCarouselRun, Display, TEXT("  round %d: %s  chips=%d/%d in %d spins%s"),
					RoundNum, bCleared ? TEXT("CLEAR") : TEXT("BUST"), Run.RoundChips, Quota, RoundSpins,
					bCleared ? *FString::Printf(TEXT("  +%d currency (bank=%d)"), Run.LastReward, Run.Currency) : TEXT(""));
				RoundSpins = 0;
			}
		}
		else if (Run.Phase == ECarouselRunPhase::Shop)
		{
			// Auto-buy the cheapest affordable offering; otherwise leave for the next round.
			int32 Best = INDEX_NONE, BestCost = MAX_int32;
			for (int32 i = 0; i < Run.Offerings.Num(); ++i)
			{
				if (Run.Offerings[i].Cost <= Run.Currency && Run.Offerings[i].Cost < BestCost)
				{
					Best = i; BestCost = Run.Offerings[i].Cost;
				}
			}
			if (Best != INDEX_NONE)
			{
				UE_LOG(LogCarouselRun, Display, TEXT("    shop: buy '%s' (-%d, bank=%d)"),
					*Run.Offerings[Best].Label.ToString(), Run.Offerings[Best].Cost, Run.Currency - Run.Offerings[Best].Cost);
				Run.Buy(Best);
			}
			else
			{
				Run.AdvanceToNextRound();
			}
		}
	}

	UE_LOG(LogCarouselRun, Display, TEXT("[Carousel] RunDemo result: %s — reached round %d/%d, bank=%d, charms=%d, paylines=%d"),
		(Run.Phase == ECarouselRunPhase::Won) ? TEXT("WON") : TEXT("LOST"),
		FMath::Min(Run.RoundIndex + 1, Run.NumRounds()), Run.NumRounds(),
		Run.Currency, Run.Build.OwnedCharms.Num(), Run.Build.ActivePaylines.Num());

	if (FApp::IsUnattended()) { RequestEngineExit(TEXT("carousel.RunDemo complete")); }
}

static FAutoConsoleCommand GCarouselRunDemoCmd(
	TEXT("carousel.RunDemo"),
	TEXT("Play one Carousel run headlessly (auto-buy cheapest shop item). Usage: carousel.RunDemo [seed]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&CarouselRunDemoConsole));
