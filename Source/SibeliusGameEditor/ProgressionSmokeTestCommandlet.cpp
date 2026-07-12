// ProgressionSmokeTestCommandlet.cpp — FUN-1/FUN-2 headless gate. See header.
//
// NAMED namespace + function-scoped `using` (unity-build safe), no variable
// shadowing (warnings-as-errors). Editor-closed.

#include "ProgressionSmokeTestCommandlet.h"
#include "ProgressionTypes.h"
#include "ProgressionSaveGame.h"
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

	// --- 3) Save round-trip on the sandbox slot: write -> load -> compare -> delete.
	{
		FSibeliusSaveIO::Delete(SandboxSlot); // pre-clean a crashed prior run

		UProgressionSaveGame* Written = NewObject<UProgressionSaveGame>(GetTransientPackage());
		Written->SaveVersion = UProgressionSaveGame::CurrentSaveVersion;
		Written->State.Unlock(EPowerVerb::Refactor);
		Written->State.Unlock(EPowerVerb::Generate);
		Written->State.AddSauce(123);
		Written->State.Claim(TEXT("Smoke.GrantA"));
		Written->State.Claim(TEXT("Smoke.GrantB"));

		R.Check(FSibeliusSaveIO::Commit(Written, SandboxSlot), TEXT("commit to sandbox slot"));

		UProgressionSaveGame* Loaded = Cast<UProgressionSaveGame>(FSibeliusSaveIO::Load(SandboxSlot));
		R.Check(Loaded != nullptr, TEXT("load from sandbox slot"));
		if (Loaded)
		{
			R.Check(Loaded->SaveVersion == UProgressionSaveGame::CurrentSaveVersion, TEXT("round-trip: SaveVersion stamped"));
			R.Check(Loaded->State.Sauce == 123, TEXT("round-trip: sauce survives"));
			R.Check(Loaded->State.IsUnlocked(EPowerVerb::CodeVision), TEXT("round-trip: starter power survives"));
			R.Check(Loaded->State.IsUnlocked(EPowerVerb::Refactor), TEXT("round-trip: unlocked Refactor survives"));
			R.Check(Loaded->State.IsUnlocked(EPowerVerb::Generate), TEXT("round-trip: unlocked Generate survives"));
			R.Check(!Loaded->State.IsUnlocked(EPowerVerb::Deploy), TEXT("round-trip: locked Deploy stays locked"));
			R.Check(Loaded->State.HasClaimed(TEXT("Smoke.GrantA")), TEXT("round-trip: claimed grant survives"));
			R.Check(!Loaded->State.Claim(TEXT("Smoke.GrantB")), TEXT("round-trip: re-claim still refused"));
		}

		R.Check(FSibeliusSaveIO::Delete(SandboxSlot), TEXT("sandbox slot cleanup"));
	}

	UE_LOG(LogProgressionSmoke, Display, TEXT("=== Progression smoke test: %d failure(s) ==="), R.Failures);
	return R.Failures == 0 ? 0 : 1;
}
