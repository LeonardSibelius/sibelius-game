// ChatEasterEggs.h
//
// Easter eggs in the AI agent chat (docs/THE_UNWRITTEN.md, "Easter eggs"). Two are built:
//   1. The Pigeon Ledger — ask Kaia about pigeons three times.
//   5. Not Programmer    — tell the council your name is Leonard.
//
// THE PLAYER'S WORDS TRIGGER AN EGG, NEVER THE AI'S. A player can talk the model into saying
// anything, so nothing here reads a reply. The game checks what the player typed, grants the
// reward itself, and sends the backend an allowlisted event (backend/events.js) so the agents
// can react in character. The agents' running jokes are the clues; the triggers live here.
//
// EVALUATE, THEN COMMIT. Evaluate has no side effects, so the panel can put the event on the
// request before it is sent; Commit runs only once the send was accepted. A send refused as
// busy or unavailable counts for nothing — the player may simply type it again.

#pragma once

#include "CoreMinimal.h"

class UProgressionSubsystem;
struct FProgressionState;

namespace ChatEasterEggs
{
	/** One-time grant keys. Written into saves — never rename. */
	inline const FName PigeonLedger(TEXT("Egg_PigeonLedger"));
	inline const FName NotProgrammer(TEXT("Egg_NotProgrammer"));

	/** Messages to Kaia that mentioned pigeons, toward egg 1. A lifetime stat, so per save. */
	inline const FName KaiaPigeonQuestionsStat(TEXT("KaiaPigeonQuestions"));

	constexpr int32 PigeonQuestionsNeeded = 3;
	constexpr int32 PigeonLedgerSauce = 5;
	constexpr int32 NotProgrammerSauce = 10;

	/** "pigeon", "Pigeons", "the PIGEONS?" — loose, so nobody has to guess a spelling. */
	SIBELIUSGAME_API bool MentionsPigeons(const FString& Line);

	/** "my name is Leonard", "I'm Leonard", "call me Leonard" — punctuation and case ignored. */
	SIBELIUSGAME_API bool NamesHimselfLeonard(const FString& Line);

	/** What one player line would do. No side effects. */
	struct FCheck
	{
		bool bCountsTowardPigeons = false;   // bump the stat on commit
		FName Egg;                           // NAME_None unless this line finds an egg
		FString StoryEvent;                  // the backend's allowlisted event, or empty
	};

	/**
	 * A line to one agent. ChatId is the stable id the chat is keyed by ("kaia").
	 * Reads the plain state, not the subsystem, so the gate can test it on a throwaway state
	 * without ever touching a player's save (the progression slot has no sandbox).
	 */
	SIBELIUSGAME_API FCheck EvaluateChatLine(const FProgressionState& State, FName ChatId, const FString& Line);

	/** A line to the council. */
	SIBELIUSGAME_API FCheck EvaluateCouncilLine(const FProgressionState& State, const FString& Line);

	/** The same, from the live game: no subsystem (headless, teardown) means no egg. */
	SIBELIUSGAME_API FCheck EvaluateChatLine(const UProgressionSubsystem* Progression, FName ChatId, const FString& Line);
	SIBELIUSGAME_API FCheck EvaluateCouncilLine(const UProgressionSubsystem* Progression, const FString& Line);

	/**
	 * Apply a check once the send was accepted: bump the count, claim the egg once per save,
	 * grant its sauce. Returns the line the panel prints, or empty when no egg was found.
	 */
	SIBELIUSGAME_API FString Commit(UProgressionSubsystem* Progression, const FCheck& Check);
}
