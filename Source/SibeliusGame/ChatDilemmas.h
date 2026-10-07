// ChatDilemmas.h
//
// Each AI agent asks Leonard for advice, once (backend/dilemmas.js). Walt, 2026-10-06: "how
// about if the agents ask the player for personal advice? that might be entertaining" - "let's
// just do the feature for all agents and see what happens later".
//
// THE GAME ASKS. Her question is a fixed line, shown as her turn when a conversation opens, the
// first time they talk after they have already talked (TurnsBeforeAsking). It is about HER life,
// never his, so nothing private is ever asked.
//
// SHE ONLY RECORDS WHICH WAY HE ADVISED. While her question is open, every request to her
// carries its event; the server lets a reply carry the advice back only then, and only as one
// of her two answers. Unlike an Easter egg (ChatEasterEggs.h), the reply decides - which is safe
// because nothing is granted for it: the worst a player can talk her into is recording the
// advice he gave. After that the request's facts carry it, and she knows how it turned out.

#pragma once

#include "CoreMinimal.h"

class UProgressionSubsystem;
struct FProgressionState;

namespace ChatDilemmas
{
	struct FDilemma
	{
		FName ChatId;              // the agent, as the chat is keyed: "isla"
		const TCHAR* Id;           // backend/dilemmas.js's id: "isla_versions"
		const TCHAR* Question;     // her line - THE SAME WORDS as the server's (test.js checks)
		const TCHAR* ChoiceA;      // the two answers the server allowlists
		const TCHAR* ChoiceB;
		const TCHAR* NoteA;        // what the panel prints once he has advised
		const TCHAR* NoteB;
	};

	/** Her dilemma, or null for anyone without one. */
	SIBELIUSGAME_API const FDilemma* Find(FName ChatId);

	/** Turns already remembered with her before she asks: not in their very first talk. */
	constexpr int32 TurnsBeforeAsking = 4;

	/** Grant keys. Written into saves - never rename. "Dilemma_isla_versions_Asked", "..._keep". */
	SIBELIUSGAME_API FName AskedGrant(const FDilemma& Dilemma);
	SIBELIUSGAME_API FName AdvisedGrant(const FDilemma& Dilemma, const FString& Choice);

	/** The answer he gave her, or empty. */
	SIBELIUSGAME_API FString AdviceGiven(const FProgressionState& State, const FDilemma& Dilemma);

	/** Put her question as this conversation opens? Not yet asked, not answered, and they have talked before. */
	SIBELIUSGAME_API bool ShouldAsk(const FProgressionState& State, FName ChatId, int32 RememberedTurns);

	/** Asked and not yet answered: her requests carry the event. */
	SIBELIUSGAME_API bool IsOpen(const FProgressionState& State, FName ChatId);

	/** The backend's allowlisted event: "dilemma_" + Id. */
	SIBELIUSGAME_API FString StoryEvent(const FDilemma& Dilemma);

	/** Every answer he has given, as "isla_versions:keep" - the request's facts.advice. */
	SIBELIUSGAME_API TArray<FString> AdviceFacts(const FProgressionState& State);

	/** One of her two answers, lower-cased, or empty. */
	SIBELIUSGAME_API FString ValidChoice(const FDilemma& Dilemma, const FString& Choice);

	/** For the mock service: the answer a line names as a whole word ("keep them"), or empty. */
	SIBELIUSGAME_API FString ChoiceNamedIn(const FDilemma& Dilemma, const FString& Line);

	/** Note that she asked (saves). */
	SIBELIUSGAME_API void MarkAsked(UProgressionSubsystem* Progression, const FDilemma& Dilemma);

	/**
	 * Record his advice, once, if her question is open and it is one of her answers (saves).
	 * Returns the line the panel prints, or empty when nothing was recorded.
	 */
	SIBELIUSGAME_API FString CommitAdvice(UProgressionSubsystem* Progression, FName ChatId, const FString& Choice);
}
