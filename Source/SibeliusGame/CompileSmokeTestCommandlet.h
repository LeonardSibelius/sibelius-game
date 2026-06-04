// Ch3 - Compile (SIB-27). Headless gate: -run=CompileSmokeTest
// Mirrors the SIB-19 / Refuser / CodeVision / Refactor commandlets.
// Helpers live in a NAMED namespace (CompileSmokeTestNS) - CP3 unity-build lesson.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Commandlets/Commandlet.h"
#include "CompileSmokeTestCommandlet.generated.h"

UCLASS()
class UCompileSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCompileSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};

#endif // WITH_EDITOR
