// RefactorSmokeTestCommandlet.h
//
// SIB-26 — Headless smoke test for Ch2 Refactor (L_Office_v02).
// Clean exit (0) = green. Mirrors USmokeTestCommandlet / CodeVisionSmokeTest.
// Editor-only (uses UnrealEd map-loading). Build in editor targets only.
//
// Run:
//   "<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
//     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" ^
//     -run=RefactorSmokeTest -unattended -nopause -nosplash -stdout

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RefactorSmokeTestCommandlet.generated.h"

UCLASS()
class URefactorSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URefactorSmokeTestCommandlet();
	virtual int32 Main(const FString& Params) override;
};
