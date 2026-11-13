// ChallengeTypes.cpp - see header.

#include "ChallengeTypes.h"
#include "ProgressionTypes.h"

namespace
{
	using enum EChallengeTest;

	// Where each game is, in the words a player needs. The agents' homes are behind Compile's
	// portals; the house records are all in the living room from the first minute.
	const TCHAR* const KaiaHome    = TEXT("Kaia's jungle: the portal at the end of the start hall [C]");
	const TCHAR* const EliseHome   = TEXT("Elise's grassland: the portal at her bedroom window [C]");
	const TCHAR* const AislingHome = TEXT("Aisling's snowfield: the portal behind her in the AI Temple [C]");
	const TCHAR* const IslaHome    = TEXT("Isla's keep: up through the attic");
	const TCHAR* const NyraCourt   = TEXT("The chess park: the BASKETBALL door in the living room");
	const TCHAR* const KartGarage  = TEXT("Your kart, in the living room");
	const TCHAR* const ChessDoor   = TEXT("The CHESS door in the living room");
	const TCHAR* const GoDoor      = TEXT("The GO door in the living room");
	const TCHAR* const LivingRoom  = TEXT("In the living room");
	const TCHAR* const BowlingDoor = TEXT("The BOWLING door in the living room");

	TArray<FChallengeTier> Build()
	{
		namespace S = SibeliusStats;
		return {
			// --- The agents, each at the game in her home -------------------------------------
			{ TEXT("Kaia.Bronze"),    TEXT("Kaia"),    TEXT("Lantern Burst"), 1, AtLeast, S::LanternStagesCleared, 1,  TEXT("Clear a stage"),                          KaiaHome },
			{ TEXT("Kaia.Silver"),    TEXT("Kaia"),    TEXT("Lantern Burst"), 2, AtLeast, S::LanternStagesCleared, 3,  TEXT("Clear stage 3"),                          KaiaHome },
			{ TEXT("Kaia.Gold"),      TEXT("Kaia"),    TEXT("Lantern Burst"), 3, AtLeast, S::LanternStagesCleared, 5,  TEXT("Clear stage 5"),                          KaiaHome },

			{ TEXT("Elise.Bronze"),   TEXT("Elise"),   TEXT("Orbital golf"),  1, AtLeast, S::OrbitGolfRounds,      1,  TEXT("Finish a round of three holes"),          EliseHome },
			{ TEXT("Elise.Silver"),   TEXT("Elise"),   TEXT("Orbital golf"),  2, AtMost,  S::OrbitGolfBest,        11, TEXT("A round in 11 shots or fewer"),           EliseHome },
			{ TEXT("Elise.Gold"),     TEXT("Elise"),   TEXT("Orbital golf"),  3, AtMost,  S::OrbitGolfBest,        8,  TEXT("A round at par (8) or better"),           EliseHome },

			{ TEXT("Aisling.Bronze"), TEXT("Aisling"), TEXT("4D billiards"),  1, AtLeast, S::HyperTablesCleared,   1,  TEXT("Clear the table"),                        AislingHome },
			{ TEXT("Aisling.Silver"), TEXT("Aisling"), TEXT("4D billiards"),  2, AtMost,  S::HyperBilliardsBest,   20, TEXT("Clear it in 20 strokes or fewer"),        AislingHome },
			{ TEXT("Aisling.Gold"),   TEXT("Aisling"), TEXT("4D billiards"),  3, AtMost,  S::HyperBilliardsBest,   14, TEXT("Clear it in 14 strokes or fewer"),        AislingHome },

			{ TEXT("Isla.Bronze"),    TEXT("Isla"),    TEXT("The Dragon's Wager"), 1, AtLeast, S::DragonsWagerBest, 1,   TEXT("Bank any treasure"),                  IslaHome },
			{ TEXT("Isla.Silver"),    TEXT("Isla"),    TEXT("The Dragon's Wager"), 2, AtLeast, S::DragonsWagerBest, 300, TEXT("Bank 300 treasure in one wager"),     IslaHome },
			{ TEXT("Isla.Gold"),      TEXT("Isla"),    TEXT("The Dragon's Wager"), 3, AtLeast, S::DragonsWagerBest, 600, TEXT("Bank 600 treasure in one wager"),     IslaHome },

			{ TEXT("Nyra.Bronze"),    TEXT("Nyra"),    TEXT("The three-point contest"), 1, AtLeast, S::ThreePointWins, 1,  TEXT("Beat Nyra"),                        NyraCourt },
			{ TEXT("Nyra.Silver"),    TEXT("Nyra"),    TEXT("The three-point contest"), 2, AtLeast, S::ThreePointBest, 12, TEXT("Score 12 or more"),                 NyraCourt },
			{ TEXT("Nyra.Gold"),      TEXT("Nyra"),    TEXT("The three-point contest"), 3, AtLeast, S::ThreePointBest, 18, TEXT("Score 18 or more"),                 NyraCourt },

			{ TEXT("GrandPrix.Bronze"), TEXT("Grand Prix"), TEXT("All five agents, three laps"), 1, AtLeast, S::GrandPrixRaces,      1, TEXT("Finish a race"),                      KartGarage },
			{ TEXT("GrandPrix.Silver"), TEXT("Grand Prix"), TEXT("All five agents, three laps"), 2, AtLeast, S::GrandPrixWins,       1, TEXT("Win a race"),                         KartGarage },
			{ TEXT("GrandPrix.Gold"),   TEXT("Grand Prix"), TEXT("All five agents, three laps"), 3, AtLeast, S::GrandPrixFierceWins, 1, TEXT("Win with the agents set to Fierce"),  KartGarage },

			// --- House records: the games Leonard grew up with ----------------------------------
			{ TEXT("House.Chess1"),   TEXT("House"),   TEXT("Chess"),         1, AtLeast, S::ChessBestLevel,  1, TEXT("Beat the Beginner ghost"),   ChessDoor },
			{ TEXT("House.Chess2"),   TEXT("House"),   TEXT("Chess"),         2, AtLeast, S::ChessBestLevel,  2, TEXT("Beat the Hustler ghost"),    ChessDoor },
			{ TEXT("House.Chess3"),   TEXT("House"),   TEXT("Chess"),         3, AtLeast, S::ChessBestLevel,  3, TEXT("Beat the Master ghost"),     ChessDoor },
			{ TEXT("House.Go1"),      TEXT("House"),   TEXT("Go"),            1, AtLeast, S::GoBestLevel,     1, TEXT("Beat the Beginner ghost"),   GoDoor },
			{ TEXT("House.Go2"),      TEXT("House"),   TEXT("Go"),            2, AtLeast, S::GoBestLevel,     2, TEXT("Beat the Hustler ghost"),    GoDoor },
			{ TEXT("House.Go3"),      TEXT("House"),   TEXT("Go"),            3, AtLeast, S::GoBestLevel,     3, TEXT("Beat the Master ghost"),     GoDoor },
			{ TEXT("House.Pinball"),  TEXT("House"),   TEXT("Pinball"),       0, AtLeast, S::PinballBest,  2000, TEXT("Score 2,000 in one game"),   LivingRoom },
			{ TEXT("House.Bowling"),  TEXT("House"),   TEXT("Bowling"),       0, AtLeast, S::BowlingBest,   150, TEXT("Bowl a 150 game"),           BowlingDoor },
			{ TEXT("House.Pool"),     TEXT("House"),   TEXT("Pool"),          0, AtMost,  S::PoolBestStrokes, 25, TEXT("Clear the table in 25 strokes or fewer"), LivingRoom },
			{ TEXT("House.Poker"),    TEXT("House"),   TEXT("Video poker"),   0, AtLeast, S::PokerBestHand,   7, TEXT("Hold four of a kind"),       LivingRoom },
		};
	}
}

