// ChatEasterEggs.h
//
// Easter eggs in the AI agent chat (docs/THE_UNWRITTEN.md, "Easter eggs"). Built:
//   1. The Pigeon Ledger  — ask Kaia about pigeons three times.
//   2. Finally in Stock   — ask Aisling for the same thing five times.
//   3. Fold Early         — tell Isla "fold".
//   4. The Memo Poetry Reading — say "poetry" to Nyra.
//   5. Not Programmer     — tell the council your name is Leonard.
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
	inline const FName FinallyInStock(TEXT("Egg_FinallyInStock"));
	inline const FName FoldEarly(TEXT("Egg_FoldEarly"));
	inline const FName MemoPoetry(TEXT("Egg_MemoPoetry"));

	/** Asks for the same item, this one included, before Aisling finds it (egg 2). */
	constexpr int32 SameAsksNeeded = 5;

	/** Messages to Kaia that mentioned pigeons, toward egg 1. A lifetime stat, so per save. */
	inline const FName KaiaPigeonQuestionsStat(TEXT("KaiaPigeonQuestions"));

	constexpr int32 PigeonQuestionsNeeded = 3;
	constexpr int32 PigeonLedgerSauce = 5;
	constexpr int32 NotProgrammerSauce = 10;

	/** "pigeon", "Pigeons", "the PIGEONS?" — loose, so nobody has to guess a spelling. */
	SIBELIUSGAME_API bool MentionsPigeons(const FString& Line);

	/** "my name is Leonard", "I'm Leonard", "call me Leonard" — punctuation and case ignored. */
	SIBELIUSGAME_API bool NamesHimselfLeonard(const FString& Line);

	/** "fold", "I fold", "folding" — the whole word, so "folder" and "unfold" do not count. */
	SIBELIUSGAME_API bool SaysFold(const FString& Line);

	/** "poetry", "a poem", "poems" — the whole word. */
	SIBELIUSGAME_API bool SaysPoetry(const FString& Line);

	/**
	 * The thing a line asks for, reduced to one comparable word, or empty when it asks for
	 * nothing. "do you have any bananas?", "can I get a banana" and "where are the bananas"
	 * all give "banana". Deliberately rough: it only has to tell one item from another.
	 */
	SIBELIUSGAME_API FString RequestedItem(const FString& Line);

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
	 *
	 * EarlierLines are the player's earlier lines to this agent, from the conversation memory
	 * (UNpcChatSubsystem::GetHistory, the player's turns only). Egg 2 counts in them, so "five
	 * times" needs no new save field: it is five of the last eight things he said to her.
	 */
	SIBELIUSGAME_API FCheck EvaluateChatLine(const FProgressionState& State, FName ChatId, const FString& Line,
		const TArray<FString>& EarlierLines = TArray<FString>());

	/** A line to the council. */
	SIBELIUSGAME_API FCheck EvaluateCouncilLine(const FProgressionState& State, const FString& Line);

	/** The same, from the live game: no subsystem (headless, teardown) means no egg. */
	SIBELIUSGAME_API FCheck EvaluateChatLine(const UProgressionSubsystem* Progression, FName ChatId, const FString& Line,
		const TArray<FString>& EarlierLines = TArray<FString>());
	SIBELIUSGAME_API FCheck EvaluateCouncilLine(const UProgressionSubsystem* Progression, const FString& Line);

	/**
	 * Apply a check once the send was accepted: bump the count, claim the egg once per save,
	 * grant its sauce. Returns the line the panel prints, or empty when no egg was found.
	 */
	SIBELIUSGAME_API FString Commit(UProgressionSubsystem* Progression, const FCheck& Check);
}
