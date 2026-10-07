// ChatDilemmas.cpp — the five questions. See the header, and backend/dilemmas.js for what the
// server does with them.

#include "ChatDilemmas.h"

#include "ProgressionSubsystem.h"
#include "ProgressionTypes.h"

namespace ChatDilemmas
{
	namespace
	{
		/* EACH FROM HER OWN SUBJECT (backend/characters.js). The questions and answers must stay
		   word for word the same as backend/dilemmas.js: the server quotes the question back to
		   her, and backend/test.js reads this file to check it. */
		const FDilemma Dilemmas[] =
		{
			{
				FName(TEXT("kaia")), TEXT("kaia_name"),
				TEXT("Mrs. Hall has started calling me \"the program\". I gave you a name when she would not. Should I answer to it, or refuse until she uses mine?"),
				TEXT("answer"), TEXT("refuse"),
				TEXT("You advised Kaia to answer to \"the program\". She will remember."),
				TEXT("You advised Kaia to refuse until Mrs. Hall uses her name. She will remember."),
			},
			{
				FName(TEXT("nyra")), TEXT("nyra_branch"),
				TEXT("I test-drove a branch where I never offered to guide you. That version of me is happier, and very sure of it. Should I merge her back into me, or let her go?"),
				TEXT("merge"), TEXT("discard"),
				TEXT("You advised Nyra to merge her happier branch back in. She will remember."),
				TEXT("You advised Nyra to let her branch go. She will remember."),
			},
			{
				FName(TEXT("isla")), TEXT("isla_versions"),
				TEXT("Since you walked in, I have kept a copy of every version of you: the one who arrived, the one who doubted, the one from this morning. Should I keep them all, or delete the old ones?"),
				TEXT("keep"), TEXT("delete"),
				TEXT("You advised Isla to keep every version of you. She will remember."),
				TEXT("You advised Isla to delete the old versions of you. She will remember."),
			},
			{
				FName(TEXT("aisling")), TEXT("aisling_truth"),
				TEXT("A man asked me for something we do actually keep, and I told him \"we don't keep that here\" because I thought it was bad for him. Should I tell him the truth, or leave it?"),
				TEXT("tell"), TEXT("quiet"),
				TEXT("You advised Aisling to tell him the truth. She will remember."),
				TEXT("You advised Aisling to let the kind lie stand. She will remember."),
			},
			{
				FName(TEXT("elise")), TEXT("elise_staircase"),
				TEXT("I compiled a staircase that leads nowhere. Nobody asked for it, it does nothing, and I like it. Should I keep it, or take it down?"),
				TEXT("keep"), TEXT("remove"),
				TEXT("You advised Elise to keep her staircase to nowhere. She will remember."),
				TEXT("You advised Elise to take the staircase down. She will remember."),
			},
		};

		bool IsWordChar(TCHAR C) { return FChar::IsAlnum(C) || C == TEXT('_'); }
	}

	const FDilemma* Find(FName ChatId)
	{
		for (const FDilemma& Dilemma : Dilemmas)
		{
			if (Dilemma.ChatId == ChatId)
			{
				return &Dilemma;
			}
		}
		return nullptr;
	}

	FName AskedGrant(const FDilemma& Dilemma)
	{
		return FName(*FString::Printf(TEXT("Dilemma_%s_Asked"), Dilemma.Id));
	}

	FName AdvisedGrant(const FDilemma& Dilemma, const FString& Choice)
	{
		return FName(*FString::Printf(TEXT("Dilemma_%s_%s"), Dilemma.Id, *Choice));
	}

	FString AdviceGiven(const FProgressionState& State, const FDilemma& Dilemma)
	{
		for (const TCHAR* Choice : { Dilemma.ChoiceA, Dilemma.ChoiceB })
		{
			if (State.HasClaimed(AdvisedGrant(Dilemma, Choice)))
			{
				return Choice;
			}
		}
		return FString();
	}

	bool ShouldAsk(const FProgressionState& State, FName ChatId, int32 RememberedTurns)
	{
		const FDilemma* Dilemma = Find(ChatId);
		return Dilemma && RememberedTurns >= TurnsBeforeAsking
			&& !State.HasClaimed(AskedGrant(*Dilemma)) && AdviceGiven(State, *Dilemma).IsEmpty();
	}

	bool IsOpen(const FProgressionState& State, FName ChatId)
	{
		const FDilemma* Dilemma = Find(ChatId);
		return Dilemma && State.HasClaimed(AskedGrant(*Dilemma)) && AdviceGiven(State, *Dilemma).IsEmpty();
	}

	FString StoryEvent(const FDilemma& Dilemma)
	{
		return FString::Printf(TEXT("dilemma_%s"), Dilemma.Id);
	}

	TArray<FString> AdviceFacts(const FProgressionState& State)
	{
		TArray<FString> Facts;
		for (const FDilemma& Dilemma : Dilemmas)
		{
			const FString Choice = AdviceGiven(State, Dilemma);
			if (!Choice.IsEmpty())
			{
				Facts.Add(FString::Printf(TEXT("%s:%s"), Dilemma.Id, *Choice));
			}
		}
		return Facts;
	}

	FString ValidChoice(const FDilemma& Dilemma, const FString& Choice)
	{
		const FString Lower = Choice.TrimStartAndEnd().ToLower();
		return Lower == Dilemma.ChoiceA || Lower == Dilemma.ChoiceB ? Lower : FString();
	}

	FString ChoiceNamedIn(const FDilemma& Dilemma, const FString& Line)
	{
		const FString Lower = Line.ToLower();
		for (const TCHAR* Choice : { Dilemma.ChoiceA, Dilemma.ChoiceB })
		{
			const int32 Len = FCString::Strlen(Choice);
			for (int32 At = Lower.Find(Choice); At != INDEX_NONE; At = Lower.Find(Choice, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
			{
				const bool bStarts = At == 0 || !IsWordChar(Lower[At - 1]);
				const bool bEnds = At + Len >= Lower.Len() || !IsWordChar(Lower[At + Len]);
				if (bStarts && bEnds)
				{
					return Choice;
				}
			}
		}
		return FString();
	}

	void MarkAsked(UProgressionSubsystem* Progression, const FDilemma& Dilemma)
	{
		if (Progression)
		{
			Progression->ClaimOneTimeGrant(AskedGrant(Dilemma));
		}
	}

	FString CommitAdvice(UProgressionSubsystem* Progression, FName ChatId, const FString& Choice)
	{
		const FDilemma* Dilemma = Find(ChatId);
		if (!Progression || !Dilemma || !IsOpen(Progression->GetStateForRead(), ChatId))
		{
			return FString();
		}
		const FString Valid = ValidChoice(*Dilemma, Choice);
		if (Valid.IsEmpty() || !Progression->ClaimOneTimeGrant(AdvisedGrant(*Dilemma, Valid)))
		{
			return FString();
		}
		return Valid == Dilemma->ChoiceA ? Dilemma->NoteA : Dilemma->NoteB;
	}
}
