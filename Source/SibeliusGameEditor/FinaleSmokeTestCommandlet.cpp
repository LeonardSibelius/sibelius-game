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

	UE_LOG(LogFinaleSmoke, Display, TEXT("=== Finale smoke test: %d failure(s) ==="), R.Failures);
	return R.Failures == 0 ? 0 : 1;
}
