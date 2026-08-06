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
#include "SlotParSheet.h"
#include "SlotParSheetMath.h"   // the closed form the technician's panel will show live
#include "SlotTypes.h"
#include "SlotScreenWidget.h"
#include "Engine/Texture2D.h"

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

		/* ---------- THE CLOSED FORM MUST AGREE WITH THE SIMULATION ----------
		   The technician's panel shows RTP live, computed in closed form because a
		   million spins cannot run per keystroke. If that formula and this simulation
		   ever disagree, the panel is lying to the player in real time — in a feature
		   whose entire purpose is to teach them what a par sheet is. So the two are
		   checked against each other on the shipped par sheet AND on deliberately odd
		   ones, where the wild-attribution rule is most likely to be got wrong.

		   Tolerance is sampling noise, not slack: a million spins puts the simulated
		   RTP within roughly a tenth of a point of truth for a machine this volatile. */
		{
			const FSlotParSheet Shipped = FSlotParSheet::CelestialFortune();
			const FSlotParSheetReport Exact = SlotParSheetMath::Analyze(Shipped);

			UE_LOG(LogSlotSmoke, Display, TEXT("  Closed form       : RTP %.3f %%  (base %.3f %%, +%.4f free spins/spin, trigger %.4f %%)"),
				Exact.RtpPercent, Exact.BaseRtpPercent, Exact.FreeSpinsPerBaseSpin, Exact.TriggerPercent);

			R.Check(Exact.bConverged, TEXT("Closed form converges on the shipped par sheet"));
			R.Check(FMath::Abs(Exact.RtpPercent - RTP) <= 0.35,
				FString::Printf(TEXT("Closed-form RTP %.3f%% agrees with %lld-spin sim %.3f%% (delta %.3f)"),
					Exact.RtpPercent, SpinsTotal, RTP, Exact.RtpPercent - RTP));

			// Trigger probability: the sim counts triggers across ALL spins, base and
			// free alike, so compare against that same denominator.
			const double SimTriggerPct = 100.0 * static_cast<double>(BonusTriggers) / static_cast<double>(SpinsTotal);
			R.Check(FMath::Abs(Exact.TriggerPercent - SimTriggerPct) <= 0.10,
				FString::Printf(TEXT("Closed-form trigger %.4f%% agrees with sim %.4f%%"), Exact.TriggerPercent, SimTriggerPct));

			// Contributions must account for the base game, or the panel's per-symbol
			// breakdown would not add up to the total it sits under.
			double ContribSum = 0.0;
			for (const FSlotSymbolContribution& C : Exact.Contributions) { ContribSum += C.BaseRtpPercent; }
			R.Check(FMath::Abs(ContribSum - Exact.BaseRtpPercent) <= 0.001,
				FString::Printf(TEXT("Per-symbol contributions sum to base RTP (%.4f vs %.4f)"), ContribSum, Exact.BaseRtpPercent));

			/* ---------- THE METERS MUST COUNT THE SAME GAME (docs/FLOOR_REPORT.md) ----
			   The floor report shows the player a MEASURED return and asks them to compare
			   it with par. That comparison is worthless if the meters count something
			   subtly different from what the closed form solves — and the likeliest way to
			   get it wrong is invisible: crediting free spins as wagered, or counting a
			   retrigger twice. Both would leave the machine playing perfectly while the
			   report quietly lied.

			   The million-spin loop above already accumulated every total independently,
			   so this costs nothing but the comparison. */
			const FSlotMeters& Met = M->GetSessionMeters();

			const bool bMetersAgreeWithLoop =
				   Met.BaseSpins     == BaseDone
				&& Met.FreeSpins     == FreeSpinsPlayed
				&& Met.BonusTriggers == BonusTriggers
				&& FMath::Abs(static_cast<double>(Met.CoinIn)  - TotalBetSum) < 1.0
				&& FMath::Abs(static_cast<double>(Met.CoinOut) - TotalWinSum) < 1.0
				&& FMath::Abs(static_cast<double>(Met.BiggestWin) - MaxWinX * TotalBet) < 1.0;

			R.Check(bMetersAgreeWithLoop,
				FString::Printf(TEXT("Meters match the independent tally (%lld/%lld spins, %lld in, %lld out)"),
					Met.BaseSpins, Met.FreeSpins, Met.CoinIn, Met.CoinOut));

			// The check that matters: what the meters MEASURE against what par PREDICTS.
			// Same tolerance as the sim comparison above — this is the same measurement,
			// reached by the code the player's page will actually read.
			const double MeterRtp = Met.MeasuredRtpPercent();
			R.Check(FMath::Abs(Exact.RtpPercent - MeterRtp) <= 0.35,
				FString::Printf(TEXT("Meter RTP %.3f%% agrees with closed form %.3f%% (delta %.3f)"),
					MeterRtp, Exact.RtpPercent, MeterRtp - Exact.RtpPercent));

			/* Hit frequency is a PAID-spin statistic in both places, and that agreement is
			   the load-bearing part: the meters and MeasureBySimulation must exclude free
			   spins the same way, or the page's "paid anything" row would sit beside a par
			   figure computed on a different denominator.

			   200k spins rather than the 10k default: the standard error on a ~31% rate is
			   0.46 points at 10k, which would force a tolerance so loose it could not catch
			   a denominator mistake. At 200k it is ~0.10, so 0.50 is a real bar. */
			FSlotParSheetReport Measured = Exact;
			SlotParSheetMath::MeasureBySimulation(Shipped, Measured, 200000);
			const double MeterHit = Met.MeasuredHitPercent();
			R.Check(FMath::Abs(Measured.HitFrequencyPercent - MeterHit) <= 0.50,
				FString::Printf(TEXT("Meter hit frequency %.2f%% agrees with simulation %.2f%% (delta %.2f)"),
					MeterHit, Measured.HitFrequencyPercent, MeterHit - Measured.HitFrequencyPercent));

			UE_LOG(LogSlotSmoke, Display, TEXT("  Meters            : RTP %.3f %%  hit %.2f %%  biggest %lld  bonus 1 in %.1f"),
				MeterRtp, MeterHit, Met.BiggestWin, Met.MeasuredBonusOneIn());

			/* ---------- THE CONFIDENCE BAND (docs/FLOOR_REPORT.md step 4) ----------
			   The floor report tells the player which measured returns count as normal.
			   Too WIDE and it excuses a genuinely broken machine; too NARROW and it calls
			   an ordinary session unusual — which is the failure that matters here,
			   because the page exists to say the opposite. */

			// The fix that motivated step 4. Volatility skips free spins entirely, so it
			// misses the bonus round — the largest single source of variance in the
			// machine. WageredVolatility credits each wagered spin with what its bonus
			// paid, so it MUST come out larger. If this ever inverts, the band is being
			// computed from the wrong number and every verdict on the page is too
			// confident.
			R.Check(Measured.WageredVolatility > Measured.Volatility,
				FString::Printf(TEXT("Wagered volatility %.3f exceeds per-spin %.3f (the bonus round's variance is counted)"),
					Measured.WageredVolatility, Measured.Volatility));

			// The band must shrink as 1/sqrt(N): four times the spins, half the width.
			// This is the property the whole lesson rests on, and it is exact — no
			// sampling noise, because it is arithmetic on a fixed volatility.
			const double Half1k = Measured.ConfidenceHalfWidth(1000);
			const double Half4k = Measured.ConfidenceHalfWidth(4000);
			R.Check(Half1k > 0.0 && FMath::Abs(Half4k - Half1k * 0.5) < 0.0001,
				FString::Printf(TEXT("Band narrows as 1/sqrt(N): %.3f pts at 1k, %.3f at 4k (half)"), Half1k, Half4k));

			// And the inverse must agree with the forward calculation: feeding the
			// spins-to-1-point figure back in has to yield a 1-point band.
			const double NeededFor1 = Measured.SpinsToMeasureWithin(1.0);
			const double BandAtNeeded = Measured.ConfidenceHalfWidth(static_cast<int64>(NeededFor1));
			R.Check(NeededFor1 > 0.0 && FMath::Abs(BandAtNeeded - 1.0) < 0.01,
				FString::Printf(TEXT("SpinsToMeasureWithin inverts the band: %s spins -> %.4f pts"),
					*FString::Printf(TEXT("%.0f"), NeededFor1), BandAtNeeded));

			UE_LOG(LogSlotSmoke, Display,
				TEXT("  Confidence        : wagered vol %.3f (per-spin %.3f)  |  after 300 spins +/- %.1f pts  |  1 pt needs %.0f spins"),
				Measured.WageredVolatility, Measured.Volatility,
				Measured.ConfidenceHalfWidth(300), NeededFor1);
		}

		/* ---------- the same check on VARIANT par sheets ----------
		   The shipped sheet has exactly ONE wild in forty stops, which is the case least
		   likely to expose a wild-attribution mistake. These variants make wilds common
		   enough that getting the rule wrong shows up as a clear divergence. */
		{
			struct FVariant { const TCHAR* Name; int32 ExtraWilds; double JackpotPay; bool bStripEarths; };
			static const FVariant Variants[] = {
				{ TEXT("wild-heavy"),   3, 1000.0, false },
				{ TEXT("wild-free"),   -1, 1000.0, false },
				{ TEXT("big-jackpot"),  0, 4000.0, false },
				// CONTROLS that isolate one half of the maths each:
				//   no-bonus       : no Earths at all, so no free spins. Total RTP == base
				//                    RTP, and any gap is purely the payline calculation.
				//   no-bonus-nowild: the above AND no wilds. The simplest machine this
				//                    model can express - if THIS disagrees, something very
				//                    basic is wrong.
				{ TEXT("no-bonus"),         0, 1000.0, true },
				{ TEXT("no-bonus-nowild"), -1, 1000.0, true },
			};

			for (const FVariant& V : Variants)
			{
				FSlotParSheet P = FSlotParSheet::CelestialFortune();
				P.Name = V.Name;

				if (V.bStripEarths)
				{
					for (ESlotSymbol& S : P.Strip)
					{
						if (S == ESlotSymbol::Earth) { S = ESlotSymbol::Moon; }
					}
				}

				// Convert Star stops to Wild (or the single Wild back to Star) so the
				// strip length — and therefore every other symbol's odds — is untouched.
				int32 ToChange = V.ExtraWilds;
				for (int32 i = 0; i < P.Strip.Num() && ToChange != 0; ++i)
				{
					if (ToChange > 0 && P.Strip[i] == ESlotSymbol::Star) { P.Strip[i] = ESlotSymbol::Wild; --ToChange; }
					else if (ToChange < 0 && P.Strip[i] == ESlotSymbol::Wild) { P.Strip[i] = ESlotSymbol::Star; ++ToChange; }
				}
				for (FSlotPayRow& Row : P.PayTable)
				{
					if (Row.Symbol == ESlotSymbol::Seven) { Row.Pay5 = V.JackpotPay; }
				}

				const FSlotParSheetReport Ex = SlotParSheetMath::Analyze(P);

				// Simulate the variant directly rather than trusting the formula twice.
				USlotGameModel* VM = NewObject<USlotGameModel>();
				VM->SetParSheet(P);
				VM->Init(4242);
				const int32 VBet = VM->NumLines();

				// 400k left the wild-heavy variant only 0.04 points inside tolerance —
				// not a wrong answer, just a noisy measurement of a very volatile
				// machine. Seeds are fixed so this is deterministic rather than flaky,
				// but a margin that thin would trip on any future strip tweak. 2M spins
				// costs about a third of a second and roughly halves the noise.
				const int64 VSpins = 2000000;

				// Split base-spin wins from free-spin wins so the two halves of the
				// closed form can be checked separately. A gap in one and not the other
				// says immediately which formula is wrong.
				double VWinBase = 0.0, VWinFree = 0.0, VBetSum = 0.0;
				int64 VBaseSpins = 0, VFreeSpins = 0;
				for (int64 i = 0; i < VSpins; ++i)
				{
					const FSlotSpinResult Res = VM->Spin(VBet);
					if (Res.bWasFreeSpin) { ++VFreeSpins; VWinFree += Res.TotalWin; }
					else                  { ++VBaseSpins; VBetSum += VBet; VWinBase += Res.TotalWin; }
				}
				const double VRtp     = 100.0 * (VWinBase + VWinFree) / VBetSum;
				const double VRtpBase = 100.0 * VWinBase / VBetSum;   // lines only
				const double VFreeRatio = (VBaseSpins > 0) ? static_cast<double>(VFreeSpins) / static_cast<double>(VBaseSpins) : 0.0;

				UE_LOG(LogSlotSmoke, Display,
					TEXT("  Variant '%s'  wilds %d  RTP exact %.3f sim %.3f  |  BASE exact %.3f sim %.3f  |  free/spin exact %.4f sim %.4f"),
					V.Name, P.CountOf(ESlotSymbol::Wild),
					Ex.RtpPercent, VRtp, Ex.BaseRtpPercent, VRtpBase, Ex.FreeSpinsPerBaseSpin, VFreeRatio);

				// The base-game formula on its own. On the no-bonus controls this is the
				// entire machine, so it must be tight.
				R.Check(FMath::Abs(Ex.BaseRtpPercent - VRtpBase) <= 0.40,
					FString::Printf(TEXT("Variant '%s': BASE closed form %.3f%% agrees with sim %.3f%% (delta %.3f)"),
						V.Name, Ex.BaseRtpPercent, VRtpBase, Ex.BaseRtpPercent - VRtpBase));

				// Tolerance scales with the machine's return, because the noise does.
				// Free spins pay 3x, so a variant returning 300% amplifies ordinary
				// sampling error threefold on a far more volatile distribution: the
				// wild-heavy total lands ~0.75 points out while its BASE figure — the
				// one that tests the formula itself — agrees to 0.08. An absolute bar
				// would fail a correct calculation for being measured on a wilder
				// machine. 0.3% of RTP, with a floor for ordinary sheets.
				// Floor is 0.80, not 0.60: big-jackpot cleared 0.60 by three THOUSANDTHS
				// of a point. Deterministic seeds mean that would not flake, but any
				// future strip tweak would tip it into a red gate that says nothing
				// about correctness. The BASE check above (0.40, absolute) is the one
				// that actually validates the formula; this one guards the free-spin
				// amplification, where high-variance sheets are measured, not derived.
				const double TotalTol = FMath::Max(0.80, 0.003 * Ex.RtpPercent);
				R.Check(FMath::Abs(Ex.RtpPercent - VRtp) <= TotalTol,
					FString::Printf(TEXT("Variant '%s': closed form %.3f%% agrees with sim %.3f%% (delta %.3f, tol %.3f)"),
						V.Name, Ex.RtpPercent, VRtp, Ex.RtpPercent - VRtp, TotalTol));

				// 'wild-free' is the control: with no wilds on the strip there is no
				// attribution question at all, so a divergence here would mean the
				// ordinary run maths is wrong rather than the wild rule.
				if (V.ExtraWilds < 0)
				{
					R.Check(P.CountOf(ESlotSymbol::Wild) == 0, TEXT("Control variant really has zero wilds"));
				}
			}
		}
	}

	/* ---------- S2: presentation assets (SC4) ---------- */
	{
		// The widget class must resolve (native class — a link/registration check).
		R.Check(USlotScreenWidget::StaticClass() != nullptr, TEXT("S2: USlotScreenWidget class resolves"));

		// All nine sprites must exist where the widget loads them. Runs AFTER
		// Tools/Scripts/import_symbol_sprites.py per the S2/S3 notes.
		static const TCHAR* Ids[] = { TEXT("star"), TEXT("moon"), TEXT("galaxy"), TEXT("saturn"),
		                              TEXT("mars"), TEXT("crown"), TEXT("seven"), TEXT("wild"), TEXT("scatter") };
		int32 Loaded = 0;
		for (const TCHAR* Id : Ids)
		{
			const FString Path = FString::Printf(TEXT("/Game/SlotFactory/SymbolSprites/T_sym_%s.T_sym_%s"), Id, Id);
			if (LoadObject<UTexture2D>(nullptr, *Path)) { ++Loaded; }
			else { UE_LOG(LogSlotSmoke, Error, TEXT("  missing sprite: %s"), *Path); }
		}
		R.Check(Loaded == 9, FString::Printf(TEXT("S2: %d/9 symbol sprites resolve in /Game/SlotFactory/SymbolSprites"), Loaded));

		/* The technician's panel body font (docs/FLOOR_REPORT.md step 3).
		   The meters page is columns of figures and only aligns in a monospace face. The
		   panel resolves this with a constructor FObjectFinder, and FObjectFinder FAILS
		   SILENTLY — a wrong path leaves the text block on the proportional default, which
		   still renders, still reads, and quietly misaligns every column. Nothing would
		   log. So assert the path resolves rather than trusting it.

		   This does NOT prove it stages into a package: the editor sees the whole disk.
		   The hard reference is what should carry it through the cook; verify in
		   Saved/Cooked before shipping. */
		R.Check(LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono")) != nullptr,
			TEXT("S2: panel body font /Engine/EngineFonts/DroidSansMono resolves"));
	}

	if (R.Failures == 0)
	{
		UE_LOG(LogSlotSmoke, Display, TEXT("=== SLOT SMOKE TEST PASSED (S1 math + S2 assets green). ==="));
		return 0;
	}
	UE_LOG(LogSlotSmoke, Error, TEXT("=== SLOT SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	return R.Failures;
}
