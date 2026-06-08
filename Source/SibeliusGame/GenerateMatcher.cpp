// GenerateMatcher.cpp — SIB-30 Ch6 SPIKE. Pure keyword-overlap matcher.

#include "GenerateMatcher.h"

TArray<FString> TokenizeGenerateInput(const FString& RawInput)
{
	const FString Lower = RawInput.ToLower();

	// Replace every non-alphanumeric char with a space, then split on whitespace.
	FString Cleaned;
	Cleaned.Reserve(Lower.Len());
	for (const TCHAR C : Lower)
	{
		Cleaned.AppendChar(FChar::IsAlnum(C) ? C : TEXT(' '));
	}

	TArray<FString> Tokens;
	Cleaned.ParseIntoArrayWS(Tokens); // splits on whitespace, drops empties
	return Tokens;
}

// # of input tokens that exactly match any (lowercased) keyword of the entry. The
// entry's keywords are pipe-delimited in data; parse + normalize them here.
// CP3 lesson #6: file-static, not an anonymous namespace (unity-build safety).
static int32 ScoreGenerateEntry(const TArray<FString>& Tokens, const FGenerateCatalogEntry& Entry)
{
	TArray<FString> KeywordList;
	Entry.GetKeywordTokens(KeywordList);

	TSet<FString> Keys;
	Keys.Reserve(KeywordList.Num());
	for (const FString& K : KeywordList)
	{
		Keys.Add(K.ToLower().TrimStartAndEnd());
	}

	int32 Score = 0;
	for (const FString& T : Tokens)
	{
		if (Keys.Contains(T))
		{
			++Score;
		}
	}
	return Score;
}

FGenerateResolution ClassifyGenerateRequest(const FString& RawInput,
	const TArray<FGenerateCatalogEntry>& Catalog, int32 RemainingBudget)
{
	FGenerateResolution Res;

	const TArray<FString> Tokens = TokenizeGenerateInput(RawInput);
	if (Tokens.Num() == 0)
	{
		Res.Outcome = EGenerateOutcome::RefusedNoMatch;
		Res.RefusalReason = TEXT("empty request");
		return Res;
	}

	// Score every entry; track the best score and how many entries share it.
	int32 BestScore = 0;
	int32 BestIdx = INDEX_NONE;
	int32 TopCount = 0;
	for (int32 i = 0; i < Catalog.Num(); ++i)
	{
		const int32 Score = ScoreGenerateEntry(Tokens, Catalog[i]);
		if (Score <= 0)
		{
			continue;
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIdx = i;
			TopCount = 1;
		}
		else if (Score == BestScore)
		{
			++TopCount;
		}
	}

	if (BestScore == 0 || BestIdx == INDEX_NONE)
	{
		Res.Outcome = EGenerateOutcome::RefusedNoMatch;
		Res.RefusalReason = TEXT("nothing in the catalog matched");
		return Res;
	}
	if (TopCount > 1)
	{
		// DECISION DB: a tie pushes back ("be more specific") rather than guessing.
		Res.Outcome = EGenerateOutcome::RefusedAmbiguous;
		Res.RefusalReason = TEXT("ambiguous — more than one entry matched equally");
		return Res;
	}

	const FGenerateCatalogEntry& Win = Catalog[BestIdx];
	if (Win.Cost > RemainingBudget)
	{
		Res.Outcome = EGenerateOutcome::RefusedOverBudget;
		Res.EntryId = Win.EntryId;
		Res.Cost = Win.Cost;
		Res.RefusalReason = FString::Printf(TEXT("over budget: cost %d > remaining %d"), Win.Cost, RemainingBudget);
		return Res;
	}

	Res.Outcome = EGenerateOutcome::Resolved;
	Res.EntryId = Win.EntryId;
	Res.Cost = Win.Cost;
	return Res;
}
