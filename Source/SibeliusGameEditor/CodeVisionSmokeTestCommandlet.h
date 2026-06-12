// CodeVisionSmokeTestCommandlet.h
//
// SIB-25 — Headless smoke test for Ch1 Code Vision (L_CodeVisionTest).
// Clean exit (0) = green. Mirrors USmokeTestCommandlet (SIB-19).
//
// Editor-only: uses UnrealEd map-loading utilities. Build in editor targets only.
//
// Run:
//   "<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
//     "C:\Users\wpark\projects\sibelius-game\SibeliusGame.uproject" ^
//     -run=CodeVisionSmokeTest -unattended -nopause -nosplash -stdout

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CodeVisionSmokeTestCommandlet.generated.h"

UCLASS()
class UCodeVisionSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCodeVisionSmokeTestCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};
