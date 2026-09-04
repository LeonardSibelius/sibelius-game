// ProgressionTypes.cpp — pure progression model (FUN-1). See header.

#include "ProgressionTypes.h"
#include "SlotParSheet.h"   // the self-test covers EqualsMath — see the meters block below

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

FString PowerVerbMemoir(EPowerVerb Verb)
{
	/* Walt's eight messages, docs/MEMOIR_VOICE.md — verbatim, and deliberately not
	   polished. Six are mapped to powers here; the Bally and San Diego County messages are
	   level placards. Moved out of SibeliusHUD.cpp so the Journal can show the player's
	   collected record as well as the twelve-second flash at the moment of earning. */
	switch (Verb)
	{
	case EPowerVerb::CodeVision:
		return TEXT("Hey SAIC, you could have used this AI skill on the CHCS project in 1988.");
	case EPowerVerb::Refactor:
		return TEXT("Hey Seagate, you could have used this AI skill in 1998.");
	case EPowerVerb::Compile:
		return TEXT("Hey IBM, you could have used this AI skill in 1995 on the San Francisco Project for distributed Java.");
	case EPowerVerb::TestDrive:
		return TEXT("Hey Motorola, you could have used this AI skill in 2001.");
	case EPowerVerb::Deploy:
		return TEXT("Hey Northrop Grumman, you could have used this AI skill on the Electronic Family Housing system for the Navy in 2002.");
	case EPowerVerb::Generate:
		return TEXT("Hey Army Recruiting, you could have used this AI skill on iKrome in 2022. It is being retired now, like all the rest.");
	default:
		return FString();
	}
}

