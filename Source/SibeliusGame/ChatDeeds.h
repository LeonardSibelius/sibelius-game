// ChatDeeds.h
//
// What the agents remember him doing, and noticing when he comes back (backend/memory.js).
// Walt, 2026-10-19, on what a player of this game wishes for - not to be forgotten, to be
// known, to have his work count: "go ahead with 1 and 2". 1: a welcome back. 2: a record of his
// deeds that outlasts the conversation window (UNpcChatSubsystem::MaxTurnsKept is eight
// exchanges; a win from last month is long gone from it).
//
// DEEDS, NEVER CONFIDENCES. Everything here is something the game itself saw him do - a win, a
// record, a universe kept. Nothing he typed is kept or summarised for it: on 2026-10-06 Walt
// ruled that the agents never ask about the player's own life, and called the alternative
// "the companion-app trap". The world remembering what he did is the safe way to be known.
//
// THE GAME DECIDES WHAT IS TRUE. Each deed is an allowlisted id, with a whole number where it
// has one ("chess_wins:3"); the server allowlists it again and writes the sentence itself
// (backend/memory.js). The model never decides a deed happened.
//
// WHAT IS NEW is the game's call too: each agent's saved conversation keeps the deeds she knew
// of when she last answered him (FNpcChatHistory::DeedsKnown), and a deed whose string is not
// among them - a new one, or a count or record that has grown - is news to her.

#pragma once

#include "CoreMinimal.h"

struct FProgressionState;

namespace ChatDeeds
{
	/**
	 * Every deed he has done, as the request's facts.deeds: "battle_won", "universes_kept:2".
	 * Counts and records of zero are left out - a deed is something that happened. In a fixed
	 * order, so the same save always sends the same list.
	 */
	SIBELIUSGAME_API TArray<FString> FromState(const FProgressionState& State);

	/** The deeds in Now that Known does not hold: what she has not heard of yet. */
	SIBELIUSGAME_API TArray<FString> NewSince(const TArray<FString>& Now, const TArray<FString>& Known);

	/**
	 * Whole hours between two moments, for facts.hoursAway: never negative (a clock set back
	 * reads as "just now"), and capped so a corrupt date cannot send a silly number.
	 */
	SIBELIUSGAME_API int32 HoursBetween(const FDateTime& Then, const FDateTime& Now);

	/** The cap on HoursBetween: twenty years. backend/memory.js refuses anything above it. */
	constexpr int32 MaxHoursAway = 24 * 365 * 20;
}
