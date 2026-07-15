// ProgressionTypes.cpp — pure progression model (FUN-1). See header.

#include "ProgressionTypes.h"

namespace
{
	uint8 PowerBit(EPowerVerb Verb)
	{
		return 1 << static_cast<uint8>(Verb);
	}
}

FString PowerVerbDisplayName(EPowerVerb Verb)
{
	switch (Verb)
	{
	case EPowerVerb::CodeVision: return TEXT("CODE VISION");
	case EPowerVerb::Refactor:   return TEXT("REFACTOR");
	case EPowerVerb::Compile:    return TEXT("COMPILE");
	case EPowerVerb::TestDrive:  return TEXT("TEST-DRIVE");
	case EPowerVerb::Deploy:     return TEXT("DEPLOY");
	case EPowerVerb::Generate:   return TEXT("GENERATE");
	default:                     return TEXT("UNKNOWN");
	}
}

bool ParsePowerVerb(const FString& Name, EPowerVerb& OutVerb)
{
	// Compare on a lowered, separator-stripped form so "test-drive", "TestDrive",
	// and "testdrive" all resolve.
	FString Clean = Name.ToLower();
	Clean.ReplaceInline(TEXT("-"), TEXT(""));
	Clean.ReplaceInline(TEXT("_"), TEXT(""));
	Clean.ReplaceInline(TEXT(" "), TEXT(""));

	if (Clean == TEXT("codevision") || Clean == TEXT("vision")) { OutVerb = EPowerVerb::CodeVision; return true; }
	if (Clean == TEXT("refactor"))                              { OutVerb = EPowerVerb::Refactor;   return true; }
	if (Clean == TEXT("compile") || Clean == TEXT("build"))     { OutVerb = EPowerVerb::Compile;    return true; }
	if (Clean == TEXT("testdrive") || Clean == TEXT("branch"))  { OutVerb = EPowerVerb::TestDrive;  return true; }
	if (Clean == TEXT("deploy"))                                { OutVerb = EPowerVerb::Deploy;     return true; }
	if (Clean == TEXT("generate"))                              { OutVerb = EPowerVerb::Generate;   return true; }
	return false;
}

bool FProgressionState::IsUnlocked(EPowerVerb Verb) const
{
	return (UnlockedMask & PowerBit(Verb)) != 0;
}

bool FProgressionState::Unlock(EPowerVerb Verb)
{
	if (IsUnlocked(Verb))
	{
		return false;
	}
	UnlockedMask |= PowerBit(Verb);
	return true;
}

void FProgressionState::UnlockAll()
{
	for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
	{
		UnlockedMask |= PowerBit(static_cast<EPowerVerb>(i));
	}
}

int32 FProgressionState::NumUnlocked() const
{
	int32 Num = 0;
	for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
	{
		if (IsUnlocked(static_cast<EPowerVerb>(i)))
		{
			++Num;
		}
	}
	return Num;
}

void FProgressionState::AddSauce(int32 Amount)
{
	if (Amount > 0)
	{
		Sauce += Amount;
	}
}

bool FProgressionState::TrySpendSauce(int32 Amount)
{
	if (Amount < 0 || Sauce < Amount)
	{
		return false;
	}
	Sauce -= Amount;
	return true;
}

bool FProgressionState::HasClaimed(FName GrantKey) const
{
	return ClaimedGrants.Contains(GrantKey);
}

bool FProgressionState::Claim(FName GrantKey)
{
	if (GrantKey.IsNone() || HasClaimed(GrantKey))
	{
		return false;
	}
	ClaimedGrants.Add(GrantKey);
	return true;
}

int32 FProgressionState::GetPurchaseCount(FName OfferKey) const
{
	const int32* Found = PurchaseCounts.Find(OfferKey);
	return Found ? *Found : 0;
}

void FProgressionState::RecordPurchase(FName OfferKey)
{
	if (!OfferKey.IsNone())
	{
		++PurchaseCounts.FindOrAdd(OfferKey);
	}
}

int32 FProgressionState::GetStat(FName Key) const
{
	const int32* Found = LifetimeStats.Find(Key);
	return Found ? *Found : 0;
}

void FProgressionState::BumpStat(FName Key, int32 Delta)
{
	if (!Key.IsNone() && Delta > 0)
	{
		LifetimeStats.FindOrAdd(Key) += Delta;
	}
}

void FProgressionState::RaiseStat(FName Key, int32 Value)
{
	if (!Key.IsNone() && Value > GetStat(Key))
	{
		LifetimeStats.FindOrAdd(Key) = Value;
	}
}

