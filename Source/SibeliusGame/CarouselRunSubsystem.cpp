// CarouselRunSubsystem.cpp — SIB-46 live-play wrapper. See header. Forwards to FCarouselRun and
// broadcasts events on phase transitions so presentation can react.

#include "CarouselRunSubsystem.h"

void UCarouselRunSubsystem::StartRun(int32 Seed)
{
	Run.StartRun(Seed);
	OnShopOpened.Clear();   // (no-op safety; fresh run)
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

	if (Before == ECarouselRunPhase::Spinning && Run.Phase == ECarouselRunPhase::Shop)
	{
		OnRoundCleared.Broadcast(RoundBefore, Run.LastReward);
		OnShopOpened.Broadcast();
	}
	else if (Run.Phase == ECarouselRunPhase::Lost)
	{
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
		OnRunEnded.Broadcast(/*bWon=*/true);
	}
	return true;
}
