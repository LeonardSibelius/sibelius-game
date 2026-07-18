// PokerGameModel.cpp — SIDE_GAMES G5. See header.

#include "PokerGameModel.h"

void UPokerGameModel::Init(int32 Seed)
{
	Rng.Initialize(Seed);
	bAwaitingDraw = false;
	bInitialized = true;
}

TArray<int32> UPokerGameModel::Deal()
{
	checkf(bInitialized, TEXT("Call Init(Seed) before Deal()"));

	// Fresh deck, Fisher-Yates with the seeded stream.
	Deck.SetNum(DECK_SIZE);
	for (int32 i = 0; i < DECK_SIZE; ++i) { Deck[i] = i; }
	for (int32 i = DECK_SIZE - 1; i > 0; --i)
	{
		const int32 j = Rng.RandRange(0, i);
		Deck.Swap(i, j);
	}

	Hand.SetNum(HAND_SIZE);
	for (int32 i = 0; i < HAND_SIZE; ++i) { Hand[i] = Deck[i]; }
	DeckCursor = HAND_SIZE;
	bAwaitingDraw = true;
	return Hand;
}

FPokerHandResult UPokerGameModel::Draw(int32 HoldMask)
{
	checkf(bAwaitingDraw, TEXT("Draw() without a pending Deal()"));

	for (int32 i = 0; i < HAND_SIZE; ++i)
	{
		if (!(HoldMask & (1 << i)))
		{
			Hand[i] = Deck[DeckCursor++];
		}
	}
	bAwaitingDraw = false;

	FPokerHandResult Out;
	Out.Cards = Hand;
	Out.Rank = EvaluateHand(Hand);
	Out.PayMultiplier = PayMultiplier(Out.Rank);
	return Out;
}

EPokerHandRank UPokerGameModel::EvaluateHand(const TArray<int32>& Cards)
{
	if (Cards.Num() != HAND_SIZE) { return EPokerHandRank::Nothing; }

	int32 RankCount[13] = {};
	int32 SuitCount[4] = {};
	for (const int32 C : Cards)
	{
		++RankCount[RankOf(C)];
		++SuitCount[SuitOf(C)];
	}

	bool bFlush = false;
	for (const int32 S : SuitCount) { if (S == HAND_SIZE) { bFlush = true; } }

	// Distinct ranks, ascending.
	TArray<int32> Distinct;
	for (int32 r = 0; r < 13; ++r) { if (RankCount[r] > 0) { Distinct.Add(r); } }

	bool bStraight = false, bAceHigh = false;
	if (Distinct.Num() == HAND_SIZE)
	{
		if (Distinct.Last() - Distinct[0] == 4)
		{
			bStraight = true;
			bAceHigh = (Distinct[0] == 8);   // T J Q K A
		}
		else if (Distinct[0] == 0 && Distinct[1] == 1 && Distinct[2] == 2 && Distinct[3] == 3 && Distinct[4] == 12)
		{
			bStraight = true;                // the wheel: A 2 3 4 5 (ace low)
		}
	}

	int32 Four = 0, Three = 0, Pairs = 0, HighPairRank = -1;
	for (int32 r = 0; r < 13; ++r)
	{
		if (RankCount[r] == 4) { Four = 1; }
		else if (RankCount[r] == 3) { Three = 1; }
		else if (RankCount[r] == 2) { ++Pairs; HighPairRank = FMath::Max(HighPairRank, r); }
	}

	if (bStraight && bFlush && bAceHigh) { return EPokerHandRank::RoyalFlush; }
	if (bStraight && bFlush)             { return EPokerHandRank::StraightFlush; }
	if (Four)                            { return EPokerHandRank::FourOfAKind; }
	if (Three && Pairs == 1)             { return EPokerHandRank::FullHouse; }
	if (bFlush)                          { return EPokerHandRank::Flush; }
	if (bStraight)                       { return EPokerHandRank::Straight; }
	if (Three)                           { return EPokerHandRank::ThreeOfAKind; }
	if (Pairs == 2)                      { return EPokerHandRank::TwoPair; }
	if (Pairs == 1 && HighPairRank >= 9) { return EPokerHandRank::JacksOrBetter; }   // J Q K A
	return EPokerHandRank::Nothing;
}

int32 UPokerGameModel::PayMultiplier(EPokerHandRank Rank)
{
	// The FULL-PAY 9/6 table — the model's ONLY tuning point.
	switch (Rank)
	{
	case EPokerHandRank::JacksOrBetter: return 1;
	case EPokerHandRank::TwoPair:       return 2;
	case EPokerHandRank::ThreeOfAKind:  return 3;
	case EPokerHandRank::Straight:      return 4;
	case EPokerHandRank::Flush:         return 6;
	case EPokerHandRank::FullHouse:     return 9;
	case EPokerHandRank::FourOfAKind:   return 25;
	case EPokerHandRank::StraightFlush: return 50;
	case EPokerHandRank::RoyalFlush:    return 250;
	default:                            return 0;
	}
}

const TCHAR* UPokerGameModel::RankDisplayName(EPokerHandRank Rank)
{
	switch (Rank)
	{
	case EPokerHandRank::JacksOrBetter: return TEXT("JACKS OR BETTER");
	case EPokerHandRank::TwoPair:       return TEXT("TWO PAIR");
	case EPokerHandRank::ThreeOfAKind:  return TEXT("THREE OF A KIND");
	case EPokerHandRank::Straight:      return TEXT("STRAIGHT");
	case EPokerHandRank::Flush:         return TEXT("FLUSH");
	case EPokerHandRank::FullHouse:     return TEXT("FULL HOUSE");
	case EPokerHandRank::FourOfAKind:   return TEXT("FOUR OF A KIND");
	case EPokerHandRank::StraightFlush: return TEXT("STRAIGHT FLUSH");
	case EPokerHandRank::RoyalFlush:    return TEXT("ROYAL FLUSH");
	default:                            return TEXT("NOTHING");
	}
}
