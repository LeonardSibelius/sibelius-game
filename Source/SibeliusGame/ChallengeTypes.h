// ChallengeTypes.h
//
// THE AGENTS' CHALLENGES (docs/AGENT_CHALLENGES_PLAN.md, ratified 2026-11-13: Walt, "Approve as
// written"). Each of the five agents challenges Leonard at the game in her home - Bronze, Silver,
// Gold - and the living-room games Leonard grew up with keep house records. Every tier met pays
// sauce once and (phase 2) unlocks a Steam achievement.
//
// PURE DATA AND ONE TEST. A tier is "this lifetime stat, at least / at most this number", read
// from FProgressionState::LifetimeStats, so meeting a tier needs no state of its own beyond the
// one-time grant that says it has been paid ("Challenge.<Id>"). The games only ever record their
// results (BumpStat / RaiseStat / LowerStat); UProgressionSubsystem::EvaluateChallenges notices.
//
// THE TARGETS ARE A FIRST GUESS (plan §6). They are meant to be set from Walt's own play - Bronze
// a newcomer's first good game, Gold his best - and they may move in a patch. A tier already paid
// stays paid when its target moves.

#pragma once

#include "CoreMinimal.h"

struct FProgressionState;

enum class EChallengeTest : uint8
{
	AtLeast,   // the stat is a count or a best score
	AtMost,    // the stat is a "fewest" record (LowerStat); 0 means not done yet, never met
};

struct FChallengeTier
{
	FName Id;              // "Kaia.Silver" - stable: it is in every save as Challenge.<Id>
	const TCHAR* Who;      // "Kaia" ... "Grand Prix", or "House" for the living-room records
	const TCHAR* Game;     // "Lantern Burst"
	int32 Tier;            // 1 Bronze, 2 Silver, 3 Gold; 0 a single house record
	EChallengeTest Test;
	FName Stat;            // a SibeliusStats key
	int32 Target;
	const TCHAR* Goal;     // what to do, in the menu's words
	const TCHAR* Where;    // how to get there
};

namespace SibeliusChallenges
{
	/** Every challenge, agents first, in the order the CHALLENGES tab shows them. */
	SIBELIUSGAME_API const TArray<FChallengeTier>& All();

	/** True when the save's records meet this tier. */
	SIBELIUSGAME_API bool IsMet(const FProgressionState& State, const FChallengeTier& Tier);

	/** The one-time grant that says this tier has been paid. */
	SIBELIUSGAME_API FName ClaimKey(const FChallengeTier& Tier);

	/** Bronze 50, Silver 100, Gold 150, a house record 100. */
	SIBELIUSGAME_API int32 SauceFor(const FChallengeTier& Tier);

	/** "BRONZE" / "SILVER" / "GOLD" / "RECORD". */
	SIBELIUSGAME_API const TCHAR* TierName(int32 Tier);

	/** "your best: 11" or "" when there is nothing to show yet. */
	SIBELIUSGAME_API FString BestText(const FProgressionState& State, const FChallengeTier& Tier);

	/** The banner when a tier falls: "KAIA'S CHALLENGE - SILVER   +100 SAUCE". */
	SIBELIUSGAME_API FString BannerText(const FChallengeTier& Tier);

	/** Self-check for the gate: ids unique, every tier has a stat and a target. */
	SIBELIUSGAME_API bool SelfTest(FString& OutError);
}
