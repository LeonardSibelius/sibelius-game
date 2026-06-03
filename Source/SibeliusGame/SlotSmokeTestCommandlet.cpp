// SlotSmokeTestCommandlet.cpp
//
// SIB-34 / S1 — the regulator's suite. See header.
//
// [HARD] Determinism: same seed => identical win sequence; different seed differs (SL2)
// [HARD] Sanity: every win finite and >= 0; grid always 15 valid symbols
// [HARD] RTP within the WIDE catastrophic band [75%, 115%]
// [HARD] Hit frequency within [10%, 50%]
// [HARD] Bonus trigger rate within [1/200, 1/15] of base spins
// [HARD] Max single-spin win <= 10,000x total bet
// [WARN] RTP outside the DESIGN band [88%, 102%] (tune the par sheet, then tighten)
//
// CP3 lesson #6: helpers live in the NAMED namespace SlotSmokeTestNS.

#include "SlotSmokeTestCommandlet.h"
#include "SlotGameModel.h"
#include "SlotTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlotSmoke, Log, All);

namespace SlotSmokeTestNS
{
	const int32 BaseSpins = 1000000;
	const int32 TotalBet = 150;            // multiple of 15 lines
	const int32 SeedA = 42;
	const int32 SeedB = 1337;

	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition) { UE_LOG(LogSlotSmoke, Display, TEXT("  [PASS] %s"), *Label); }
			else { ++Failures; UE_LOG(LogSlotSmoke, Error, TEXT("  [FAIL] %s"), *Label); }
		}
		void Warn(bool bCondition, const FString& Label)
		{
			UE_LOG(LogSlotSmoke, Display, TEXT("  [%s] %s"), bCondition ? TEXT("PASS") : TEXT("WARN"), *Label);
		}
	};
}

