// SauceSmokeTestCommandlet.h
//
// World Three P0 — headless smoke test for ASauceCauldron + ABookRain. Modeled on
// the sibling gates (URefuserSmokeTestCommandlet / UGenerateSmokeTestCommandlet).
// Editor-only module (PK12: commandlets never in the runtime module). Asserts
// STATE/LOGIC only — the actual rain + feed visuals are a PIE check (SS8).

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SauceSmokeTestCommandlet.generated.h"

// Headless listener: counts OnSauceComplete broadcasts so the gate can prove the
// completion delegate fires EXACTLY once (SS9 one-shot latch). A dynamic multicast
// delegate only binds UFUNCTIONs, so the counter must be a UObject.
UCLASS()
class USauceCompleteCounter : public UObject
{
	GENERATED_BODY()

public:
	int32 Count = 0;

	UFUNCTION()
	void HandleSauceComplete() { ++Count; }
};

UCLASS()
class USauceSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USauceSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
