// ProgressionSmokeTestCommandlet.cpp — FUN-1/FUN-2 headless gate. See header.
//
// NAMED namespace + function-scoped `using` (unity-build safe), no variable
// shadowing (warnings-as-errors). Editor-closed.

#include "ProgressionSmokeTestCommandlet.h"
#include "ProgressionTypes.h"
#include "ProgressionSaveGame.h"
#include "SauceShop.h"
#include "SibeliusSaveIO.h"

#include "UObject/Package.h"   // GetTransientPackage

DEFINE_LOG_CATEGORY_STATIC(LogProgressionSmoke, Log, All);

namespace ProgressionSmokeTestNS
{
	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogProgressionSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogProgressionSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};

	// Sandbox slot so the gate can never touch a real player save.
	const FString SandboxSlot = TEXT("ProgressionSmokeSandbox");
}

UProgressionSmokeTestCommandlet::UProgressionSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UProgressionSmokeTestCommandlet::Main(const FString& Params)
{
	using namespace ProgressionSmokeTestNS;

	UE_LOG(LogProgressionSmoke, Display, TEXT("=== FUN-1/FUN-2 Progression smoke test ==="));

	FResult R;

	// --- 1) The pure model's own invariants.
	{
		FString SelfTestError;
		const bool bSelfTest = RunProgressionSelfTest(SelfTestError);
		R.Check(bSelfTest, FString::Printf(TEXT("FProgressionState self-test (%s)"),
			bSelfTest ? TEXT("ok") : *SelfTestError));
	}

	// --- 2) The exec-cheat name parser accepts loose forms and rejects junk.
	{
		EPowerVerb Verb = EPowerVerb::CodeVision;
		R.Check(ParsePowerVerb(TEXT("refactor"), Verb) && Verb == EPowerVerb::Refactor, TEXT("parse 'refactor'"));
		R.Check(ParsePowerVerb(TEXT("Test-Drive"), Verb) && Verb == EPowerVerb::TestDrive, TEXT("parse 'Test-Drive'"));
		R.Check(ParsePowerVerb(TEXT("vision"), Verb) && Verb == EPowerVerb::CodeVision, TEXT("parse 'vision' alias"));
		R.Check(!ParsePowerVerb(TEXT("frobnicate"), Verb), TEXT("reject unknown power name"));
	}

	// --- 2.5) The cauldron catalog reflects the state (FUN-3).
	{
		FProgressionState ShopState;
		TArray<FSauceOffer> Offers;

		// Fresh state: 5 locked powers (Code Vision is pre-owned) + 2 upgrades.
		FSauceShop::BuildOffers(ShopState, Offers);
		R.Check(Offers.Num() == 7, FString::Printf(TEXT("fresh catalog has 7 offers (got %d)"), Offers.Num()));
		const bool bNoCodeVisionOffer = !Offers.ContainsByPredicate([](const FSauceOffer& O)
			{ return O.bIsPowerUnlock && O.Power == EPowerVerb::CodeVision; });
		R.Check(bNoCodeVisionOffer, TEXT("pre-owned Code Vision is never on sale"));

		// Unlocking a power removes its offer.
		ShopState.Unlock(EPowerVerb::Refactor);
		FSauceShop::BuildOffers(ShopState, Offers);
		R.Check(Offers.Num() == 6, TEXT("unlocked power leaves the catalog"));

		// A maxed-out upgrade leaves the catalog.
		for (int32 i = 0; i < FSauceShop::SlapMaxPurchases; ++i)
		{
			ShopState.RecordPurchase(TEXT("Slap.Mighty"));
		}
		FSauceShop::BuildOffers(ShopState, Offers);
		const bool bSlapGone = !Offers.ContainsByPredicate([](const FSauceOffer& O)
			{ return O.Key == FName(TEXT("Slap.Mighty")); });
		R.Check(bSlapGone, TEXT("maxed upgrade leaves the catalog"));
	}

	// --- 3) Save round-trip on the sandbox slot: write -> load -> compare -> delete.
	{
		FSibeliusSaveIO::Delete(SandboxSlot); // pre-clean a crashed prior run

		UProgressionSaveGame* Written = NewObject<UProgressionSaveGame>(GetTransientPackage());
		Written->SaveVersion = UProgressionSaveGame::CurrentSaveVersion;
		Written->State.Unlock(EPowerVerb::Refactor);
		Written->State.Unlock(EPowerVerb::Generate);
		Written->State.AddSauce(123);

		/* Capture what was actually written rather than asserting a literal.
		   This check read `== 123` and went RED the moment a fresh state started with a
		   50-sauce stake (v0.9.3) — 50 + 123 = 173, the save was perfectly fine, and the
		   only thing broken was the test's arithmetic. A gate that fails for a reason
		   nobody has to act on stops being read, which costs more than the check is
		   worth. The question here is "does sauce survive a round trip", not "is sauce
		   123", so ask that instead and let the starting stake move freely. */
		const int32 ExpectedSauce = Written->State.Sauce;
		Written->State.Claim(TEXT("Smoke.GrantA"));
		Written->State.Claim(TEXT("Smoke.GrantB"));
		Written->State.RecordPurchase(TEXT("Budget.Generate"));
		Written->State.RecordPurchase(TEXT("Budget.Generate"));

		/* The lifetime hard meters (docs/FLOOR_REPORT.md step 2). FSlotMeters is a NESTED
		   USTRUCT inside the saved state, so SaveGame serialization has to recurse into it
		   for its members to persist at all. That is the entire persistence claim of this
		   step, and its failure would be quiet in the worst way — meters that count
		   perfectly all session and read zero after a restart. Prove it, do not assume it.

		   CoinIn is deliberately above INT32_MAX: the fields are int64 precisely so a long
		   life on the machine cannot wrap, and a silent narrowing anywhere in the pipe
		   would show up here rather than in a player's save a year from now. */
		Written->State.SlotLifetimeMeters.BaseSpins     = 1234;
		Written->State.SlotLifetimeMeters.FreeSpins     = 234;
		Written->State.SlotLifetimeMeters.CoinIn        = 5000000000LL;   // > INT32_MAX
		Written->State.SlotLifetimeMeters.CoinOut       = 4778350000LL;
		Written->State.SlotLifetimeMeters.PayingSpins   = 383;
		Written->State.SlotLifetimeMeters.BonusTriggers = 39;
		Written->State.SlotLifetimeMeters.BiggestWin    = 30600;

		R.Check(FSibeliusSaveIO::Commit(Written, SandboxSlot), TEXT("commit to sandbox slot"));

		UProgressionSaveGame* Loaded = Cast<UProgressionSaveGame>(FSibeliusSaveIO::Load(SandboxSlot));
		R.Check(Loaded != nullptr, TEXT("load from sandbox slot"));
		if (Loaded)
		{
			R.Check(Loaded->SaveVersion == UProgressionSaveGame::CurrentSaveVersion, TEXT("round-trip: SaveVersion stamped"));
			R.Check(Loaded->State.Sauce == ExpectedSauce,
				FString::Printf(TEXT("round-trip: sauce survives (%d)"), ExpectedSauce));
			R.Check(Loaded->State.IsUnlocked(EPowerVerb::CodeVision), TEXT("round-trip: starter power survives"));
			R.Check(Loaded->State.IsUnlocked(EPowerVerb::Refactor), TEXT("round-trip: unlocked Refactor survives"));
			R.Check(Loaded->State.IsUnlocked(EPowerVerb::Generate), TEXT("round-trip: unlocked Generate survives"));
			R.Check(!Loaded->State.IsUnlocked(EPowerVerb::Deploy), TEXT("round-trip: locked Deploy stays locked"));
			R.Check(Loaded->State.HasClaimed(TEXT("Smoke.GrantA")), TEXT("round-trip: claimed grant survives"));
			R.Check(!Loaded->State.Claim(TEXT("Smoke.GrantB")), TEXT("round-trip: re-claim still refused"));
			R.Check(Loaded->State.GetPurchaseCount(TEXT("Budget.Generate")) == 2, TEXT("round-trip: purchase counts survive"));

			const FSlotMeters& LM = Loaded->State.SlotLifetimeMeters;
			R.Check(LM.BaseSpins == 1234 && LM.FreeSpins == 234 && LM.PayingSpins == 383
				&& LM.BonusTriggers == 39 && LM.BiggestWin == 30600,
				TEXT("round-trip: lifetime meter counts survive (nested USTRUCT recurses)"));
			R.Check(LM.CoinIn == 5000000000LL && LM.CoinOut == 4778350000LL,
				FString::Printf(TEXT("round-trip: int64 coin in/out survive above INT32_MAX (%lld / %lld)"),
					LM.CoinIn, LM.CoinOut));
			R.Check(FMath::Abs(LM.MeasuredRtpPercent() - 95.567) < 0.001,
				FString::Printf(TEXT("round-trip: measured RTP reads %.3f%% from the loaded meters"), LM.MeasuredRtpPercent()));
		}

		R.Check(FSibeliusSaveIO::Delete(SandboxSlot), TEXT("sandbox slot cleanup"));
	}

	UE_LOG(LogProgressionSmoke, Display, TEXT("=== Progression smoke test: %d failure(s) ==="), R.Failures);
	return R.Failures == 0 ? 0 : 1;
}
