// PokerSmokeTestCommandlet.cpp — SIDE_GAMES G5. See header.
//
// [HARD] Evaluator: 14 constructed hands rank exactly right (incl. the wheel,
//        the wheel straight flush, and low-pair-is-Nothing)
// [HARD] Determinism: same seed => identical 200-hand sequence; different differs
// [HARD] Deck honesty: no duplicate card in any deal+draw; all cards 0..51
// [HARD] RTP within the catastrophic band [80%, 105%] under simple strategy
// [WARN] RTP outside the design band [93%, 101%] (full-pay 9/6 with a simple
//        player should land ~95-99%)
// [HARD] S2: UPokerScreenWidget class resolves

#include "PokerSmokeTestCommandlet.h"
#include "PokerGameModel.h"
#include "PokerScreenWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPokerSmoke, Log, All);

namespace PokerSmokeTestNS
{
	const int32 SimHands = 200000;
	const int32 SeedA = 42;
	const int32 SeedB = 1337;

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition) { UE_LOG(LogPokerSmoke, Display, TEXT("  [PASS] %s"), *Label); }
			else { ++Failures; UE_LOG(LogPokerSmoke, Error, TEXT("  [FAIL] %s"), *Label); }
		}
		void Warn(bool bCondition, const FString& Label)
		{
			UE_LOG(LogPokerSmoke, Display, TEXT("  [%s] %s"), bCondition ? TEXT("PASS") : TEXT("WARN"), *Label);
		}
	};

	int32 Card(int32 Rank, int32 Suit) { return Suit * 13 + Rank; }

	// The sim player's simple strategy (deliberately NOT optimal — a human
	// baseline): keep made hands sensibly, chase 4-flushes, keep low pairs,
	// keep high cards. Returns the hold mask.
	int32 SimpleStrategy(const TArray<int32>& Hand)
	{
		const EPokerHandRank Rank = UPokerGameModel::EvaluateHand(Hand);

		// Straight or better (incl. full house / quads / royals): stand pat.
		if (Rank >= EPokerHandRank::Straight) { return 0b11111; }

		int32 RankCount[13] = {};
		for (const int32 C : Hand) { ++RankCount[UPokerGameModel::RankOf(C)]; }

		// Trips: hold the three, draw two.
		if (Rank == EPokerHandRank::ThreeOfAKind)
		{
			int32 Mask = 0;
			for (int32 i = 0; i < Hand.Num(); ++i)
			{
				if (RankCount[UPokerGameModel::RankOf(Hand[i])] == 3) { Mask |= (1 << i); }
			}
			return Mask;
		}

		// Two pair / any pair (high or low): hold the paired cards.
		int32 PairMask = 0;
		for (int32 i = 0; i < Hand.Num(); ++i)
		{
			if (RankCount[UPokerGameModel::RankOf(Hand[i])] == 2) { PairMask |= (1 << i); }
		}
		if (PairMask != 0) { return PairMask; }

		// Four to a flush: hold them.
		int32 SuitCount[4] = {};
		for (const int32 C : Hand) { ++SuitCount[UPokerGameModel::SuitOf(C)]; }
		for (int32 S = 0; S < 4; ++S)
		{
			if (SuitCount[S] == 4)
			{
				int32 Mask = 0;
				for (int32 i = 0; i < Hand.Num(); ++i)
				{
					if (UPokerGameModel::SuitOf(Hand[i]) == S) { Mask |= (1 << i); }
				}
				return Mask;
			}
		}

		// High cards (J+): hold them all, draw the rest.
		int32 HighMask = 0;
		for (int32 i = 0; i < Hand.Num(); ++i)
		{
			if (UPokerGameModel::RankOf(Hand[i]) >= 9) { HighMask |= (1 << i); }
		}
		return HighMask;
	}
}