namespace SibeliusChallenges
{
	const TArray<FChallengeTier>& All()
	{
		static const TArray<FChallengeTier> Table = Build();
		return Table;
	}

	bool IsMet(const FProgressionState& State, const FChallengeTier& Tier)
	{
		const int32 Value = State.GetStat(Tier.Stat);
		return Tier.Test == EChallengeTest::AtMost
			? (Value > 0 && Value <= Tier.Target)
			: Value >= Tier.Target;
	}

	FName ClaimKey(const FChallengeTier& Tier)
	{
		return FName(*(FString(TEXT("Challenge.")) + Tier.Id.ToString()));
	}

	int32 SauceFor(const FChallengeTier& Tier)
	{
		switch (Tier.Tier)
		{
		case 1:  return 50;
		case 2:  return 100;
		case 3:  return 150;
		default: return 100;
		}
	}

	const TCHAR* TierName(int32 Tier)
	{
		switch (Tier)
		{
		case 1:  return TEXT("BRONZE");
		case 2:  return TEXT("SILVER");
		case 3:  return TEXT("GOLD");
		default: return TEXT("RECORD");
		}
	}

	FString BestText(const FProgressionState& State, const FChallengeTier& Tier)
	{
		const int32 Value = State.GetStat(Tier.Stat);
		if (Value <= 0)
		{
			return FString();
		}
		// The chess and Go levels, and the poker hand, are not numbers a player thinks in.
		if (Tier.Stat == SibeliusStats::ChessBestLevel || Tier.Stat == SibeliusStats::GoBestLevel)
		{
			static const TCHAR* const Names[] = { TEXT(""), TEXT("Beginner"), TEXT("Hustler"), TEXT("Master") };
			return FString::Printf(TEXT("beaten so far: %s"), Names[FMath::Clamp(Value, 0, 3)]);
		}
		if (Tier.Stat == SibeliusStats::PokerBestHand)
		{
			return FString();
		}
		return FString::Printf(TEXT("your best: %d"), Value);
	}

