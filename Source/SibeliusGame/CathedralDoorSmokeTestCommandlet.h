// CathedralDoorSmokeTestCommandlet.h
//
// SIB-31 — headless smoke test for the attic->cathedral door (Ch7 entry).
// Editor-only; implementation guarded with WITH_EDITOR. Mirrors the sibling
// per-feature smoke commandlets.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CathedralDoorSmokeTestCommandlet.generated.h"

UCLASS()
class UCathedralDoorSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCathedralDoorSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
