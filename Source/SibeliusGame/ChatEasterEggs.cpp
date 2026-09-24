// ChatEasterEggs.cpp — the AI chat's Easter eggs. See header.

#include "ChatEasterEggs.h"

#include "ProgressionSubsystem.h"

namespace
{
	/** Lowercase letters and single spaces only: "I'm  Leonard!" becomes "im leonard". */
	FString Normalise(const FString& Line)
	{
		FString Out;
		Out.Reserve(Line.Len() + 2);
		Out.AppendChar(TEXT(' '));
		for (const TCHAR Ch : Line)
		{
			if (FChar::IsAlpha(Ch))
			{
				Out.AppendChar(FChar::ToLower(Ch));
			}
			else if (Ch == TEXT('\'') || Ch == TEXT('\x2019'))
			{
				continue;   // "i'm" and "i’m" both become "im"
			}
			else if (!Out.EndsWith(TEXT(" ")))
			{
				Out.AppendChar(TEXT(' '));
			}
		}
		if (!Out.EndsWith(TEXT(" ")))
		{
			Out.AppendChar(TEXT(' '));
		}
		return Out;   // padded with spaces, so " im leonard " cannot match inside "slim leonardo"
	}
}

namespace ChatEasterEggs
{
	bool MentionsPigeons(const FString& Line)
	{
		return Line.Contains(TEXT("pigeon"), ESearchCase::IgnoreCase);
	}

	bool NamesHimselfLeonard(const FString& Line)
	{
		static const TCHAR* Phrases[] =
		{
			TEXT(" my name is leonard "),
			TEXT(" my names leonard "),
			TEXT(" i am leonard "),
			TEXT(" im leonard "),
			TEXT(" call me leonard "),
		};
		const FString Said = Normalise(Line);
		for (const TCHAR* Phrase : Phrases)
		{
			if (Said.Contains(Phrase))
			{
				return true;
			}
		}
		return false;
	}

	FCheck EvaluateChatLine(const FProgressionState& State, FName ChatId, const FString& Line)
	{
		FCheck Check;
		// FName compares without case, so "Kaia" and "kaia" are the same agent.
		if (ChatId != FName(TEXT("kaia")) || !MentionsPigeons(Line) || State.HasClaimed(PigeonLedger))
		{
			return Check;
		}

		Check.bCountsTowardPigeons = true;
		if (State.GetStat(KaiaPigeonQuestionsStat) + 1 >= PigeonQuestionsNeeded)
		{
			Check.Egg = PigeonLedger;
			Check.StoryEvent = TEXT("egg_pigeon_ledger");
		}
		return Check;
	}

	FCheck EvaluateCouncilLine(const FProgressionState& State, const FString& Line)
	{
		FCheck Check;
		if (!State.HasClaimed(NotProgrammer) && NamesHimselfLeonard(Line))
		{
			Check.Egg = NotProgrammer;
			Check.StoryEvent = TEXT("egg_not_programmer");
		}
		return Check;
	}

	FCheck EvaluateChatLine(const UProgressionSubsystem* Progression, FName ChatId, const FString& Line)
	{
		return Progression ? EvaluateChatLine(Progression->GetStateForRead(), ChatId, Line) : FCheck();
	}

	FCheck EvaluateCouncilLine(const UProgressionSubsystem* Progression, const FString& Line)
	{
		return Progression ? EvaluateCouncilLine(Progression->GetStateForRead(), Line) : FCheck();
	}

	FString Commit(UProgressionSubsystem* Progression, const FCheck& Check)
	{
		if (!Progression)
		{
			return FString();
		}

		if (Check.bCountsTowardPigeons)
		{
			Progression->BumpStat(KaiaPigeonQuestionsStat);
		}

		// ClaimOneTimeGrant is the once-per-save guarantee: false means it was already found.
		if (Check.Egg.IsNone() || !Progression->ClaimOneTimeGrant(Check.Egg))
		{
			return FString();
		}

		if (Check.Egg == PigeonLedger)
		{
			Progression->GrantSauce(PigeonLedgerSauce);
			return FString::Printf(
				TEXT("You found an Easter egg: The Pigeon Ledger. +%d sauce. It is in your Journal [J]."),
				PigeonLedgerSauce);
		}
		if (Check.Egg == NotProgrammer)
		{
			Progression->GrantSauce(NotProgrammerSauce);
			return FString::Printf(TEXT("You found an Easter egg: Not Programmer. +%d sauce."), NotProgrammerSauce);
		}
		return FString();
	}
}