	FString BannerText(const FChallengeTier& Tier)
	{
		const FString Head = FCString::Strcmp(Tier.Who, TEXT("House")) == 0
			? FString::Printf(TEXT("HOUSE RECORD - %s"), *FString(Tier.Game).ToUpper())
			: FCString::Strcmp(Tier.Who, TEXT("Grand Prix")) == 0
				? FString::Printf(TEXT("GRAND PRIX - %s"), TierName(Tier.Tier))
				: FString::Printf(TEXT("%s'S CHALLENGE - %s"), *FString(Tier.Who).ToUpper(), TierName(Tier.Tier));
		return FString::Printf(TEXT("%s   +%d SAUCE"), *Head, SauceFor(Tier));
	}

	bool SelfTest(FString& OutError)
	{
		TSet<FName> Seen;
		for (const FChallengeTier& Tier : All())
		{
			if (Tier.Id.IsNone() || Tier.Stat.IsNone() || Tier.Target <= 0 || !Tier.Who || !Tier.Game || !Tier.Goal || !Tier.Where)
			{
				OutError = FString::Printf(TEXT("challenge %s is incomplete"), *Tier.Id.ToString());
				return false;
			}
			if (Seen.Contains(Tier.Id))
			{
				OutError = FString::Printf(TEXT("challenge id %s appears twice"), *Tier.Id.ToString());
				return false;
			}
			Seen.Add(Tier.Id);
		}

		// The two tests, against a fresh state and one with records in it.
		FProgressionState Fresh;
		const FChallengeTier* Silver = All().FindByPredicate([](const FChallengeTier& T) { return T.Id == FName(TEXT("Elise.Silver")); });
		const FChallengeTier* NyraBronze = All().FindByPredicate([](const FChallengeTier& T) { return T.Id == FName(TEXT("Nyra.Bronze")); });
		if (!Silver || !NyraBronze)
		{
			OutError = TEXT("the test tiers Elise.Silver / Nyra.Bronze are missing");
			return false;
		}
		if (IsMet(Fresh, *Silver) || IsMet(Fresh, *NyraBronze))
		{
			OutError = TEXT("a fresh save meets a challenge (a 0 'fewest' record must never count)");
			return false;
		}
		FProgressionState Played;
		Played.LowerStat(SibeliusStats::OrbitGolfBest, 13);
		if (IsMet(Played, *Silver))
		{
			OutError = TEXT("13 strokes met a 'fewer than 11' tier");
			return false;
		}
		Played.LowerStat(SibeliusStats::OrbitGolfBest, 10);
		Played.LowerStat(SibeliusStats::OrbitGolfBest, 12);   // a worse round must not replace the best
		if (!IsMet(Played, *Silver) || Played.GetStat(SibeliusStats::OrbitGolfBest) != 10)
		{
			OutError = TEXT("LowerStat did not keep the fewest (10)");
			return false;
		}
		Played.BumpStat(SibeliusStats::ThreePointWins);
		if (!IsMet(Played, *NyraBronze))
		{
			OutError = TEXT("a win did not meet Nyra's Bronze");
			return false;
		}
		return true;
	}
}
