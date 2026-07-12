// FinaleSmokeTestCommandlet.h
//
// FUN-6 — headless gate for the Ch7 Synthesis stage machine. Asserts the pure
// FFinaleSequence invariants: starts at Code Vision, only the exact expected
// verb advances, the six-verb order completes, and a completed rite ignores
// further input. Pure model — no world. Run editor-closed: -run=FinaleSmokeTest.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "FinaleSmokeTestCommandlet.generated.h"

UCLASS()
class UFinaleSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UFinaleSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