const TArray<FString>& AllMemoirMessages()
{
	/* docs/MEMOIR_VOICE.md, verbatim, chronological. Walt wrote these in one sitting after
	   saying he was too discouraged to write memoir sentences; the doc's instruction is
	   "do not polish the anger out", so they are copied and not edited.

	   The Bally line is the one the whole game points at: the data warehouse and the
	   accounting reports behind the Slot Data System, and never the machine itself. The
	   player reads it a few paces from a slot machine he built. */
	static const TArray<FString> Messages = {
		TEXT("Hey SAIC, you could have used this AI skill on the CHCS project in 1988."),
		TEXT("Hey IBM, you could have used this AI skill in 1995 on the San Francisco Project for distributed Java."),
		TEXT("Hey Seagate, you could have used this AI skill in 1998."),
		TEXT("Hey Motorola, you could have used this AI skill in 2001."),
		TEXT("Hey Northrop Grumman, you could have used this AI skill on the Electronic Family Housing system for the Navy in 2002."),
		TEXT("Hey San Diego County, you could have used this AI skill on the Probation Case Management System in 2005."),
		TEXT("Hey Bally, you could have used this AI skill on the Slot Data System in 2007. I built the warehouse and the reports. I never got to build the machine."),
		TEXT("Hey Army Recruiting, you could have used this AI skill on iKrome in 2022. It is being retired now, like all the rest.")
	};
	return Messages;
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

void FProgressionState::RememberGeneratedSite(FName LevelName, FName EntryId,
	const FTransform& Transform, const FGuid& ObjectId)
{
	if (LevelName.IsNone() || EntryId.IsNone() || !ObjectId.IsValid())
	{
		return;   // an unidentifiable record is worse than none: it cannot be forgotten
	}

	for (FGeneratedSiteRecord& R : GeneratedSites)
	{
		if (R.ObjectId == ObjectId)
		{
			R.LevelName = LevelName;
			R.EntryId = EntryId;
			R.Transform = Transform;
			return;
		}
	}

	FGeneratedSiteRecord R;
	R.LevelName = LevelName;
	R.EntryId = EntryId;
	R.Transform = Transform;
	R.ObjectId = ObjectId;
	GeneratedSites.Add(R);
}

bool FProgressionState::ForgetGeneratedSite(const FGuid& ObjectId)
{
	if (!ObjectId.IsValid())
	{
		return false;
	}
	return GeneratedSites.RemoveAll(
		[&ObjectId](const FGeneratedSiteRecord& R) { return R.ObjectId == ObjectId; }) > 0;
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

	// Fresh state: Code Vision only, starting stake of 50 (five poker hands).
	if (!S.IsUnlocked(EPowerVerb::CodeVision)) { OutError = TEXT("fresh state must start with Code Vision"); return false; }
	if (S.IsUnlocked(EPowerVerb::Refactor))    { OutError = TEXT("fresh state must NOT have Refactor"); return false; }
	if (S.NumUnlocked() != 1)                  { OutError = TEXT("fresh state must have exactly 1 power"); return false; }
	if (S.Sauce != 50)                         { OutError = TEXT("fresh state must start with 50 sauce"); return false; }
	if (!S.TrySpendSauce(50) || S.Sauce != 0)  { OutError = TEXT("starting stake must be spendable down to 0"); return false; }

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

	/* The floor report's meters (docs/FLOOR_REPORT.md step 2).
	   Delta/Add is what keeps the lifetime record honest across repeated commits, and the
	   failure mode is silent — a machine that plays perfectly while the report drifts. */
	{
		if (S.SlotLifetimeMeters.TotalSpins() != 0) { OutError = TEXT("fresh state must have zero lifetime meters"); return false; }
		if (S.SlotLifetimeMeters.MeasuredRtpPercent() >= 0.0) { OutError = TEXT("unplayed meters must report RTP -1, not 0"); return false; }

		FSlotMeters Session;
		Session.BaseSpins = 100; Session.FreeSpins = 19; Session.CoinIn = 15000;
		Session.CoinOut = 14000; Session.PayingSpins = 45; Session.BonusTriggers = 3;
		Session.BiggestWin = 900;

		// First commit: a delta from nothing is the whole session.
		FSlotMeters Committed;                                  // nothing banked yet
		S.SlotLifetimeMeters.Add(Session.Delta(Committed));
		Committed = Session;
		if (S.SlotLifetimeMeters.BaseSpins != 100)  { OutError = TEXT("first commit must bank the whole session"); return false; }
		if (S.SlotLifetimeMeters.CoinIn != 15000)   { OutError = TEXT("first commit must bank coin in"); return false; }

		// Committing again with NO further play must be a no-op. This is the double-count
		// guard: the cabinet commits on every close, and most closes follow no play.
		S.SlotLifetimeMeters.Add(Session.Delta(Committed));
		if (S.SlotLifetimeMeters.BaseSpins != 100)  { OutError = TEXT("re-committing an unchanged session must not double-count"); return false; }
		if (S.SlotLifetimeMeters.CoinOut != 14000)  { OutError = TEXT("re-commit must not double-count coin out"); return false; }

		// More play, then a second commit: only the new part lands.
		Session.BaseSpins = 150; Session.CoinIn = 22500; Session.CoinOut = 21000;
		Session.BiggestWin = 600;                               // LOWER than the record
		S.SlotLifetimeMeters.Add(Session.Delta(Committed));
		if (S.SlotLifetimeMeters.BaseSpins != 150)  { OutError = TEXT("second commit must bank only the delta"); return false; }
		if (S.SlotLifetimeMeters.CoinIn != 22500)   { OutError = TEXT("second commit coin in must be the running total"); return false; }
		if (S.SlotLifetimeMeters.BiggestWin != 900) { OutError = TEXT("BiggestWin is a maximum and must never fall"); return false; }

		// Measured figures, on the totals above.
		if (!FMath::IsNearlyEqual(S.SlotLifetimeMeters.MeasuredRtpPercent(), 100.0 * 21000.0 / 22500.0, 0.001))
		{
			OutError = TEXT("MeasuredRtpPercent must be CoinOut/CoinIn"); return false;
		}
		if (!FMath::IsNearlyEqual(S.SlotLifetimeMeters.MeasuredHitPercent(), 100.0 * 45.0 / 150.0, 0.001))
		{
			OutError = TEXT("MeasuredHitPercent must divide by BASE spins"); return false;
		}
	}

	/* A par change must not be triggered by a repaint (EqualsMath ignores the name).
	   If this regresses, opening the panel silently clears the player's session meters. */
	{
		FSlotParSheet A = FSlotParSheet::CelestialFortune();
		FSlotParSheet B = FSlotParSheet::CelestialFortune();
		B.Name = TEXT("Celestial Fortune (edited)");
		if (!A.EqualsMath(B)) { OutError = TEXT("EqualsMath must ignore the display name"); return false; }

		B.Strip[0] = (B.Strip[0] == ESlotSymbol::Wild) ? ESlotSymbol::Star : ESlotSymbol::Wild;
		if (A.EqualsMath(B)) { OutError = TEXT("EqualsMath must notice a strip change"); return false; }

		FSlotParSheet C = FSlotParSheet::CelestialFortune();
		for (FSlotPayRow& Row : C.PayTable)
		{
			if (Row.Symbol == ESlotSymbol::Seven) { Row.Pay5 = 4000.0; }
		}
		if (A.EqualsMath(C)) { OutError = TEXT("EqualsMath must notice a paytable change"); return false; }
	}

	// UnlockAll covers all six.
	S.UnlockAll();
	if (S.NumUnlocked() != static_cast<int32>(EPowerVerb::Count))
	{
		OutError = TEXT("UnlockAll must unlock every verb");
		return false;
	}

	return true;
}
