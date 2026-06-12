// RefuserSmokeTestCommandlet.h
//
// CP3 - Headless smoke test for L_RefuserTest (Refuser spawner + chase wiring).
// Modeled on USibeliusSmokeTestCommandlet (SIB-19). Editor-only; implementation
// guarded with WITH_EDITOR.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RefuserSmokeTestCommandlet.generated.h"

UCLASS()
class URefuserSmokeTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URefuserSmokeTestCommandlet();

	virtual int32 Main(const FString& Params) override;
};
