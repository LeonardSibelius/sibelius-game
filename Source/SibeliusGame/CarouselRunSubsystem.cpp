// CarouselRunSubsystem.cpp — SIB-46 live-play wrapper. See header. Forwards to FCarouselRun and
// broadcasts events on phase transitions so presentation can react.
// FUN-4: staked entry + sauce settlement on run end.

#include "CarouselRunSubsystem.h"
#include "ProgressionSubsystem.h"
#include "Engine/Engine.h"

void UCarouselRunSubsystem::StartRun(int32 Seed)
{
	// An unstaked (re)start abandons any pending stake — it was already charged,
	// so treat it as a lost run (no free escape from a bad board).
	if (bStakedRun)
	{
		SettleStake(/*bWon=*/false);
	}
	Run.StartRun(Seed);
	OnShopOpened.Clear();   // (no-op safety; fresh run)
}

bool UCarouselRunSubsystem::StartStakedRun(int32 Seed)
{
	UProgressionSubsystem* Progression =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UProgressionSubsystem>() : nullptr;
	if (!Progression)
	{
		StartRun(Seed);   // bare slice: free play, nothing to settle
		return true;
	}

	if (bStakedRun)
	{
		SettleStake(/*bWon=*/false);   // restarting mid-run forfeits the old stake
	}
	if (!Progression->TrySpendSauce(EntryStake))
	{
		return false;
	}
	Run.StartRun(Seed);
	bStakedRun = true;
	Progression->BumpStat(SibeliusStats::CarouselRuns);   // APPEAL-5
	return true;
}

void UCarouselRunSubsystem::SettleStake(bool bWon)
{
	if (!bStakedRun)
	{
		return;
	}
	bStakedRun = false;

	UProgressionSubsystem* Progression =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UProgressionSubsystem>() : nullptr;
	if (!Progression)
	{
		return;
	}

	// APPEAL-5: the records the player returns to beat. Cleared rounds =
	// every round on a win, RoundIndex (0-based) on a loss.
	if (bWon)
	{
		Progression->BumpStat(SibeliusStats::CarouselWins);
	}
	Progression->RaiseStat(SibeliusStats::CarouselBestRound, bWon ? Run.NumRounds() : Run.RoundIndex);

	// Won: the payout plus the leftover bank converts to Sauce. Lost: a small
	// consolation per cleared round (RoundIndex is 0-based = rounds cleared).
	const int32 Payout = bWon
		? WinPayout + Run.Currency * SaucePerLeftoverCurrency
		: Run.RoundIndex * ConsolationPerClearedRound;

	if (Payout > 0)
	{
		Progression->GrantSauce(Payout);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, bWon ? FColor::Green : FColor::Silver,
			bWon
				? FString::Printf(TEXT("THE CAROUSEL PAYS  +%d SAUCE  (total %d)"), Payout, Progression->GetSauce())
				: FString::Printf(TEXT("The Carousel keeps its stake.  Consolation +%d sauce (total %d)"), Payout, Progression->GetSauce()));
	}
}

bool UCarouselRunSubsystem::Spin()
{
	const ECarouselRunPhase Before = Run.Phase;
	const int32 RoundBefore = Run.RoundIndex;
	if (!Run.Spin())
	{
		return false;
	}

	OnSpinResolved.Broadcast(Run.LastSpin);

	// APPEAL-5: biggest single spin, staked runs only (free grey-box play has
	// no progression subsystem, so this is naturally a no-op there).
	if (bStakedRun)
	{
		if (UProgressionSubsystem* Progression =
			GetGameInstance() ? GetGameInstance()->GetSubsystem<UProgressionSubsystem>() : nullptr)
		{
			Progression->RaiseStat(SibeliusStats::CarouselBestSpin, Run.LastSpin.SpinPayout);
		}
	}

	if (Before == ECarouselRunPhase::Spinning && Run.Phase == ECarouselRunPhase::Shop)
	{
		OnRoundCleared.Broadcast(RoundBefore, Run.LastReward);
		OnShopOpened.Broadcast();
	}
	else if (Run.Phase == ECarouselRunPhase::Lost)
	{
		SettleStake(/*bWon=*/false);   // FUN-4: before the broadcast, so listeners see the new balance
		OnRunEnded.Broadcast(/*bWon=*/false);
	}
	return true;
}

bool UCarouselRunSubsystem::Reroll()
{
	return Run.Reroll();
}

bool UCarouselRunSubsystem::BuyOffering(int32 Index)
{
	return Run.Buy(Index);
}

bool UCarouselRunSubsystem::AdvanceToNextRound()
{
	if (!Run.AdvanceToNextRound())
	{
		return false;
	}
	if (Run.Phase == ECarouselRunPhase::Won)
	{
		SettleStake(/*bWon=*/true);    // FUN-4: before the broadcast, so listeners see the new balance
		OnRunEnded.Broadcast(/*bWon=*/true);
	}
	return true;
}