bool RunProgressionSelfTest(FString& OutError)
{
	FProgressionState S;

	// Fresh state: Code Vision only, zero sauce.
	if (!S.IsUnlocked(EPowerVerb::CodeVision)) { OutError = TEXT("fresh state must start with Code Vision"); return false; }
	if (S.IsUnlocked(EPowerVerb::Refactor))    { OutError = TEXT("fresh state must NOT have Refactor"); return false; }
	if (S.NumUnlocked() != 1)                  { OutError = TEXT("fresh state must have exactly 1 power"); return false; }
	if (S.Sauce != 0)                          { OutError = TEXT("fresh state must have 0 sauce"); return false; }

	// Unlock semantics: first true, repeat false, state sticks.
	if (!S.Unlock(EPowerVerb::Refactor))       { OutError = TEXT("first Unlock must return true"); return false; }
	if (S.Unlock(EPowerVerb::Refactor))        { OutError = TEXT("second Unlock must return false"); return false; }
	if (!S.IsUnlocked(EPowerVerb::Refactor))   { OutError = TEXT("Refactor must be unlocked after Unlock"); return false; }

	// Sauce ledger: never negative, ignores bad amounts, exact spends.
	S.AddSauce(100);
	S.AddSauce(-50);                                   // ignored
	if (S.Sauce != 100)                        { OutError = TEXT("AddSauce must ignore negatives"); return false; }
	if (S.TrySpendSauce(101))                  { OutError = TEXT("overspend must fail"); return false; }
	if (S.Sauce != 100)                        { OutError = TEXT("failed spend must not change sauce"); return false; }
	if (!S.TrySpendSauce(100))                 { OutError = TEXT("exact spend must succeed"); return false; }
	if (S.Sauce != 0)                          { OutError = TEXT("sauce must be 0 after exact spend"); return false; }
	if (S.TrySpendSauce(-1))                   { OutError = TEXT("negative spend must fail"); return false; }

	// One-time grants: claim once, never twice, NAME_None never claims.
	if (!S.Claim(TEXT("Test.Grant")))          { OutError = TEXT("first Claim must return true"); return false; }
	if (S.Claim(TEXT("Test.Grant")))           { OutError = TEXT("second Claim must return false"); return false; }
	if (S.Claim(NAME_None))                    { OutError = TEXT("NAME_None must never claim"); return false; }

	// Purchase counts: zero until recorded, increments, NAME_None ignored.
	if (S.GetPurchaseCount(TEXT("Test.Offer")) != 0) { OutError = TEXT("unbought offer must count 0"); return false; }
	S.RecordPurchase(TEXT("Test.Offer"));
	S.RecordPurchase(TEXT("Test.Offer"));
	S.RecordPurchase(NAME_None);
	if (S.GetPurchaseCount(TEXT("Test.Offer")) != 2) { OutError = TEXT("purchase count must be 2 after 2 buys"); return false; }
	if (S.GetPurchaseCount(NAME_None) != 0)          { OutError = TEXT("NAME_None must never record"); return false; }

	// Lifetime stats (APPEAL-5): default 0, bumps add, records only raise.
	if (S.GetStat(TEXT("Test.Stat")) != 0)     { OutError = TEXT("unknown stat must read 0"); return false; }
	S.BumpStat(TEXT("Test.Stat"));
	S.BumpStat(TEXT("Test.Stat"), 4);
	S.BumpStat(TEXT("Test.Stat"), -3);                 // ignored
	S.BumpStat(NAME_None, 100);                        // ignored
	if (S.GetStat(TEXT("Test.Stat")) != 5)     { OutError = TEXT("BumpStat must sum positive deltas only"); return false; }
	S.RaiseStat(TEXT("Test.Best"), 7);
	S.RaiseStat(TEXT("Test.Best"), 3);                 // lower — ignored
	if (S.GetStat(TEXT("Test.Best")) != 7)     { OutError = TEXT("RaiseStat must keep the max"); return false; }
	S.RaiseStat(TEXT("Test.Best"), 9);
	if (S.GetStat(TEXT("Test.Best")) != 9)     { OutError = TEXT("RaiseStat must accept a new record"); return false; }

	// UnlockAll covers all six.
	S.UnlockAll();
	if (S.NumUnlocked() != static_cast<int32>(EPowerVerb::Count))
	{
		OutError = TEXT("UnlockAll must unlock every verb");
		return false;
	}

	return true;
}
