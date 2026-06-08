// GenerateSmokeTestCommandlet.h
//
// SIB-30 — Ch6 "Generate" predict-bugs SPIKE. Headless gate for the G1-G6 ledger
// (mirrors BranchSmokeTest). Confirms the riskiest design assumptions BEFORE the
// chapter is built: closed-catalog scope, varied-phrasing matching, content safety,
// determinism, budget economy, and persistence through the real Ch3/Ch5 pipeline.
//
// Run (Build.bat the Editor target FIRST, then):
//   "<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
//     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" ^
//     -run=GenerateSmokeTest -unattended -nopause -nosplash -stdout

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateSmokeTestCommandlet.generated.h"

UCLASS()
class UGenerateSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGenerateSmokeTestCommandlet();
	virtual int32 Main(const FString& Params) override;
};
