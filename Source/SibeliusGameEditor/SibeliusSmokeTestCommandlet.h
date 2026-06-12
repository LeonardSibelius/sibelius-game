// SibeliusSmokeTestCommandlet.h
//
// SIB-19 - Headless smoke test for L_Office_v02 (v0.2 CP1).
// Renamed from SmokeTest* to avoid colliding with the engine's built-in
// USmokeTestCommandlet. Editor-only; implementation guarded with WITH_EDITOR.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SibeliusSmokeTestCommandlet.generated.h"

UCLASS()
class USibeliusSmokeTestCommandlet : public UCommandlet
{
GENERATED_BODY()

public:
USibeliusSmokeTestCommandlet();

virtual int32 Main(const FString& Params) override;
};
