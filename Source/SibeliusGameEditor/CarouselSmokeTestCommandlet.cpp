// CarouselSmokeTestCommandlet.cpp — SIB-46 headless gate for the Carousel sim. See header.
//
// Pure sim math — no world is created, so there is no World->CleanupWorld() (the exit-3 guard only
// applies when a commandlet hand-inits a world; this one doesn't). NAMED namespace + function-scoped
// `using` (unity-build safe), no variable shadowing (warnings-as-errors). Editor-closed.

#include "CarouselSmokeTestCommandlet.h"
#include "CarouselSim.h"
#include "CarouselCharm.h"
#include "CarouselTypes.h"

#include "Math/RandomStream.h"
#include "UObject/Package.h"   // GetTransientPackage
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogCarouselSmoke, Log, All);

namespace CarouselSmokeTestNS
{
	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogCarouselSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogCarouselSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};
}

UCarouselSmokeTestCommandlet::UCarouselSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarouselSmokeTestCommandlet::Main(const FString& Params)
{
	// Function-scoped so the namespace doesn't leak into other TUs under unity build.
	using namespace CarouselSmokeTestNS;

	UE_LOG(LogCarouselSmoke, Display, TEXT("=== SIB-46 Carousel smoke test ==="));

	FResult R;

	// --- 1) The slice builds.
	TMap<FName, FSymbolDef> Symbols;
	FMachineBuild Build;
	FCarouselSim::BuildDefaultSlice(Symbols, Build);
	R.Check(Symbols.Num() >= 12, FString::Printf(TEXT("slice has >= 12 symbols (got %d)"), Symbols.Num()));
	R.Check(Build.ReelStrips.Num() == Build.NumReels, TEXT("one reel strip per reel"));
	R.Check(Build.ActivePaylines.Num() >= 1, FString::Printf(TEXT("build has paylines (got %d)"), Build.ActivePaylines.Num()));

	// --- 2) The spin pipeline resolves: a single deterministic spin fills the grid + non-negative chips.
	FCarouselSim Sim;
	Sim.Build = &Build;
	Sim.Symbols = &Symbols;

	TArray<UCarouselCharm*> Charms;
	for (const FName& Id : Build.OwnedCharms)
	{
		Charms.Add(UCarouselCharm::Make(Id, GetTransientPackage()));
	}

	FRandomStream Rng(7);
	FSpinContext Ctx;
	Ctx.Build = &Build;
	Ctx.Symbols = &Symbols;
	Ctx.Rng = &Rng;
	Ctx.Charms = Charms;
	Ctx.FreeSpinsRemaining = 0;
	Sim.Spin(Ctx);
	R.Check(Ctx.Grid.Num() == Build.NumReels * Build.RowsVisible, TEXT("spin fills the full grid"));
	R.Check(Ctx.SpinPayout >= 0, TEXT("spin payout is non-negative (integer chips)"));

	// --- 3) No infinite cascade: exercise the Cascade charm (the only one that requests re-spins) and
	//        assert the per-spin depth never exceeds the cap AND that it actually fired (cap is real).
	TArray<UCarouselCharm*> CascadeCharms;
	CascadeCharms.Add(UCarouselCharm::Make(TEXT("Cascade"), GetTransientPackage()));
	FRandomStream RngCascade(7);
	FSpinContext CascCtx;
	CascCtx.Build = &Build;
	CascCtx.Symbols = &Symbols;
	CascCtx.Rng = &RngCascade;
	CascCtx.Charms = CascadeCharms;
	CascCtx.FreeSpinsRemaining = 0;
	bool bCapHeld = true;
	int32 MaxCascadeSeen = 0;
	for (int32 i = 0; i < 2000; ++i)
	{
		Sim.Spin(CascCtx);
		MaxCascadeSeen = FMath::Max(MaxCascadeSeen, CascCtx.CascadeCount);
		if (CascCtx.CascadeCount > FCarouselSim::MaxCascadeDepth) { bCapHeld = false; }
	}
	R.Check(bCapHeld, FString::Printf(TEXT("cascade depth never exceeds MaxCascadeDepth=%d (max seen %d) — no infinite chain"),
		FCarouselSim::MaxCascadeDepth, MaxCascadeSeen));
	R.Check(MaxCascadeSeen > 0, TEXT("Cascade charm actually triggered re-spins (phase 6 exercised, then capped)"));

	// --- 4) Determinism: same (NumRuns, Seed) -> identical aggregate. Compare exact integer fields
	//        (chips/spins/wins are integer; per-round clears are arrays) — no float-equality fragility.
	const FCarouselSimReport A = FCarouselSim::RunSim(2000, 7);
	const FCarouselSimReport B = FCarouselSim::RunSim(2000, 7);
	const bool bDeterministic = (A.TotalChips == B.TotalChips) && (A.TotalSpins == B.TotalSpins)
		&& (A.RunWins == B.RunWins) && (A.ClearedRound == B.ClearedRound) && (A.BustHist == B.BustHist);
	R.Check(bDeterministic, TEXT("RunSim is deterministic for a fixed seed (identical totals + per-round clears)"));

	// --- 5) The known build clears the known seed (round-1 clears for a non-trivial slice of runs).
	R.Check(A.ClearedRound.IsValidIndex(0) && A.ClearedRound[0] > 0,
		FString::Printf(TEXT("known build clears round 1 at seed 7 (%d/%d runs)"),
			A.ClearedRound.IsValidIndex(0) ? A.ClearedRound[0] : 0, A.NumRuns));

	// --- 6) TUNED ACCEPTANCE BAND (seed 7) — the headline guard. Fails loudly if the baseline drifts.
	R.Check(A.HitFreqPct >= 45.0 && A.HitFreqPct <= 55.0,
		FString::Printf(TEXT("hit frequency in [45, 55]%% (got %.1f%%)"), A.HitFreqPct));
	R.Check(A.Round1ClearPct >= 50.0 && A.Round1ClearPct <= 60.0,
		FString::Printf(TEXT("round-1 clear in [50, 60]%% (got %.1f%%)"), A.Round1ClearPct));

	UE_LOG(LogCarouselSmoke, Display, TEXT("  seed 7 (2000 runs): hit=%.1f%%  round1=%.1f%%  EV=%.2f/spin"),
		A.HitFreqPct, A.Round1ClearPct, A.EvPerSpin);

	const int32 ExitCode = R.Failures;
	if (R.Failures == 0)
	{
		UE_LOG(LogCarouselSmoke, Display, TEXT("=== CAROUSEL SMOKE TEST PASSED (SIB-46 — pipeline + determinism + tuned band green). ==="));
	}
	else
	{
		UE_LOG(LogCarouselSmoke, Error, TEXT("=== CAROUSEL SMOKE TEST FAILED: %d assertion(s). ==="), R.Failures);
	}

	// Commandlets self-exit via the return code; this guards a headless run from hanging (the
	// -ExecCmds lesson) and is a no-op for an interactive invocation.
	if (FApp::IsUnattended())
	{
		RequestEngineExit(TEXT("CarouselSmokeTest complete"));
	}
	return ExitCode;
}
