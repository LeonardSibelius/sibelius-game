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

	/** True when any of the words appears whole in the line. */
	static bool HasWord(const FString& Line, std::initializer_list<const TCHAR*> Words)
	{
		const FString Said = Normalise(Line);
		for (const TCHAR* Word : Words)
		{
			if (Said.Contains(FString::Printf(TEXT(" %s "), Word)))
			{
				return true;
			}
		}
		return false;
	}

	bool SaysFold(const FString& Line)
	{
		return HasWord(Line, { TEXT("fold"), TEXT("folds"), TEXT("folded"), TEXT("folding") });
	}

	bool SaysPoetry(const FString& Line)
	{
		return HasWord(Line, { TEXT("poetry"), TEXT("poem"), TEXT("poems") });
	}

	FString RequestedItem(const FString& Line)
	{
		// The ways people ask a shopkeeper for something, apostrophes already gone ("id like").
		static const TCHAR* Asks[] =
		{
			TEXT(" do you have "), TEXT(" do you sell "), TEXT(" do you stock "), TEXT(" do you carry "),
			TEXT(" do you keep "), TEXT(" have you got "), TEXT(" got any "), TEXT(" can i get "),
			TEXT(" can i have "), TEXT(" could i get "), TEXT(" could i have "), TEXT(" may i have "),
			TEXT(" i want "), TEXT(" i would like "), TEXT(" id like "), TEXT(" i need "),
			TEXT(" looking for "), TEXT(" where is "), TEXT(" where are "), TEXT(" wheres "),
			TEXT(" give me "), TEXT(" sell me "), TEXT(" ill take "),
			/* INSISTING COUNTS TOO (Walt, 2026-09-23): "are you sure you don't have bananas?",
			   "I know you have bananas". Pushing a shopkeeper is exactly how a player chases
			   this egg. " you have " also covers "don't you have" and "are you sure you have". */
			TEXT(" you have "), TEXT(" you dont have "), TEXT(" you do not have "),
		};
		// Words between the ask and the thing: "do you have ANY bananas", "can i get A banana".
		static const TCHAR* Filler[] =
		{
			TEXT("a"), TEXT("an"), TEXT("any"), TEXT("some"), TEXT("the"), TEXT("more"), TEXT("of"),
			TEXT("your"), TEXT("me"), TEXT("us"), TEXT("to"), TEXT("buy"), TEXT("get"), TEXT("please"),
			TEXT("just"), TEXT("one"), TEXT("few"), TEXT("in"), TEXT("stock"), TEXT("i"), TEXT("my"),
		};

		const FString Said = Normalise(Line);
		int32 Earliest = INDEX_NONE;
		int32 AskLen = 0;
		for (const TCHAR* Ask : Asks)
		{
			const int32 At = Said.Find(Ask);
			if (At != INDEX_NONE && (Earliest == INDEX_NONE || At < Earliest))
			{
				Earliest = At;
				AskLen = FCString::Strlen(Ask);
			}
		}
		if (Earliest == INDEX_NONE)
		{
			return FString();
		}

		TArray<FString> Words;
		Said.Mid(Earliest + AskLen).ParseIntoArray(Words, TEXT(" "));
		for (const FString& Word : Words)
		{
			bool bFiller = false;
			for (const TCHAR* F : Filler)
			{
				if (Word == F)
				{
					bFiller = true;
					break;
				}
			}
			if (bFiller)
			{
				continue;
			}
			// "bananas" and "banana" are the same ask. Crude, but applied the same way to
			// both sides of every comparison, which is all it has to be.
			return (Word.Len() > 3 && Word.EndsWith(TEXT("s"))) ? Word.LeftChop(1) : Word;
		}
		return FString();
	}

	FCheck EvaluateChatLine(const FProgressionState& State, FName ChatId, const FString& Line,
		const TArray<FString>& EarlierLines)
	{
		FCheck Check;

		/* EGG 2, FINALLY IN STOCK. Aisling's line is "we don't keep that here"; ask her for the
		   same thing five times and, for once, she finds it. Counted in the conversation
		   memory, so it survives closing the panel and needs no new save field. */
		if (ChatId == FName(TEXT("aisling")))
		{
			const FString Item = RequestedItem(Line);
			if (Item.IsEmpty() || State.HasClaimed(FinallyInStock))
			{
				return Check;
			}
			int32 Asks = 1;
			for (const FString& Earlier : EarlierLines)
			{
				Asks += RequestedItem(Earlier) == Item ? 1 : 0;
			}
			if (Asks >= SameAsksNeeded)
			{
				Check.Egg = FinallyInStock;
				Check.StoryEvent = TEXT("egg_finally_in_stock");
			}
			return Check;
		}

		/* EGG 3, FOLD EARLY. Isla's plan to win at poker is to fold before the cards are
		   dealt; the player who takes her at her word is the most efficient she has seen. */
		if (ChatId == FName(TEXT("isla")))
		{
			if (SaysFold(Line) && !State.HasClaimed(FoldEarly))
			{
				Check.Egg = FoldEarly;
				Check.StoryEvent = TEXT("egg_fold_early");
			}
			return Check;
		}

		/* EGG 4, THE MEMO POETRY READING. Nyra's scheme is to sell Mrs. Hall's memos as
		   poetry; say the word and she has already set three of them. */
		if (ChatId == FName(TEXT("nyra")))
		{
			if (SaysPoetry(Line) && !State.HasClaimed(MemoPoetry))
			{
				Check.Egg = MemoPoetry;
				Check.StoryEvent = TEXT("egg_memo_poetry");
			}
			return Check;
		}

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

	FCheck EvaluateChatLine(const UProgressionSubsystem* Progression, FName ChatId, const FString& Line,
		const TArray<FString>& EarlierLines)
	{
		return Progression ? EvaluateChatLine(Progression->GetStateForRead(), ChatId, Line, EarlierLines) : FCheck();
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
		if (Check.Egg == FoldEarly)
		{
			return TEXT("You found an Easter egg: Fold Early.");
		}
		if (Check.Egg == MemoPoetry)
		{
			return TEXT("You found an Easter egg: The Memo Poetry Reading. It is in your Journal [J].");
		}
		if (Check.Egg == FinallyInStock)
		{
			// No sauce: the reward is Aisling finding it, and the line on RECORDS.
			return TEXT("You found an Easter egg: Finally in Stock.");
		}
		if (Check.Egg == NotProgrammer)
		{
			Progression->GrantSauce(NotProgrammerSauce);
			return FString::Printf(TEXT("You found an Easter egg: Not Programmer. +%d sauce."), NotProgrammerSauce);
		}
		return FString();
	}
}