UPokerSmokeTestCommandlet::UPokerSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UPokerSmokeTestCommandlet::Main(const FString& /*Params*/)
{
	using namespace PokerSmokeTestNS;

	UE_LOG(LogPokerSmoke, Display, TEXT("=== SIDE_GAMES G5 poker model smoke test (the regulator's suite) ==="));

	FResult R;

	/* ---------- the evaluator on known hands ---------- */
	{
		struct FKnown { const TCHAR* Label; TArray<int32> Cards; EPokerHandRank Expect; };
		const int32 S = 0, H = 1, D = 2, C = 3;   // suits
		const TArray<FKnown> Known = {
			{ TEXT("Royal flush (T-A spades)"),        { Card(8,S), Card(9,S), Card(10,S), Card(11,S), Card(12,S) }, EPokerHandRank::RoyalFlush },
			{ TEXT("Straight flush (5-9 hearts)"),     { Card(3,H), Card(4,H), Card(5,H), Card(6,H), Card(7,H) },   EPokerHandRank::StraightFlush },
			{ TEXT("Wheel straight flush (A-5 clubs)"),{ Card(12,C), Card(0,C), Card(1,C), Card(2,C), Card(3,C) },  EPokerHandRank::StraightFlush },
			{ TEXT("Four of a kind (7777K)"),          { Card(5,S), Card(5,H), Card(5,D), Card(5,C), Card(11,S) },  EPokerHandRank::FourOfAKind },
			{ TEXT("Full house (QQQ44)"),              { Card(10,S), Card(10,H), Card(10,D), Card(2,S), Card(2,H) },EPokerHandRank::FullHouse },
			{ TEXT("Flush (hearts, no straight)"),     { Card(0,H), Card(3,H), Card(5,H), Card(7,H), Card(11,H) },  EPokerHandRank::Flush },
			{ TEXT("Straight (6-T mixed)"),            { Card(4,S), Card(5,H), Card(6,D), Card(7,C), Card(8,S) },   EPokerHandRank::Straight },
			{ TEXT("Wheel straight (A-5 mixed)"),      { Card(12,S), Card(0,H), Card(1,D), Card(2,C), Card(3,S) },  EPokerHandRank::Straight },
			{ TEXT("Three of a kind (999J3)"),         { Card(7,S), Card(7,H), Card(7,D), Card(9,C), Card(1,S) },   EPokerHandRank::ThreeOfAKind },
			{ TEXT("Two pair (JJ449)"),                { Card(9,S), Card(9,H), Card(2,D), Card(2,C), Card(7,S) },   EPokerHandRank::TwoPair },
			{ TEXT("Jacks or better (JJ junk)"),       { Card(9,S), Card(9,H), Card(0,D), Card(4,C), Card(7,S) },   EPokerHandRank::JacksOrBetter },
			{ TEXT("Jacks or better (AA junk)"),       { Card(12,S), Card(12,H), Card(0,D), Card(4,C), Card(7,S) }, EPokerHandRank::JacksOrBetter },
			{ TEXT("Low pair (TT) is Nothing"),        { Card(8,S), Card(8,H), Card(0,D), Card(4,C), Card(11,S) },  EPokerHandRank::Nothing },
			{ TEXT("High-card junk is Nothing"),       { Card(0,S), Card(3,H), Card(5,D), Card(7,C), Card(12,S) },  EPokerHandRank::Nothing },
		};
		for (const FKnown& K : Known)
		{
			const EPokerHandRank Got = UPokerGameModel::EvaluateHand(K.Cards);
			R.Check(Got == K.Expect, FString::Printf(TEXT("Evaluator: %s"), K.Label));
		}
	}

	/* ---------- determinism ---------- */
	{
		UPokerGameModel* A = NewObject<UPokerGameModel>();
		UPokerGameModel* B = NewObject<UPokerGameModel>();
		UPokerGameModel* C = NewObject<UPokerGameModel>();
		A->Init(SeedA); B->Init(SeedA); C->Init(SeedB);

		bool bSameIdentical = true, bDiffDiffers = false;
		for (int32 i = 0; i < 200; ++i)
		{
			const TArray<int32> Da = A->Deal(), Db = B->Deal(), Dc = C->Deal();
			const FPokerHandResult Ra = A->Draw(0), Rb = B->Draw(0), Rc = C->Draw(0);
			if (Da != Db || Ra.Cards != Rb.Cards) { bSameIdentical = false; }
			if (Da != Dc || Ra.Cards != Rc.Cards) { bDiffDiffers = true; }
		}
		R.Check(bSameIdentical, TEXT("Determinism: same seed => identical 200-hand sequence"));
		R.Check(bDiffDiffers, TEXT("Sanity: different seed => different sequence"));
	}

	/* ---------- deck honesty ---------- */
	{
		UPokerGameModel* M = NewObject<UPokerGameModel>();
		M->Init(7);
		bool bHonest = true;
		for (int32 i = 0; i < 2000 && bHonest; ++i)
		{
			TSet<int32> Seen;
			const TArray<int32> Dealt = M->Deal();
			for (const int32 CardV : Dealt)
			{
				if (CardV < 0 || CardV >= 52 || Seen.Contains(CardV)) { bHonest = false; }
				Seen.Add(CardV);
			}
			const FPokerHandResult Res = M->Draw(0);   // replace all five
			for (const int32 CardV : Res.Cards)
			{
				if (CardV < 0 || CardV >= 52 || Seen.Contains(CardV)) { bHonest = false; }
				Seen.Add(CardV);
			}
		}
		R.Check(bHonest, TEXT("Deck honesty: 2000 full-replace hands, no duplicate or invalid card"));
	}

	/* ---------- RTP under the simple-strategy player ---------- */
	{
		UPokerGameModel* M = NewObject<UPokerGameModel>();
		M->Init(SeedA);
		int64 TotalPay = 0;
		int64 HandCounts[10] = {};
		const double Start = FPlatformTime::Seconds();
		for (int32 i = 0; i < SimHands; ++i)
		{
			const TArray<int32> Dealt = M->Deal();
			const int32 Mask = SimpleStrategy(Dealt);
			const FPokerHandResult Res = M->Draw(Mask);
			TotalPay += Res.PayMultiplier;
			++HandCounts[static_cast<int32>(Res.Rank)];
		}
		const double Secs = FPlatformTime::Seconds() - Start;
		const double Rtp = 100.0 * static_cast<double>(TotalPay) / SimHands;   // bet = 1 unit/hand

		UE_LOG(LogPokerSmoke, Display, TEXT("  ---------------- PAY REPORT (simple strategy) ----------------"));
		UE_LOG(LogPokerSmoke, Display, TEXT("  Hands             : %d"), SimHands);
		UE_LOG(LogPokerSmoke, Display, TEXT("  RTP               : %.3f %%"), Rtp);
		for (int32 Rank = 9; Rank >= 0; --Rank)
		{
			if (HandCounts[Rank] > 0)
			{
				UE_LOG(LogPokerSmoke, Display, TEXT("  %-18s: %lld  (1 in %.0f)"),
					UPokerGameModel::RankDisplayName(static_cast<EPokerHandRank>(Rank)),
					HandCounts[Rank], static_cast<double>(SimHands) / HandCounts[Rank]);
			}
		}
		UE_LOG(LogPokerSmoke, Display, TEXT("  Sim speed         : %.2fs for %d hands"), Secs, SimHands);
		UE_LOG(LogPokerSmoke, Display, TEXT("  --------------------------------------------------------------"));

		R.Check(Rtp >= 80.0 && Rtp <= 105.0, FString::Printf(TEXT("RTP %.2f%% within catastrophic band [80, 105]"), Rtp));
		R.Warn(Rtp >= 93.0 && Rtp <= 101.0, FString::Printf(TEXT("RTP %.2f%% within DESIGN band [93, 101] (tune the paytable if WARN)"), Rtp));
	}

	/* ---------- S2 hookup ---------- */
	R.Check(UPokerScreenWidget::StaticClass() != nullptr, TEXT("S2: UPokerScreenWidget class resolves"));

	if (R.Failures == 0)
	{
		UE_LOG(LogPokerSmoke, Display, TEXT("=== POKER SMOKE TEST PASSED (model math + screen class green). ==="));
		return 0;
	}
	UE_LOG(LogPokerSmoke, Error, TEXT("=== POKER SMOKE TEST FAILED: %d failure(s). ==="), R.Failures);
	return 1;
}
