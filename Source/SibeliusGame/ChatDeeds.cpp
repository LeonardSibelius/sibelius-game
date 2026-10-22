// ChatDeeds.cpp — the deeds the agents may remember. See the header, and backend/memory.js for
// the sentences the server writes from them. THE IDS ARE THE SERVER'S: a new one here needs a
// line in memory.js's DEEDS, or the server drops it.

#include "ChatDeeds.h"

#include "ProgressionTypes.h"
#include "ChatEasterEggs.h"   // NotProgrammer: he told the council his name
#include "BattleArrival.h"    // BattleWonGrant: the meadow

namespace ChatDeeds
{
	TArray<FString> FromState(const FProgressionState& State)
	{
		TArray<FString> Deeds;
		const auto Flag = [&Deeds](bool bDone, const TCHAR* Id)
		{
			if (bDone)
			{
				Deeds.Add(Id);
			}
		};
		const auto Count = [&Deeds](int32 N, const TCHAR* Id)
		{
			if (N > 0)
			{
				Deeds.Add(FString::Printf(TEXT("%s:%d"), Id, N));
			}
		};

		// The story's own milestones.
		Flag(State.HasClaimed(ABattleArrival::BattleWonGrant), TEXT("battle_won"));
		Flag(State.HasClaimed(ChatEasterEggs::NotProgrammer), TEXT("named_himself"));
		// No "city_answered" or "universes_kept" since 2026-10-22: Walt dropped the universe game,
		// so the agents are not told of a gallery that is no longer there. The server still knows
		// both ids, for builds before it.

		// The side games (SibeliusStats, kept since 2026-10-19).
		Count(State.GetStat(SibeliusStats::GrandPrixRaces), TEXT("grand_prix_races"));
		Count(State.GetStat(SibeliusStats::GrandPrixWins), TEXT("grand_prix_wins"));
		Count(State.GetStat(SibeliusStats::ThreePointContests), TEXT("three_point_contests"));
		Count(State.GetStat(SibeliusStats::ThreePointWins), TEXT("three_point_wins"));
		Count(State.GetStat(SibeliusStats::ChessWins), TEXT("chess_wins"));
		Count(State.GetStat(SibeliusStats::GoWins), TEXT("go_wins"));
		Count(State.GetStat(SibeliusStats::PinballBest), TEXT("pinball_best"));
		Count(State.GetStat(SibeliusStats::BowlingBest), TEXT("bowling_best"));
		Count(State.GetStat(SibeliusStats::PoolTablesCleared), TEXT("pool_clears"));
		return Deeds;
	}

	TArray<FString> NewSince(const TArray<FString>& Now, const TArray<FString>& Known)
	{
		TArray<FString> Fresh;
		for (const FString& Deed : Now)
		{
			if (!Known.Contains(Deed))
			{
				Fresh.Add(Deed);
			}
		}
		return Fresh;
	}

	int32 HoursBetween(const FDateTime& Then, const FDateTime& Now)
	{
		const double Hours = (Now - Then).GetTotalHours();
		return Hours <= 0.0 ? 0 : static_cast<int32>(FMath::Min(Hours, static_cast<double>(MaxHoursAway)));
	}
}
