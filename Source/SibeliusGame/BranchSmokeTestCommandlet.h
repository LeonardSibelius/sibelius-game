// BranchSmokeTestCommandlet.h
//
// SIB-28 - Headless smoke test for Ch4 Test-Drive. Grows one assert block per
// phase; Phase 0 covers the seam (subsystem exists, FSM transitions, EnterBranch
// captures the declared set). Clean exit (0) = green. Mirrors the other
// per-chapter smoke commandlets. Editor-only (uses UnrealEd map loading).
//
// Run:
//   "<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
//     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" ^
//     -run=BranchSmokeTest -unattended -nopause -nosplash -stdout

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BranchSmokeTestCommandlet.generated.h"

UCLASS()
class UBranchSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBranchSmokeTestCommandlet();
	virtual int32 Main(const FString& Params) override;
};
