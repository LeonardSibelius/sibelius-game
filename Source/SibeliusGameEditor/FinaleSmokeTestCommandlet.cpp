// FinaleSmokeTestCommandlet.cpp — FUN-6 headless gate. See header.

#include "FinaleSmokeTestCommandlet.h"
#include "FinaleAltar.h"
#include "ProgressionTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogFinaleSmoke, Log, All);

namespace FinaleSmokeTestNS
{
	struct FResult
	{
		int32 Failures = 0;
		void Check(bool bCondition, const FString& Label)
		{
			if (bCondition)
			{
				UE_LOG(LogFinaleSmoke, Display, TEXT("  [PASS] %s"), *Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogFinaleSmoke, Error, TEXT("  [FAIL] %s"), *Label);
			}
		}
	};
}

UFinaleSmokeTestCommandlet::UFinaleSmokeTestCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UFinaleSmokeTestCommandlet::Main(const FString& Params)
{
	using namespace FinaleSmokeTestNS;

	UE_LOG(LogFinaleSmoke, Display, TEXT("=== FUN-6 Finale (Synthesis) smoke test ==="));

	FResult R;

	FFinaleSequence Seq;

	// The rite opens on Code Vision.
	R.Check(!Seq.IsComplete(), TEXT("fresh sequence is not complete"));
	R.Check(Seq.CurrentVerb() == EPowerVerb::CodeVision, TEXT("stage 1 asks for Code Vision"));

	// A wrong verb never advances.
	R.Check(!Seq.Submit(EPowerVerb::Deploy), TEXT("wrong verb is refused"));
	R.Check(Seq.StageIndex == 0, TEXT("refusal leaves the stage unchanged"));

	// The six verbs in enum (chapter) order complete the rite exactly.
	for (uint8 i = 0; i < static_cast<uint8>(EPowerVerb::Count); ++i)
	{
		const EPowerVerb Verb = static_cast<EPowerVerb>(i);
		R.Check(Seq.Submit(Verb), FString::Printf(TEXT("stage %d accepts %s"), i + 1, *PowerVerbDisplayName(Verb)));
	}
	R.Check(Seq.IsComplete(), TEXT("six correct verbs complete the rite"));

	// A completed rite ignores everything.
	R.Check(!Seq.Submit(EPowerVerb::CodeVision), TEXT("a complete rite ignores further verbs"));

	// A repeat of an EARLIER verb mid-rite is refused (order, not a checklist).
	FFinaleSequence Seq2;
	Seq2.Submit(EPowerVerb::CodeVision);
	Seq2.Submit(EPowerVerb::Refactor);
	R.Check(!Seq2.Submit(EPowerVerb::CodeVision), TEXT("an already-shown verb does not advance"));
	R.Check(Seq2.CurrentVerb() == EPowerVerb::Compile, TEXT("mid-rite stage still asks for Compile"));

	/* --- THE CLOSING SEQUENCE'S PAYLOAD (docs/SPINE.md Move 3) ---
	   The finale reads Walt's messages back in order. If that list is short, mis-ordered,
	   or carries an empty entry, the ending plays a blank beat and nothing says so. */
	{
		const TArray<FString>& Messages = AllMemoirMessages();
		R.Check(Messages.Num() == 8,
			FString::Printf(TEXT("SPINE: the closing sequence has all EIGHT messages (%d)"), Messages.Num()));

		bool bAllPresent = true;
		for (const FString& M : Messages)
		{
			if (M.IsEmpty()) { bAllPresent = false; }
		}
		R.Check(bAllPresent, TEXT("SPINE: no message in the closing sequence is empty"));

		/* The two that belong to no power, and therefore had no home in the code until the
		   finale needed them. Bally is the one the whole game points at. */
		bool bHasBally = false, bHasSanDiego = false;
		for (const FString& M : Messages)
		{
			if (M.Contains(TEXT("Bally")))          { bHasBally = true; }
			if (M.Contains(TEXT("San Diego")))      { bHasSanDiego = true; }
		}
		R.Check(bHasBally, TEXT("SPINE: the Bally message is in the closing sequence"));
		R.Check(bHasSanDiego, TEXT("SPINE: the San Diego County message is in the closing sequence"));

		// Chronological: the career is being read back, so the last one must be the last.
		R.Check(Messages[0].Contains(TEXT("1988")), TEXT("SPINE: the sequence opens on 1988"));
		R.Check(Messages.Last().Contains(TEXT("2022")), TEXT("SPINE: the sequence closes on 2022"));
	}

	UE_LOG(LogFinaleSmoke, Display, TEXT("=== Finale smoke test: %d failure(s) ==="), R.Failures);
	return R.Failures == 0 ? 0 : 1;
}