USlotSmokeTestCommandlet::USlotSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 USlotSmokeTestCommandlet::Main(const FString& /*Params*/)
{
	using namespace SlotSmokeTestNS;

	UE_LOG(LogSlotSmoke, Display, TEXT("=== SIB-34 S1 slot model smoke test (the regulator's suite) ==="));

	FResult R;

	/* ---------- determinism (SL2) ---------- */
	{
		USlotGameModel* A = NewObject<USlotGameModel>();
		USlotGameModel* B = NewObject<USlotGameModel>();
		USlotGameModel* C = NewObject<USlotGameModel>();
		A->Init(SeedA); B->Init(SeedA); C->Init(SeedB);

		bool bSameSeedIdentical = true;
		bool bDiffSeedDiffers = false;
		for (int32 i = 0; i < 1000; ++i)
		{
			const FSlotSpinResult Ra = A->Spin(TotalBet);
			const FSlotSpinResult Rb = B->Spin(TotalBet);
			const FSlotSpinResult Rc = C->Spin(TotalBet);
			if (Ra.TotalWin != Rb.TotalWin || Ra.Grid != Rb.Grid) { bSameSeedIdentical = false; }
			if (Ra.TotalWin != Rc.TotalWin || Ra.Grid != Rc.Grid) { bDiffSeedDiffers = true; }
		}
		R.Check(bSameSeedIdentical, TEXT("Determinism: same seed => identical 1000-spin sequence"));
		R.Check(bDiffSeedDiffers, TEXT("Sanity: different seed => different sequence"));
	}

	/* ---------- the million-spin distribution run ---------- */
	{
		USlotGameModel* M = NewObject<USlotGameModel>();
		M->Init(SeedA);

		double TotalBetSum = 0.0, TotalWinSum = 0.0, MaxWinX = 0.0;
		int64 SpinsTotal = 0, BaseDone = 0, Hits = 0, BonusTriggers = 0, FreeSpinsPlayed = 0;
		bool bAllFinite = true, bGridValid = true;

		const double StartTime = FPlatformTime::Seconds();
		while (BaseDone < BaseSpins)
		{
			const FSlotSpinResult Res = M->Spin(TotalBet);
			++SpinsTotal;
			if (Res.bWasFreeSpin) { ++FreeSpinsPlayed; }
			else { ++BaseDone; TotalBetSum += TotalBet; }

			TotalWinSum += Res.TotalWin;
			if (Res.TotalWin > 0.0) { ++Hits; }
			if (Res.bBonusTriggered) { ++BonusTriggers; }

			const double WinX = Res.TotalWin / static_cast<double>(TotalBet);
			if (WinX > MaxWinX) { MaxWinX = WinX; }
			if (!FMath::IsFinite(Res.TotalWin) || Res.TotalWin < 0.0) { bAllFinite = false; }
			if (Res.Grid.Num() != 15) { bGridValid = false; }
		}
		const double Elapsed = FPlatformTime::Seconds() - StartTime;

		const double RTP = 100.0 * TotalWinSum / TotalBetSum;
		const double HitFreq = 100.0 * static_cast<double>(Hits) / static_cast<double>(SpinsTotal);
		const double BonusRate = static_cast<double>(BonusTriggers) / static_cast<double>(BaseDone); // per base spin
		const double BonusOneIn = (BonusRate > 0.0) ? (1.0 / BonusRate) : 0.0;

		/* ---------- THE PAR SHEET REPORT ---------- */
		UE_LOG(LogSlotSmoke, Display, TEXT("  ---------------- PAR SHEET REPORT ----------------"));
		UE_LOG(LogSlotSmoke, Display, TEXT("  Base spins        : %lld   (+%lld free spins, %lld total)"), BaseDone, FreeSpinsPlayed, SpinsTotal);
		UE_LOG(LogSlotSmoke, Display, TEXT("  RTP               : %.3f %%"), RTP);
		UE_LOG(LogSlotSmoke, Display, TEXT("  Hit frequency     : %.2f %% of all spins"), HitFreq);
		UE_LOG(LogSlotSmoke, Display, TEXT("  Bonus trigger     : 1 in %.1f base spins (%lld triggers)"), BonusOneIn, BonusTriggers);
		UE_LOG(LogSlotSmoke, Display, TEXT("  Max single-spin   : %.1fx total bet"), MaxWinX);
		UE_LOG(LogSlotSmoke, Display, TEXT("  Sim speed         : %.2fs for %lld spins"), Elapsed, SpinsTotal);
		UE_LOG(LogSlotSmoke, Display, TEXT("  --------------------------------------------------"));

		/* ---------- assertions ---------- */
		R.Check(bAllFinite, TEXT("All wins finite and non-negative"));
		R.Check(bGridValid, TEXT("Grid always 5x3 valid symbols"));
		R.Check(RTP >= 75.0 && RTP <= 115.0, FString::Printf(TEXT("RTP %.2f%% within catastrophic band [75, 115]"), RTP));
		R.Check(HitFreq >= 10.0 && HitFreq <= 50.0, FString::Printf(TEXT("Hit frequency %.2f%% within [10, 50]"), HitFreq));
		R.Check(BonusOneIn >= 15.0 && BonusOneIn <= 200.0, FString::Printf(TEXT("Bonus 1-in-%.1f within [15, 200]"), BonusOneIn));
		R.Check(MaxWinX <= 10000.0, FString::Printf(TEXT("Max win %.1fx <= 10000x exposure cap"), MaxWinX));
		R.Warn(RTP >= 88.0 && RTP <= 102.0, FString::Printf(TEXT("RTP %.2f%% within DESIGN band [88, 102] (tune par sheet if WARN)"), RTP));
	}

	if (R.Failures == 0)
	{
		UE_LOG(LogSlotSmoke, Display, TEXT("=== SLOT SMOKE TEST PASSED (S1 green — the machine's math holds). ==="));
		return 0;
	}
	UE_LOG(LogSlotSmoke, Error, TEXT("=== SLOT SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
}
